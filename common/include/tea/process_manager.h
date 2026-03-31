#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <sys/types.h>
#include "tea/common.h"
#include <nlohmann/json.hpp>

namespace tea {

using json = nlohmann::json;

// Plugin process lifecycle manager
class ProcessManager {
public:
    struct ProcessInfo {
        std::string name;
        std::string binary_path;
        std::vector<std::string> args;
        std::string working_dir;
        pid_t pid = 0;
        PluginState state = PluginState::STOPPED;
        int exit_code = 0;
        int restart_count = 0;
        int64_t started_at = 0;
        int64_t stopped_at = 0;
        // I/O pipes for stdio communication
        int stdin_fd = -1;
        int stdout_fd = -1;
    };

    using StateChangeCallback = std::function<void(const std::string& name, PluginState old_state, PluginState new_state)>;
    using OutputCallback = std::function<void(const std::string& name, const std::string& line)>;

    ProcessManager();
    ~ProcessManager();

    // Register a process to manage
    bool registerProcess(const std::string& name, const std::string& binary,
                         const std::vector<std::string>& args = {},
                         const std::string& work_dir = ".");

    // Unregister
    bool unregisterProcess(const std::string& name);

    // Start/stop
    bool startProcess(const std::string& name);
    bool stopProcess(const std::string& name, int timeout_ms = 5000);

    // Stop all
    void stopAll(int timeout_ms = 5000);

    // Send data to process stdin
    bool sendToProcess(const std::string& name, const std::string& data);

    // Get info
    ProcessInfo getProcessInfo(const std::string& name) const;
    std::vector<ProcessInfo> getAllProcesses() const;
    bool hasProcess(const std::string& name) const;

    // Callbacks
    void setStateChangeCallback(StateChangeCallback cb) { on_state_change_ = std::move(cb); }
    void setOutputCallback(OutputCallback cb) { on_output_ = std::move(cb); }

    // Start monitoring thread
    void startMonitor();
    void stopMonitor();

private:
    void monitorLoop();
    void readOutputLoop(const std::string& name, int fd);

    mutable std::mutex mutex_;
    std::map<std::string, ProcessInfo> processes_;

    StateChangeCallback on_state_change_;
    OutputCallback on_output_;

    std::atomic<bool> monitoring_{false};
    std::thread monitor_thread_;
    std::map<std::string, std::thread> output_threads_;
};

} // namespace tea
