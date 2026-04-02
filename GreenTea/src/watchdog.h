#pragma once

#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <sys/types.h>
#include <tea/config.h>

class Watchdog {
public:
    struct WatchedProcess {
        std::string name;
        std::string binary;
        std::vector<std::string> args;
        std::string working_dir;
        int startup_delay_sec = 0;
        int max_restarts = 10;
        int restart_delay_sec = 2;
        bool auto_start = true;
        int start_order = 0;
        // Runtime state
        pid_t pid = 0;
        int parent_guard_fd = -1;
        bool running = false;
        int restart_count = 0;
        int64_t last_start_time = 0;
        int64_t last_stop_time = 0;
        int last_exit_code = 0;
    };

    Watchdog();
    ~Watchdog();

    bool loadConfig(const tea::Config& config);
    void start();
    void stop();

    // Manual control
    bool startProcess(const std::string& name);
    bool stopProcess(const std::string& name);
    bool restartProcess(const std::string& name);

    // Update binary
    bool updateBinary(const std::string& name, const std::string& new_binary_path);

    // Get status
    std::vector<WatchedProcess> getProcesses() const;
    WatchedProcess getProcess(const std::string& name) const;

private:
    void monitorLoop();
    bool launchProcess(WatchedProcess& proc);
    bool killProcess(WatchedProcess& proc, int timeout_ms = 5000);
    void closeParentGuardFd(WatchedProcess& proc);

    mutable std::mutex mutex_;
    std::map<std::string, WatchedProcess> processes_;
    int check_interval_sec_ = 3;

    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
};
