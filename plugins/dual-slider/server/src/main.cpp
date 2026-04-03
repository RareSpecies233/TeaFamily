#include <iostream>
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

static std::mutex g_out_mutex;

static void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_out_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

static void handleMessage(const json& msg) {
    const std::string action = msg.value("action", "");
    const std::string cmd = msg.value("cmd", "");

    if (cmd == "stop") {
        sendMessage({{"action", "stopped"}, {"plugin", "dual-slider"}});
        return;
    }

    if (action == "ping") {
        sendMessage({{"action", "pong"}, {"request_id", msg.value("request_id", "")}});
        return;
    }

    // dual-slider server acts as a transparent relay between LemonTea and HoneyTea plugin.
    // This keeps one authoritative state on HoneyTea while preserving plugin-rpc API semantics.
    if (!action.empty()) {
        sendMessage(msg);
    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("dual-slider server started");

    sendMessage({
        {"action", "ready"},
        {"plugin", "dual-slider"},
        {"role", "server"}
    });

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            const auto msg = json::parse(line);
            handleMessage(msg);
        } catch (const json::parse_error& e) {
            spdlog::error("dual-slider server parse error: {}", e.what());
        }
    }

    spdlog::info("dual-slider server exiting");
    return 0;
}
