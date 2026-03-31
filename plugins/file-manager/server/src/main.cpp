/**
 * File Manager Plugin - Server Side (runs on LemonTea)
 *
 * Relays file operation requests between OrangeTea HTTP API
 * and HoneyTea client-side plugin via parent process stdio.
 */

#include <iostream>
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

static std::mutex g_mutex;

static void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

static void handleMessage(const json& msg) {
    std::string action = msg.value("action", "");

    if (action == "list" || action == "read" || action == "write" ||
        action == "delete" || action == "mkdir" || action == "rename" ||
        action == "stat" || action == "upload" || action == "download") {
        // Forward all file operations to client side
        sendMessage(msg);

    } else if (action == "list_result" || action == "read_result" ||
               action == "stat_result" || action == "download_result" ||
               action == "error" || action == "success") {
        // Results from client side, relay back to parent
        sendMessage(msg);

    } else if (action == "ping") {
        json pong;
        pong["action"] = "pong";
        sendMessage(pong);
    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("File Manager plugin server started");

    json ready;
    ready["action"] = "ready";
    ready["plugin"] = "file-manager";
    ready["role"] = "server";
    sendMessage(ready);

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

    spdlog::info("File Manager plugin server exiting");
    return 0;
}
