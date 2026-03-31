#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <vector>

// Local TCP server for receiving binary update requests from HoneyTea/LemonTea
class Updater {
public:
    using UpdateCallback = std::function<bool(const std::string& name, const std::string& temp_path)>;

    Updater(uint16_t port);
    ~Updater();

    void setUpdateCallback(UpdateCallback cb) { on_update_ = std::move(cb); }

    bool start();
    void stop();

private:
    void acceptLoop();
    void handleClient(int client_fd);

    uint16_t port_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    UpdateCallback on_update_;
    std::string temp_dir_ = "/tmp/greentea_updates";
};
