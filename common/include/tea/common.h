#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <atomic>

namespace tea {

// Default ports
constexpr uint16_t DEFAULT_TCP_PORT      = 9527;
constexpr uint16_t DEFAULT_HTTP_PORT     = 9528;
constexpr uint16_t DEFAULT_GREENTEA_PORT = 9529;
constexpr uint16_t DEFAULT_UDP_BASE_PORT = 9530;

// Node roles
enum class NodeRole : uint8_t {
    HONEY_TEA = 0,  // Client
    LEMON_TEA = 1   // Server
};

inline const char* nodeRoleStr(NodeRole r) {
    return r == NodeRole::HONEY_TEA ? "HoneyTea" : "LemonTea";
}

// Node info
struct NodeInfo {
    std::string id;
    std::string name;
    NodeRole    role;
    std::string address;
    uint16_t    port = 0;
    bool        connected = false;
    int64_t     last_heartbeat = 0;
};

// Plugin state
enum class PluginState : uint8_t {
    STOPPED = 0,
    STARTING,
    RUNNING,
    STOPPING,
    CRASHED,
    UNKNOWN
};

inline const char* pluginStateStr(PluginState s) {
    switch (s) {
        case PluginState::STOPPED:  return "stopped";
        case PluginState::STARTING: return "starting";
        case PluginState::RUNNING:  return "running";
        case PluginState::STOPPING: return "stopping";
        case PluginState::CRASHED:  return "crashed";
        default:                    return "unknown";
    }
}

// Plugin info
struct PluginInfo {
    std::string  name;
    std::string  version;
    std::string  description;
    std::string  binary_path;
    std::string  comm_type;   // "stdio", "tcp", "udp"
    uint16_t     comm_port = 0;
    PluginState  state = PluginState::STOPPED;
    pid_t        pid = 0;
    bool         auto_start = false;
    bool         has_frontend = false;
    std::string  frontend_path;
};

// Timestamp helper
inline int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // namespace tea
