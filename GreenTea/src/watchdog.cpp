#include "watchdog.h"
#include <tea/common.h>
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

Watchdog::Watchdog() {}

Watchdog::~Watchdog() {
    stop();
}

bool Watchdog::loadConfig(const tea::Config& config) {
    check_interval_sec_ = config.get<int>("check_interval_sec", 3);

    auto procs = config.raw().value("processes", tea::json::array());
    for (const auto& p : procs) {
        WatchedProcess wp;
        wp.name = p.value("name", "");
        wp.binary = p.value("binary", "");
        wp.args = p.value("args", std::vector<std::string>{});
        wp.working_dir = p.value("working_dir", ".");
        wp.max_restarts = p.value("max_restarts", 10);
        wp.restart_delay_sec = p.value("restart_delay_sec", 2);
        wp.auto_start = p.value("auto_start", true);

        if (wp.name.empty() || wp.binary.empty()) {
            spdlog::warn("Skipping process with empty name or binary");
            continue;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        processes_[wp.name] = wp;
        spdlog::info("Watchdog: registered process '{}' ({})", wp.name, wp.binary);
    }
    return true;
}

void Watchdog::start() {
    // Auto-start processes
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, proc] : processes_) {
            if (proc.auto_start) {
                spdlog::info("Auto-starting process: {}", name);
                launchProcess(proc);
            }
        }
    }

    running_.store(true);
    monitor_thread_ = std::thread(&Watchdog::monitorLoop, this);
    spdlog::info("Watchdog started, check interval: {}s", check_interval_sec_);
}

void Watchdog::stop() {
    running_.store(false);
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    // Stop all processes
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, proc] : processes_) {
        if (proc.running) {
            spdlog::info("Watchdog: stopping process '{}'", name);
            killProcess(proc);
        }
    }
}

bool Watchdog::startProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end()) return false;
    if (it->second.running) return true;
    return launchProcess(it->second);
}

bool Watchdog::stopProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end()) return false;
    if (!it->second.running) return true;
    return killProcess(it->second);
}

bool Watchdog::restartProcess(const std::string& name) {
    stopProcess(name);
    return startProcess(name);
}

bool Watchdog::updateBinary(const std::string& name, const std::string& new_binary_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end()) {
        spdlog::error("Watchdog: process '{}' not found for update", name);
        return false;
    }

    auto& proc = it->second;
    std::string old_binary = proc.binary;
    std::string backup_path = old_binary + ".bak";

    // Stop the process first
    if (proc.running) {
        killProcess(proc);
    }

    try {
        // Backup old binary
        if (fs::exists(old_binary)) {
            fs::copy_file(old_binary, backup_path, fs::copy_options::overwrite_existing);
        }

        // Copy new binary
        fs::copy_file(new_binary_path, old_binary, fs::copy_options::overwrite_existing);

        // Make executable
        fs::permissions(old_binary, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                       fs::perm_options::add);

        spdlog::info("Watchdog: binary updated for '{}': {} -> {}", name, new_binary_path, old_binary);

        // Restart
        proc.restart_count = 0;
        launchProcess(proc);

        // Clean up temp file
        if (fs::exists(new_binary_path) && new_binary_path != old_binary) {
            fs::remove(new_binary_path);
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Watchdog: binary update failed for '{}': {}", name, e.what());
        // Restore backup
        if (fs::exists(backup_path)) {
            try { fs::copy_file(backup_path, old_binary, fs::copy_options::overwrite_existing); }
            catch (...) {}
        }
        return false;
    }
}

std::vector<Watchdog::WatchedProcess> Watchdog::getProcesses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WatchedProcess> result;
    for (const auto& [name, proc] : processes_) {
        result.push_back(proc);
    }
    return result;
}

Watchdog::WatchedProcess Watchdog::getProcess(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end()) return {};
    return it->second;
}

void Watchdog::monitorLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(check_interval_sec_));

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, proc] : processes_) {
            if (!proc.running || proc.pid <= 0) continue;

            int status;
            pid_t result = waitpid(proc.pid, &status, WNOHANG);
            if (result == proc.pid) {
                proc.running = false;
                proc.last_exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                proc.last_stop_time = tea::nowMs();

                spdlog::warn("Process '{}' (pid={}) exited with status {}",
                            name, proc.pid, proc.last_exit_code);
                proc.pid = 0;

                // Auto-restart if under limit
                if (proc.auto_start && proc.restart_count < proc.max_restarts) {
                    spdlog::info("Auto-restarting '{}' (attempt {}/{})",
                                name, proc.restart_count + 1, proc.max_restarts);
                    std::this_thread::sleep_for(std::chrono::seconds(proc.restart_delay_sec));
                    launchProcess(proc);
                    proc.restart_count++;
                } else if (proc.restart_count >= proc.max_restarts) {
                    spdlog::error("Process '{}' exceeded max restarts ({}), giving up",
                                 name, proc.max_restarts);
                }
            }
        }
    }
}

bool Watchdog::launchProcess(WatchedProcess& proc) {
    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("Fork failed for '{}': {}", proc.name, strerror(errno));
        return false;
    }

    if (pid == 0) {
        // Child process
        if (!proc.working_dir.empty()) {
            if (chdir(proc.working_dir.c_str()) != 0) {
                // ignore
            }
        }

        // Create new session to detach from terminal
        setsid();

        std::vector<const char*> argv;
        argv.push_back(proc.binary.c_str());
        for (const auto& arg : proc.args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        execvp(proc.binary.c_str(), const_cast<char**>(argv.data()));
        _exit(127);
    }

    proc.pid = pid;
    proc.running = true;
    proc.last_start_time = tea::nowMs();
    spdlog::info("Launched process '{}' with pid={}", proc.name, pid);
    return true;
}

bool Watchdog::killProcess(WatchedProcess& proc, int timeout_ms) {
    if (proc.pid <= 0) return true;

    ::kill(proc.pid, SIGTERM);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    int status;
    while (std::chrono::steady_clock::now() < deadline) {
        if (waitpid(proc.pid, &status, WNOHANG) == proc.pid) {
            proc.running = false;
            proc.pid = 0;
            proc.last_stop_time = tea::nowMs();
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ::kill(proc.pid, SIGKILL);
    waitpid(proc.pid, &status, 0);
    proc.running = false;
    proc.pid = 0;
    proc.last_stop_time = tea::nowMs();
    return true;
}
