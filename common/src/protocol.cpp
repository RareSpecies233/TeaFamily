#include "tea/protocol.h"
#include <arpa/inet.h>
#include <cstring>
#include <spdlog/spdlog.h>

namespace tea {

std::vector<uint8_t> Message::serialize() const {
    std::string payload_str = payload.dump();
    uint32_t payload_len = static_cast<uint32_t>(payload_str.size());
    uint32_t total_len = payload_len; // length field does not include header

    std::vector<uint8_t> buf(MSG_HEADER_SIZE + payload_len);

    // Write length (network byte order)
    uint32_t net_len = htonl(total_len);
    std::memcpy(buf.data(), &net_len, 4);

    // Write type (network byte order)
    uint16_t net_type = htons(static_cast<uint16_t>(type));
    std::memcpy(buf.data() + 4, &net_type, 2);

    // Write payload
    std::memcpy(buf.data() + MSG_HEADER_SIZE, payload_str.data(), payload_len);

    return buf;
}

int Message::deserialize(const uint8_t* data, size_t len, Message& msg) {
    if (len < MSG_HEADER_SIZE) return 0; // incomplete header

    uint32_t net_len;
    std::memcpy(&net_len, data, 4);
    uint32_t payload_len = ntohl(net_len);

    if (payload_len > MAX_MSG_SIZE) return -1; // too large

    size_t total = MSG_HEADER_SIZE + payload_len;
    if (len < total) return 0; // incomplete payload

    uint16_t net_type;
    std::memcpy(&net_type, data + 4, 2);
    msg.type = static_cast<MsgType>(ntohs(net_type));

    if (payload_len > 0) {
        try {
            msg.payload = json::parse(data + MSG_HEADER_SIZE,
                                       data + MSG_HEADER_SIZE + payload_len);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse message payload: {}", e.what());
            return -1;
        }
    } else {
        msg.payload = json::object();
    }

    return static_cast<int>(total);
}

Message Message::makeHandshake(const std::string& node_id, const std::string& role) {
    return {MsgType::HANDSHAKE, {{"node_id", node_id}, {"role", role}}};
}

Message Message::makeHeartbeat() {
    return {MsgType::HEARTBEAT, {{"timestamp", nowMs()}}};
}

Message Message::makePluginListReq() {
    return {MsgType::PLUGIN_LIST_REQ, json::object()};
}

Message Message::makePluginListRsp(const json& plugins) {
    return {MsgType::PLUGIN_LIST_RSP, {{"plugins", plugins}}};
}

Message Message::makePluginStart(const std::string& name) {
    return {MsgType::PLUGIN_START, {{"name", name}}};
}

Message Message::makePluginStop(const std::string& name) {
    return {MsgType::PLUGIN_STOP, {{"name", name}}};
}

Message Message::makeChildMsg(const std::string& plugin, const std::string& target, const json& data) {
    return {MsgType::CHILD_MSG, {{"plugin", plugin}, {"target", target}, {"data", data}}};
}

Message Message::makeError(const std::string& error) {
    return {MsgType::ERROR_MSG, {{"error", error}}};
}

Message Message::makeFileTransferBegin(const std::string& filename, size_t total_size) {
    return {MsgType::FILE_TRANSFER_BEGIN, {
        {"filename", filename},
        {"total_size", total_size},
        {"transfer_id", std::to_string(nowMs())}
    }};
}

Message Message::makeFileTransferData(const std::string& transfer_id,
                                       const std::vector<uint8_t>& chunk, size_t offset) {
    // Encode chunk as base64-like hex string for JSON transport
    std::string hex;
    hex.reserve(chunk.size() * 2);
    static const char digits[] = "0123456789abcdef";
    for (uint8_t b : chunk) {
        hex.push_back(digits[b >> 4]);
        hex.push_back(digits[b & 0x0f]);
    }
    return {MsgType::FILE_TRANSFER_DATA, {
        {"transfer_id", transfer_id},
        {"offset", offset},
        {"size", chunk.size()},
        {"data", hex}
    }};
}

Message Message::makeFileTransferEnd(const std::string& transfer_id) {
    return {MsgType::FILE_TRANSFER_END, {{"transfer_id", transfer_id}}};
}

Message Message::makeUpdateBinary(const std::string& target, size_t size) {
    return {MsgType::UPDATE_BINARY, {{"target", target}, {"size", size}}};
}

} // namespace tea
