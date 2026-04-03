#include "watchdog.h"
#include <tea/common.h>
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

bool isLikelyRelativePathArg(const std::string& arg) {
    if (arg.empty()) {
        return false;
    }

    const fs::path p(arg);
    if (p.is_absolute()) {
        return false;
    }

    if (arg.rfind("./", 0) == 0 || arg.rfind("../", 0) == 0) {
        return true;
    }

    return arg.find('/') != std::string::npos || arg.find('\\') != std::string::npos;
}

std::string joinArgs(const std::vector<std::string>& args) {
    if (args.empty()) {
        return "<none>";
    }

    std::string out;
    for (size_t i = 0; i < args.size(); ++i) {
        out += args[i];
        if (i + 1 < args.size()) {
            out += ' ';
        }
    }
    return out;
}

} // namespace

Watchdog::Watchdog() {}

Watchdog::~Watchdog() {
    stop();
}

bool Watchdog::loadConfig(const tea::Config& config, const std::string& config_base_dir) {
    check_interval_sec_ = config.get<int>("check_interval_sec", 3);

    fs::path base_dir = config_base_dir.empty() ? fs::current_path() : fs::path(config_base_dir);
    if (base_dir.is_relative()) {
        base_dir = fs::absolute(base_dir);
    }
    base_dir = base_dir.lexically_normal();

    auto resolvePath = [&base_dir](const std::string& raw) -> std::string {
        if (raw.empty()) {
            return raw;
        }

        fs::path p(raw);
        if (p.is_absolute()) {
            return p.lexically_normal().string();
        }
        return (base_dir / p).lexically_normal().string();
    };

    auto normalizeConfigArg = [&base_dir](const std::string& original_arg,
                                          const std::string& resolved_arg) -> std::string {
        if (resolved_arg.empty() || fs::exists(resolved_arg)) {
            return resolved_arg;
        }

        fs::path arg_path(original_arg);
        if (arg_path.filename() != "config.json") {
            return resolved_arg;
        }

        const std::string owner = arg_path.parent_path().filename().string();
        if (owner != "GreenTea" && owner != "LemonTea" && owner != "HoneyTea") {
            return resolved_arg;
        }

        fs::path candidate = base_dir / (owner + ".json");
        if (fs::exists(candidate)) {
            return candidate.lexically_normal().string();
        }
        return resolved_arg;
    };

    auto procs = config.raw().value("processes", tea::json::array());
    int start_order = 0;

    std::map<std::string, WatchedProcess> new_processes;
    for (const auto& p : procs) {
        WatchedProcess wp;
        wp.name = p.value("name", "");
        wp.binary = resolvePath(p.value("binary", ""));
        wp.args = p.value("args", std::vector<std::string>{});
        wp.working_dir = resolvePath(p.value("working_dir", "."));
        wp.startup_delay_sec = p.value("startup_delay_sec", 0);
        wp.max_restarts = p.value("max_restarts", 10);
        wp.restart_delay_sec = p.value("restart_delay_sec", 2);
        wp.auto_start = p.value("auto_start", true);
        wp.start_order = start_order++;

        for (size_t i = 0; i < wp.args.size(); ++i) {
            const bool is_config_value =
                i > 0 && (wp.args[i - 1] == "--config" || wp.args[i - 1] == "-c" ||
                          wp.args[i - 1] == "--config-file");
            const bool should_resolve = is_config_value || isLikelyRelativePathArg(wp.args[i]);
            if (!should_resolve) {
                continue;
            }

            std::string resolved = resolvePath(wp.args[i]);
            if (is_config_value) {
                resolved = normalizeConfigArg(wp.args[i], resolved);
            }
            wp.args[i] = resolved;
        }

        if (wp.name.empty() || wp.binary.empty()) {
            spdlog::warn("Skipping process with empty name or binary");
            continue;
        }

        if (!fs::exists(wp.binary)) {
            spdlog::warn("Watchdog config: binary for '{}' does not exist yet: {}", wp.name, wp.binary);
        }

        if (!fs::exists(wp.working_dir)) {
            spdlog::warn("Watchdog config: working_dir for '{}' does not exist yet: {}", wp.name, wp.working_dir);
        }

        new_processes[wp.name] = wp;
        spdlog::info("Watchdog: registered process '{}' binary='{}' work_dir='{}' args='{}'",
                     wp.name, wp.binary, wp.working_dir, joinArgs(wp.args));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        processes_ = std::move(new_processes);
        output_line_buffers_.clear();
    }

    return true;
}

void Watchdog::start() {
    if (running_.exchange(true)) {
        spdlog::warn("Watchdog start requested while already running");
        return;
    }

    output_thread_ = std::thread(&Watchdog::outputLoop, this);

    std::vector<std::pair<int, std::string>> auto_starts;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto_starts.reserve(processes_.size());
        for (const auto& [name, proc] : processes_) {
            if (proc.auto_start) {
                auto_starts.emplace_back(proc.start_order, name);
            }
        }
    }
    std::sort(auto_starts.begin(), auto_starts.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& item : auto_starts) {
        int startup_delay_sec = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = processes_.find(item.second);
            if (it == processes_.end()) {
                continue;
            }
            auto& proc = it->second;
            if (!proc.running) {
                spdlog::info("Auto-starting process: {}", proc.name);
                launchProcess(proc);
            }
            startup_delay_sec = std::max(0, proc.startup_delay_sec);
        }
        if (startup_delay_sec > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(startup_delay_sec));
        }
    }

    monitor_thread_ = std::thread(&Watchdog::monitorLoop, this);
    spdlog::info("Watchdog started, check interval: {}s", check_interval_sec_);
}

void Watchdog::stop() {
    const bool was_running = running_.exchange(false);
    if (!was_running && !monitor_thread_.joinable() && !output_thread_.joinable()) {
        return;
    }

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    // Stop all processes
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, proc] : processes_) {
            if (proc.running) {
                spdlog::info("Watchdog: stopping process '{}'", name);
                killProcess(proc);
            }
            closeParentGuardFd(proc);
            closeOutputFd(proc);
        }
    }

    if (output_thread_.joinable()) {
        output_thread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    output_line_buffers_.clear();
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
                closeParentGuardFd(proc);

                if (proc.last_exit_code == 127) {
                    spdlog::warn("Process '{}' (pid={}) exited with status 127. binary='{}' work_dir='{}' args='{}'. "
                                 "This often means executable path/runtime loader/config path is invalid.",
                                 name, proc.pid, proc.binary, proc.working_dir, joinArgs(proc.args));
                } else {
                    spdlog::warn("Process '{}' (pid={}) exited with status {}",
                                 name, proc.pid, proc.last_exit_code);
                }
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
    closeParentGuardFd(proc);
    closeOutputFd(proc);

    int guard_pipe[2] = {-1, -1};
    const bool guard_ready = (::pipe(guard_pipe) == 0);
    if (!guard_ready) {
        spdlog::warn("Watchdog: failed to create parent guard pipe for '{}': {}",
                     proc.name, strerror(errno));
    }

    int output_pipe[2] = {-1, -1};
    const bool output_ready = (::pipe(output_pipe) == 0);
    if (!output_ready) {
        spdlog::warn("Watchdog: failed to create output capture pipe for '{}': {}",
                     proc.name, strerror(errno));
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (guard_ready) {
            ::close(guard_pipe[0]);
            ::close(guard_pipe[1]);
        }
        if (output_ready) {
            ::close(output_pipe[0]);
            ::close(output_pipe[1]);
        }
        spdlog::error("Fork failed for '{}': {}", proc.name, strerror(errno));
        return false;
    }

    if (pid == 0) {
        if (guard_ready) {
            ::close(guard_pipe[1]);
            const std::string guard_fd = std::to_string(guard_pipe[0]);
            setenv("TEA_PARENT_GUARD_FD", guard_fd.c_str(), 1);
        } else {
            unsetenv("TEA_PARENT_GUARD_FD");
        }

        if (output_ready) {
            ::close(output_pipe[0]);
            ::dup2(output_pipe[1], STDOUT_FILENO);
            ::dup2(output_pipe[1], STDERR_FILENO);
            ::close(output_pipe[1]);
        }

        // Child process
        if (!proc.working_dir.empty()) {
            if (chdir(proc.working_dir.c_str()) != 0) {
                std::fprintf(stderr,
                             "Watchdog child '%s': failed to chdir to '%s': %s\n",
                             proc.name.c_str(), proc.working_dir.c_str(), std::strerror(errno));
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
        std::fprintf(stderr,
                     "Watchdog child '%s': execvp failed for '%s' (cwd='%s'): %s\n",
                     proc.name.c_str(), proc.binary.c_str(), proc.working_dir.c_str(),
                     std::strerror(errno));
        _exit(127);
    }

    if (guard_ready) {
        ::close(guard_pipe[0]);
        int flags = fcntl(guard_pipe[1], F_GETFD);
        if (flags >= 0) {
            fcntl(guard_pipe[1], F_SETFD, flags | FD_CLOEXEC);
        }
        proc.parent_guard_fd = guard_pipe[1];
    } else {
        proc.parent_guard_fd = -1;
    }

    if (output_ready) {
        ::close(output_pipe[1]);

        int status_flags = fcntl(output_pipe[0], F_GETFL, 0);
        if (status_flags >= 0) {
            fcntl(output_pipe[0], F_SETFL, status_flags | O_NONBLOCK);
        }

        int fd_flags = fcntl(output_pipe[0], F_GETFD);
        if (fd_flags >= 0) {
            fcntl(output_pipe[0], F_SETFD, fd_flags | FD_CLOEXEC);
        }

        proc.output_fd = output_pipe[0];
    } else {
        proc.output_fd = -1;
    }

    proc.pid = pid;
    proc.running = true;
    proc.last_start_time = tea::nowMs();
    spdlog::info("Launched process '{}' with pid={} binary='{}' work_dir='{}' args='{}'",
                 proc.name, pid, proc.binary, proc.working_dir, joinArgs(proc.args));
    return true;
}

bool Watchdog::killProcess(WatchedProcess& proc, int timeout_ms) {
    closeParentGuardFd(proc);
    if (proc.pid <= 0) {
        closeOutputFd(proc);
        return true;
    }

    ::kill(proc.pid, SIGTERM);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    int status;
    while (std::chrono::steady_clock::now() < deadline) {
        if (waitpid(proc.pid, &status, WNOHANG) == proc.pid) {
            proc.running = false;
            proc.pid = 0;
            proc.last_stop_time = tea::nowMs();
            closeOutputFd(proc);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ::kill(proc.pid, SIGKILL);
    waitpid(proc.pid, &status, 0);
    proc.running = false;
    proc.pid = 0;
    proc.last_stop_time = tea::nowMs();
    closeOutputFd(proc);
    return true;
}

void Watchdog::closeParentGuardFd(WatchedProcess& proc) {
    if (proc.parent_guard_fd >= 0) {
        ::close(proc.parent_guard_fd);
        proc.parent_guard_fd = -1;
    }
}

void Watchdog::closeOutputFd(WatchedProcess& proc) {
    if (proc.output_fd >= 0) {
        ::close(proc.output_fd);
        proc.output_fd = -1;
    }
}

void Watchdog::outputLoop() {
    constexpr int kPollTimeoutMs = 300;
    constexpr int kReadSize = 4096;

    while (true) {
        std::vector<struct pollfd> poll_fds;
        std::vector<std::string> names;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [name, proc] : processes_) {
                if (proc.output_fd < 0) {
                    continue;
                }

                struct pollfd pfd;
                pfd.fd = proc.output_fd;
                pfd.events = POLLIN | POLLHUP | POLLERR;
                pfd.revents = 0;
                poll_fds.push_back(pfd);
                names.push_back(name);
            }

            if (!running_.load() && poll_fds.empty()) {
                break;
            }
        }

        if (poll_fds.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            continue;
        }

        const int ready = ::poll(poll_fds.data(), poll_fds.size(), kPollTimeoutMs);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            spdlog::warn("Watchdog output poll failed: {}", std::strerror(errno));
            continue;
        }

        if (ready == 0) {
            continue;
        }

        for (size_t i = 0; i < poll_fds.size(); ++i) {
            const auto revents = poll_fds[i].revents;
            if ((revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
                continue;
            }

            const std::string proc_name = names[i];
            const int fd = poll_fds[i].fd;
            bool close_requested = false;

            while (true) {
                char buf[kReadSize + 1];
                const ssize_t n = ::read(fd, buf, kReadSize);
                if (n > 0) {
                    buf[n] = '\0';

                    std::vector<std::string> lines;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        std::string& cache = output_line_buffers_[proc_name];
                        cache.append(buf, static_cast<size_t>(n));

                        size_t pos = std::string::npos;
                        while ((pos = cache.find('\n')) != std::string::npos) {
                            std::string line = cache.substr(0, pos);
                            cache.erase(0, pos + 1);
                            if (!line.empty()) {
                                lines.push_back(std::move(line));
                            }
                        }
                    }

                    for (const auto& line : lines) {
                        spdlog::info("[captured:{}] {}", proc_name, line);
                    }
                    continue;
                }

                if (n == 0) {
                    close_requested = true;
                    break;
                }

                if (errno == EINTR) {
                    continue;
                }

                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }

                close_requested = true;
                break;
            }

            if (!close_requested) {
                continue;
            }

            std::string trailing;
            bool should_close = false;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = processes_.find(proc_name);
                if (it != processes_.end() && it->second.output_fd == fd) {
                    should_close = true;
                    it->second.output_fd = -1;
                }

                auto cache_it = output_line_buffers_.find(proc_name);
                if (cache_it != output_line_buffers_.end()) {
                    trailing = cache_it->second;
                    output_line_buffers_.erase(cache_it);
                }
            }

            if (should_close) {
                ::close(fd);
            }

            if (!trailing.empty()) {
                spdlog::info("[captured:{}] {}", proc_name, trailing);
            }
        }
    }
}
