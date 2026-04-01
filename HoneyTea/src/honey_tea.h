#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <map>
#include <mutex>
#include <vector>
#include <tea/common.h>
#include <tea/config.h>
#include <tea/tcp_connection.h>
#include <tea/udp_socket.h>
#include "plugin_manager.h"

class HoneyTea {
public:
    HoneyTea();
    ~HoneyTea();

    bool init(const tea::Config& config);
    void run();
    void shutdown();

    // Plugin operations
    bool startPlugin(const std::string& name) { return plugin_mgr_->startPlugin(name); }
    bool stopPlugin(const std::string& name) { return plugin_mgr_->stopPlugin(name); }
    std::vector<tea::PluginInfo> getPlugins() const { return plugin_mgr_->getPlugins(); }

    // Self-update via GreenTea
    bool requestSelfUpdate(const std::string& new_binary_path);

private:
    struct IncomingTransfer {
        std::string plugin_name;
        size_t total_size = 0;
        size_t received_size = 0;
        std::vector<uint8_t> data;
    };

    void connectToServer();
    void onMessage(tea::TcpConnection::Ptr conn, const tea::Message& msg);
    void onDisconnect(tea::TcpConnection::Ptr conn);
    void heartbeatLoop();
    void reconnectLoop();
    void handlePluginOutput(const std::string& plugin, const tea::json& msg);

    tea::Config config_;
    std::string node_id_;
    std::string node_name_;

    // Network
    std::string server_host_;
    uint16_t server_port_;
    tea::TcpConnection::Ptr server_conn_;
    std::unique_ptr<tea::UdpSocket> udp_socket_;
    uint16_t udp_port_;

    // GreenTea
    std::string greentea_host_;
    uint16_t greentea_port_;

    // Plugins
    std::unique_ptr<PluginManager> plugin_mgr_;

    // Threads
    std::atomic<bool> running_{false};
    std::thread heartbeat_thread_;
    std::thread reconnect_thread_;
    int heartbeat_interval_;
    int reconnect_interval_;
    std::atomic<bool> connected_{false};

    // Remote plugin install transfer states.
    std::mutex transfer_mutex_;
    std::map<std::string, IncomingTransfer> incoming_transfers_;
    std::string pending_install_name_;
};
