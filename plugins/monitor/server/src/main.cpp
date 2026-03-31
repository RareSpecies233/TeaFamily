/**
 * Monitor Plugin - Server Side (runs on LemonTea)
 *
 * Aggregates system metrics from client-side plugins and stores
 * recent history. Relays requests/results via parent stdio.
 */

#include <iostream>
#include <string>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

static std::mutex g_mutex;
static constexpr size_t MAX_HISTORY = 300; // ~5 minutes at 1s intervals

struct MetricsHistory {
    std::deque<json> samples;
};

static std::unordered_map<std::string, MetricsHistory> g_history; // node_id -> history

static void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

static void handleMessage(const json& msg) {
    std::string action = msg.value("action", "");

    if (action == "metrics") {
        // Metrics data from client side
        std::string nodeId = msg.value("node_id", "unknown");
        auto& history = g_history[nodeId];
        history.samples.push_back(msg);
        if (history.samples.size() > MAX_HISTORY) {
            history.samples.pop_front();
        }
        // Forward to parent (for potential real-time streaming)
        sendMessage(msg);

    } else if (action == "query_metrics") {
        // Query historical metrics for a node
        std::string nodeId = msg.value("node_id", "");
        std::string reqId = msg.value("request_id", "");
        int count = msg.value("count", 60);

        json result;
        result["action"] = "metrics_result";
        result["request_id"] = reqId;
        result["node_id"] = nodeId;

        auto it = g_history.find(nodeId);
        if (it != g_history.end()) {
            json samples = json::array();
            auto& q = it->second.samples;
            size_t start = q.size() > static_cast<size_t>(count) ? q.size() - count : 0;
            for (size_t i = start; i < q.size(); i++) {
                samples.push_back(q[i]);
            }
            result["samples"] = samples;
        } else {
            result["samples"] = json::array();
        }
        sendMessage(result);

    } else if (action == "request_metrics") {
        // Request to start/stop collecting metrics from a node
        sendMessage(msg);

    } else if (action == "ping") {
        json pong;
        pong["action"] = "pong";
        sendMessage(pong);
    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Monitor plugin server started");

    json ready;
    ready["action"] = "ready";
    ready["plugin"] = "monitor";
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

    spdlog::info("Monitor plugin server exiting");
    return 0;
}
