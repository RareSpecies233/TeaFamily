#include "tea/process_manager.h"
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

namespace tea {

ProcessManager::ProcessManager() {}

ProcessManager::~ProcessManager() {
    stopMonitor();
    stopAll();
}

bool ProcessManager::registerProcess(const std::string& name, const std::string& binary,
                                      const std::vector<std::string>& args,
                                      const std::string& work_dir) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (processes_.count(name)) {
        spdlog::warn("Process {} already registered", name);
        return false;
    }
    ProcessInfo info;
    info.name = name;
    info.binary_path = binary;
    info.args = args;
    info.working_dir = work_dir;
    processes_[name] = info;
    spdlog::info("Process registered: {} ({})", name, binary);
    return true;
}

bool ProcessManager::unregisterProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end()) return false;
    if (it->second.state == PluginState::RUNNING) {
        spdlog::warn("Cannot unregister running process: {}", name);
        return false;
    }
    processes_.erase(it);
    spdlog::info("Process unregistered: {}", name);
    return true;
}

bool ProcessManager::startProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end()) {
        spdlog::error("Process not found: {}", name);
        return false;
    }

    auto& info = it->second;
    if (info.state == PluginState::RUNNING) {
        spdlog::warn("Process {} is already running", name);
        return true;
    }

    // Create pipes for stdin/stdout
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        spdlog::error("Failed to create pipes for {}: {}", name, strerror(errno));
        return false;
    }

    PluginState old_state = info.state;
    info.state = PluginState::STARTING;

    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("Fork failed for {}: {}", name, strerror(errno));
        info.state = old_state;
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        return false;
    }

    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);  // Close write end of stdin
        close(stdout_pipe[0]); // Close read end of stdout

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        if (!info.working_dir.empty()) {
            if (chdir(info.working_dir.c_str()) != 0) {
                // ignore chdir failure, will use current dir
            }
        }

        // Build argv
        std::vector<const char*> argv;
        argv.push_back(info.binary_path.c_str());
        for (const auto& arg : info.args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        execvp(info.binary_path.c_str(), const_cast<char**>(argv.data()));
        // If exec fails
        _exit(127);
    }

    // Parent
    close(stdin_pipe[0]);   // Close read end of stdin
    close(stdout_pipe[1]);  // Close write end of stdout

    // Set stdout pipe to non-blocking
    int flags = fcntl(stdout_pipe[0], F_GETFL, 0);
    fcntl(stdout_pipe[0], F_SETFL, flags | O_NONBLOCK);

    info.pid = pid;
    info.stdin_fd = stdin_pipe[1];
    info.stdout_fd = stdout_pipe[0];
    info.state = PluginState::RUNNING;
    info.started_at = nowMs();
    info.exit_code = 0;

    spdlog::info("Process started: {} (pid={})", name, pid);

    if (on_state_change_) {
        on_state_change_(name, old_state, PluginState::RUNNING);
    }

    // Start output reader thread
    if (on_output_) {
        std::string proc_name = name;
        int fd = stdout_pipe[0];
        output_threads_[name] = std::thread([this, proc_name, fd]() {
            readOutputLoop(proc_name, fd);
        });
    }

    return true;
}

bool ProcessManager::stopProcess(const std::string& name, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end()) return false;

    auto& info = it->second;
    if (info.state != PluginState::RUNNING) return true;

    pid_t pid = info.pid;
    PluginState old_state = info.state;
    info.state = PluginState::STOPPING;
    lock.unlock();

    // Send SIGTERM first
    ::kill(pid, SIGTERM);

    // Wait for exit
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    int status = 0;
    pid_t result;
    bool exited = false;

    while (std::chrono::steady_clock::now() < deadline) {
        result = waitpid(pid, &status, WNOHANG);
        if (result == pid) {
            exited = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!exited) {
        spdlog::warn("Process {} did not exit gracefully, sending SIGKILL", name);
        ::kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }

    lock.lock();
    it = processes_.find(name);
    if (it != processes_.end()) {
        auto& info2 = it->second;
        if (info2.stdin_fd >= 0) { close(info2.stdin_fd); info2.stdin_fd = -1; }
        if (info2.stdout_fd >= 0) { close(info2.stdout_fd); info2.stdout_fd = -1; }
        info2.pid = 0;
        info2.state = PluginState::STOPPED;
        info2.stopped_at = nowMs();
        if (WIFEXITED(status)) {
            info2.exit_code = WEXITSTATUS(status);
        }
    }
    lock.unlock();

    // Join output thread
    auto ot = output_threads_.find(name);
    if (ot != output_threads_.end()) {
        if (ot->second.joinable()) ot->second.join();
        output_threads_.erase(ot);
    }

    spdlog::info("Process stopped: {}", name);
    if (on_state_change_) {
        on_state_change_(name, old_state, PluginState::STOPPED);
    }
    return true;
}

void ProcessManager::stopAll(int timeout_ms) {
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, info] : processes_) {
            if (info.state == PluginState::RUNNING) {
                names.push_back(name);
            }
        }
    }
    for (const auto& name : names) {
        stopProcess(name, timeout_ms);
    }
}

bool ProcessManager::sendToProcess(const std::string& name, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end() || it->second.stdin_fd < 0) return false;

    std::string line = data;
    if (line.empty() || line.back() != '\n') line += '\n';

    ssize_t written = ::write(it->second.stdin_fd, line.data(), line.size());
    return written == static_cast<ssize_t>(line.size());
}

ProcessManager::ProcessInfo ProcessManager::getProcessInfo(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(name);
    if (it == processes_.end()) return {};
    return it->second;
}

std::vector<ProcessManager::ProcessInfo> ProcessManager::getAllProcesses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessInfo> result;
    for (const auto& [name, info] : processes_) {
        result.push_back(info);
    }
    return result;
}

bool ProcessManager::hasProcess(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return processes_.count(name) > 0;
}

void ProcessManager::startMonitor() {
    monitoring_.store(true);
    monitor_thread_ = std::thread(&ProcessManager::monitorLoop, this);
}

void ProcessManager::stopMonitor() {
    monitoring_.store(false);
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

void ProcessManager::monitorLoop() {
    while (monitoring_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, info] : processes_) {
            if (info.state != PluginState::RUNNING) continue;

            int status;
            pid_t result = waitpid(info.pid, &status, WNOHANG);
            if (result == info.pid) {
                // Process exited
                PluginState old_state = info.state;
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    info.state = PluginState::STOPPED;
                } else {
                    info.state = PluginState::CRASHED;
                }
                info.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                info.stopped_at = nowMs();
                info.pid = 0;

                if (info.stdin_fd >= 0) { close(info.stdin_fd); info.stdin_fd = -1; }
                if (info.stdout_fd >= 0) { close(info.stdout_fd); info.stdout_fd = -1; }

                spdlog::warn("Process {} exited with status {} (state: {})",
                            name, info.exit_code, pluginStateStr(info.state));
                if (on_state_change_) {
                    on_state_change_(name, old_state, info.state);
                }
            }
        }
    }
}

void ProcessManager::readOutputLoop(const std::string& name, int fd) {
    char buf[4096];
    std::string line_buf;

    while (true) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        int ret = poll(&pfd, 1, 500);

        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) {
            // Check if process still running
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = processes_.find(name);
            if (it == processes_.end() || it->second.state != PluginState::RUNNING) break;
            continue;
        }

        ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';

        line_buf += buf;
        size_t pos;
        while ((pos = line_buf.find('\n')) != std::string::npos) {
            std::string line = line_buf.substr(0, pos);
            line_buf.erase(0, pos + 1);
            if (on_output_ && !line.empty()) {
                on_output_(name, line);
            }
        }
    }

    // Flush remaining
    if (!line_buf.empty() && on_output_) {
        on_output_(name, line_buf);
    }
}

} // namespace tea
