#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <map>
#include <mutex>
#include <tea/common.h>
#include <tea/config.h>
#include <tea/tcp_server.h>
#include <tea/tcp_connection.h>
#include <tea/udp_socket.h>
#include "plugin_manager.h"

// Forward declare
class HttpApi;

class LemonTea {
public:
    LemonTea();
    ~LemonTea();

    bool init(const tea::Config& config);
    void run();
    void shutdown();

    // Plugin operations (local)
    bool startPlugin(const std::string& name) { return plugin_mgr_->startPlugin(name); }
    bool stopPlugin(const std::string& name) { return plugin_mgr_->stopPlugin(name); }
    std::vector<tea::PluginInfo> getPlugins() const { return plugin_mgr_->getPlugins(); }
    bool installPlugin(const std::string& name, const std::vector<uint8_t>& data) {
        return plugin_mgr_->installPlugin(name, data);
    }
    bool uninstallPlugin(const std::string& name) {
        return plugin_mgr_->uninstallPlugin(name);
    }

    // Remote HoneyTea operations
    bool startRemotePlugin(const std::string& node_id, const std::string& name);
    bool stopRemotePlugin(const std::string& node_id, const std::string& name);
    bool uninstallRemotePlugin(const std::string& node_id, const std::string& name);
    void requestRemotePluginList(const std::string& node_id);
    bool installRemotePlugin(const std::string& node_id, const std::string& name,
                             const std::vector<uint8_t>& data);

    // Get connected HoneyTea clients
    struct ClientInfo {
        std::string node_id;
        std::string address;
        uint16_t port;
        bool connected;
        int64_t last_heartbeat;
        tea::json cached_plugins;
    };
    std::vector<ClientInfo> getClients() const;

    // Self-update via GreenTea
    bool requestSelfUpdate(const std::string& new_binary_path);

    // Send message to a specific plugin on a specific node
    bool sendPluginMessage(const std::string& node_id, const std::string& plugin,
                          const tea::json& data);

    // Access to config for HTTP server setup
    const tea::Config& config() const { return config_; }
    const std::string& frontendPluginsDir() const { return frontend_plugins_dir_; }

private:
    void onClientConnect(tea::TcpConnection::Ptr conn);
    void onClientMessage(tea::TcpConnection::Ptr conn, const tea::Message& msg);
    void onClientDisconnect(tea::TcpConnection::Ptr conn);
    void heartbeatCheckLoop();
    void handlePluginOutput(const std::string& plugin, const tea::json& msg);

    tea::Config config_;
    std::string node_id_;
    std::string node_name_;

    // TCP server for HoneyTea connections
    std::unique_ptr<tea::TcpServer> tcp_server_;
    uint16_t tcp_port_;

    // UDP
    std::unique_ptr<tea::UdpSocket> udp_socket_;
    uint16_t udp_port_;

    // GreenTea
    std::string greentea_host_;
    uint16_t greentea_port_;

    // Plugins (local)
    std::unique_ptr<PluginManager> plugin_mgr_;
    std::string frontend_plugins_dir_;

    // Client tracking
    mutable std::mutex clients_mutex_;
    std::map<std::string, ClientInfo> clients_;  // node_id -> info
    std::map<int, std::string> fd_to_node_;      // fd -> node_id

    // Heartbeat
    std::atomic<bool> running_{false};
    std::thread heartbeat_thread_;
    int heartbeat_timeout_;
};
