#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tea {

using json = nlohmann::json;

// Message type definitions
enum class MsgType : uint16_t {
    // Connection management
    HANDSHAKE       = 0x0001,
    HANDSHAKE_ACK   = 0x0002,
    HEARTBEAT       = 0x0003,
    HEARTBEAT_ACK   = 0x0004,
    DISCONNECT      = 0x0005,

    // Plugin management
    PLUGIN_LIST_REQ    = 0x0010,
    PLUGIN_LIST_RSP    = 0x0011,
    PLUGIN_START       = 0x0012,
    PLUGIN_START_ACK   = 0x0013,
    PLUGIN_STOP        = 0x0014,
    PLUGIN_STOP_ACK    = 0x0015,
    PLUGIN_STATUS      = 0x0016,
    PLUGIN_INSTALL     = 0x0017,
    PLUGIN_INSTALL_ACK = 0x0018,
    PLUGIN_UNINSTALL   = 0x0019,

    // Child process messages (forwarded between nodes)
    CHILD_MSG          = 0x0020,
    CHILD_MSG_UDP      = 0x0021,

    // File transfer
    FILE_TRANSFER_BEGIN = 0x0030,
    FILE_TRANSFER_DATA  = 0x0031,
    FILE_TRANSFER_END   = 0x0032,
    FILE_TRANSFER_ACK   = 0x0033,

    // Self update
    UPDATE_BINARY  = 0x0040,
    UPDATE_ACK     = 0x0041,

    // Error
    ERROR_MSG      = 0x00FF,
};

// Wire format:
// [4 bytes: payload length (network order)]
// [2 bytes: message type (network order)]
// [N bytes: JSON payload (UTF-8)]

constexpr size_t MSG_HEADER_SIZE = 6;  // 4 + 2
constexpr size_t MAX_MSG_SIZE = 64 * 1024 * 1024; // 64 MB

struct Message {
    MsgType     type;
    json        payload;

    // Serialize to wire format
    std::vector<uint8_t> serialize() const;

    // Deserialize from buffer (must have at least MSG_HEADER_SIZE bytes to read header)
    // Returns bytes consumed, 0 if incomplete, -1 if error
    static int deserialize(const uint8_t* data, size_t len, Message& msg);

    // Helper constructors
    static Message makeHandshake(const std::string& node_id, const std::string& role);
    static Message makeHeartbeat();
    static Message makePluginListReq();
    static Message makePluginListRsp(const json& plugins);
    static Message makePluginStart(const std::string& name);
    static Message makePluginStop(const std::string& name);
    static Message makeChildMsg(const std::string& plugin, const std::string& target, const json& data);
    static Message makeError(const std::string& error);
    static Message makeFileTransferBegin(const std::string& filename, size_t total_size);
    static Message makeFileTransferData(const std::string& transfer_id, const std::vector<uint8_t>& chunk, size_t offset);
    static Message makeFileTransferEnd(const std::string& transfer_id);
    static Message makeUpdateBinary(const std::string& target, size_t size);
};

} // namespace tea
