/**
 * SSH Plugin - Server Side (runs on LemonTea)
 *
 * Receives terminal data from OrangeTea HTTP API and relays
 * to the HoneyTea client-side plugin via parent process stdio.
 * Protocol: JSON-line over stdin/stdout.
 */

#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

static std::mutex g_mutex;
static std::unordered_map<std::string, bool> g_sessions; // session_id -> active

static void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

static void handleMessage(const json& msg) {
    std::string action = msg.value("action", "");

    if (action == "create_session") {
        std::string sessionId = msg.value("session_id", "");
        std::string targetNode = msg.value("target_node", "");
        if (sessionId.empty()) return;

        g_sessions[sessionId] = true;
        spdlog::info("SSH session created: {} -> {}", sessionId, targetNode);

        // Forward to client-side plugin via parent
        json fwd;
        fwd["action"] = "create_session";
        fwd["session_id"] = sessionId;
        fwd["target_node"] = targetNode;
        fwd["cols"] = msg.value("cols", 80);
        fwd["rows"] = msg.value("rows", 24);
        sendMessage(fwd);

    } else if (action == "input") {
        std::string sessionId = msg.value("session_id", "");
        if (g_sessions.count(sessionId)) {
            json fwd;
            fwd["action"] = "input";
            fwd["session_id"] = sessionId;
            fwd["data"] = msg.value("data", "");
            sendMessage(fwd);
        }

    } else if (action == "resize") {
        std::string sessionId = msg.value("session_id", "");
        if (g_sessions.count(sessionId)) {
            json fwd;
            fwd["action"] = "resize";
            fwd["session_id"] = sessionId;
            fwd["cols"] = msg.value("cols", 80);
            fwd["rows"] = msg.value("rows", 24);
            sendMessage(fwd);
        }

    } else if (action == "output") {
        // Output coming from client side, relay back to parent for HTTP delivery
        sendMessage(msg);

    } else if (action == "close_session") {
        std::string sessionId = msg.value("session_id", "");
        g_sessions.erase(sessionId);
        spdlog::info("SSH session closed: {}", sessionId);
        json fwd;
        fwd["action"] = "close_session";
        fwd["session_id"] = sessionId;
        sendMessage(fwd);

    } else if (action == "ping") {
        json pong;
        pong["action"] = "pong";
        sendMessage(pong);
    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("SSH plugin server started");

    // Notify parent we're ready
    json ready;
    ready["action"] = "ready";
    ready["plugin"] = "ssh";
    ready["role"] = "server";
    sendMessage(ready);

    // Read JSON lines from stdin
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            json msg = json::parse(line);
            handleMessage(msg);
        } catch (const json::parse_error& e) {
            spdlog::error("JSON parse error: {}", e.what());
        }
    }

    spdlog::info("SSH plugin server exiting");
    return 0;
}
