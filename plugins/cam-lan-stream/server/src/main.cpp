#include <iostream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace {

constexpr const char* kPluginName = "cam-lan-stream";

std::mutex g_out_mutex;
std::mutex g_state_mutex;
json g_last_status = {
    {"action", "status_result"},
    {"plugin", kPluginName},
    {"state", "stopped"},
    {"service", {{"active", false}}},
    {"camera_config", json::object()},
    {"endpoints", json::object()},
    {"system", json::object()},
    {"last_error", ""},
    {"node_id", ""}
};
uint64_t g_status_version = 0;
std::string g_default_target_node;

void sendMessage(const json& payload) {
    std::lock_guard<std::mutex> lock(g_out_mutex);
    std::cout << payload.dump() << "\n";
    std::cout.flush();
}

json buildCapabilities() {
    return {
        {"action", "capabilities_result"},
        {"plugin", kPluginName},
        {"supported_actions", json::array({
            "describe",
            "discover",
            "status",
            "get_config",
            "set_config",
            "start_stream",
            "stop_stream",
            "restart_stream",
            "set_public_host"
        })},
        {"controls", json::array({
            "camera_id",
            "width",
            "height",
            "fps",
            "bitrate",
            "idr_period",
            "brightness",
            "contrast",
            "saturation",
            "sharpness",
            "exposure",
            "awb",
            "denoise",
            "shutter",
            "gain",
            "ev",
            "hflip",
            "vflip",
            "stream_path",
            "rtsp_port",
            "webrtc_port",
            "webrtc_udp_port",
            "yolo_port",
            "public_host"
        })},
        {"transport", {
            {"frontend", "MediaMTX WebRTC / WHEP"},
            {"yolo", "Annex-B H.264 over TCP"}
        }}
    };
}

std::string resolveTargetNode(const json& msg) {
    const std::vector<std::string> keys = {"target_node", "node_id", "target"};
    for (const auto& key : keys) {
        const std::string value = msg.value(key, std::string(""));
        if (!value.empty()) {
            return value;
        }
    }

    std::lock_guard<std::mutex> lock(g_state_mutex);
    return g_default_target_node;
}

bool isResponseAction(const std::string& action) {
    static const std::unordered_set<std::string> actions = {
        "status_result",
        "config_result",
        "capabilities_result",
        "stream_state",
        "stream_error",
        "stream_warning"
    };
    return actions.count(action) > 0;
}

void cacheStatus(const json& msg) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (msg.contains("from_node") && msg["from_node"].is_string()) {
        g_default_target_node = msg["from_node"].get<std::string>();
        g_last_status["node_id"] = g_default_target_node;
    }

    const std::string action = msg.value("action", std::string(""));
    if (action == "status_result") {
        g_last_status = msg;
        if (!g_default_target_node.empty() && !g_last_status.contains("node_id")) {
            g_last_status["node_id"] = g_default_target_node;
        }
        ++g_status_version;
    } else if (action == "config_result") {
        g_last_status["camera_config"] = msg.value("camera_config", json::object());
        if (msg.contains("state")) {
            g_last_status["state"] = msg["state"];
        }
        ++g_status_version;
    } else if (action == "stream_error") {
        g_last_status["state"] = "error";
        g_last_status["last_error"] = msg.value("message", std::string("unknown error"));
        ++g_status_version;
    }
}

json buildDiscoverResult(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    auto payload = buildCapabilities();
    payload["action"] = "discover_result";
    payload["last_status"] = g_last_status;
    payload["status_version"] = g_status_version;
    payload["default_target_node"] = g_default_target_node;
    if (!request_id.empty()) {
        payload["request_id"] = request_id;
    }
    return payload;
}

json buildCachedStatusResult(const std::string& request_id, const std::string& action_name) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    json payload = g_last_status;
    payload["action"] = action_name;
    if (!request_id.empty()) {
        payload["request_id"] = request_id;
    }
    return payload;
}

void emitBridgeError(const std::string& request_id, const std::string& message) {
    json payload = {
        {"action", "error"},
        {"plugin", kPluginName},
        {"message", message}
    };
    if (!request_id.empty()) {
        payload["request_id"] = request_id;
    }
    sendMessage(payload);
}

void forwardToHoney(const json& msg) {
    const std::string request_id = msg.value("request_id", std::string(""));
    const std::string target_node = resolveTargetNode(msg);
    if (target_node.empty()) {
        emitBridgeError(request_id, "missing target_node for HoneyTea camera control");
        return;
    }

    json data = msg;
    data.erase("target");
    data.erase("target_node");
    data.erase("node_id");

    sendMessage({
        {"action", "message"},
        {"plugin", kPluginName},
        {"target_plugin", kPluginName},
        {"target_node", target_node},
        {"data", data}
    });
}

bool handleMessage(const json& msg) {
    const std::string cmd = msg.value("cmd", std::string(""));
    const std::string action = msg.value("action", std::string(""));
    const std::string request_id = msg.value("request_id", std::string(""));

    if (cmd == "stop") {
        sendMessage({{"action", "stopped"}, {"plugin", kPluginName}});
        return false;
    }

    if (cmd == "start") {
        return true;
    }

    if (cmd == "status") {
        auto payload = buildDiscoverResult(request_id);
        payload["action"] = "status_result";
        sendMessage(payload);
        return true;
    }

    if (isResponseAction(action) || msg.contains("from_node")) {
        cacheStatus(msg);
        sendMessage(msg);
        return true;
    }

    if (action == "ping") {
        sendMessage({{"action", "pong"}, {"plugin", kPluginName}, {"request_id", request_id}});
        return true;
    }

    if (action == "describe" || action == "capabilities") {
        auto payload = buildCapabilities();
        if (!request_id.empty()) {
            payload["request_id"] = request_id;
        }
        sendMessage(payload);
        return true;
    }

    if (action == "discover") {
        sendMessage(buildDiscoverResult(request_id));
        return true;
    }

    if (action == "status" && resolveTargetNode(msg).empty()) {
        sendMessage(buildCachedStatusResult(request_id, "status_result"));
        return true;
    }

    if (action == "get_config" && resolveTargetNode(msg).empty()) {
        sendMessage(buildCachedStatusResult(request_id, "config_result"));
        return true;
    }

    if (action == "status" || action == "get_config" || action == "set_config" ||
        action == "start_stream" || action == "stop_stream" || action == "restart_stream" ||
        action == "set_public_host") {
        forwardToHoney(msg);
        return true;
    }

    emitBridgeError(request_id, "unsupported action: " + action);
    return true;
}

}  // namespace

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("cam-lan-stream server bridge started");

    sendMessage({
        {"action", "ready"},
        {"plugin", kPluginName},
        {"role", "server"},
        {"summary", "LemonTea-side camera control bridge"}
    });

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        try {
            auto msg = json::parse(line);
            if (!handleMessage(msg)) {
                break;
            }
        } catch (const std::exception& e) {
            sendMessage({
                {"action", "error"},
                {"plugin", kPluginName},
                {"message", std::string("invalid json message: ") + e.what()}
            });
        }
    }

    return 0;
}