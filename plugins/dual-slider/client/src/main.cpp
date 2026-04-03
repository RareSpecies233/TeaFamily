#include <iostream>
#include <string>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace {
std::mutex g_mutex;
std::mutex g_out_mutex;

double g_slider_a = 0.0;
double g_slider_b = 0.0;
int64_t g_updated_at_ms = 0;

constexpr double kMinValue = -100.0;
constexpr double kMaxValue = 100.0;
constexpr double kStep = 10.0;

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

double clampPercent(double v) {
    return std::max(kMinValue, std::min(kMaxValue, v));
}

void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_out_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

json buildValuesPayload(const std::string& action,
                        const std::string& request_id,
                        const std::string& source) {
    std::lock_guard<std::mutex> lock(g_mutex);
    json out;
    out["action"] = action;
    if (!request_id.empty()) {
        out["request_id"] = request_id;
    }
    if (!source.empty()) {
        out["source"] = source;
    }
    out["slider_a"] = g_slider_a;
    out["slider_b"] = g_slider_b;
    out["updated_at_ms"] = g_updated_at_ms;
    return out;
}

void emitValues(const std::string& action,
                const std::string& request_id,
                const std::string& source,
                const json* original_msg) {
    auto payload = buildValuesPayload(action, request_id, source);
    sendMessage(payload);

    if (!original_msg) {
        return;
    }

    // Optional plugin-to-plugin reply path: when called by another plugin message,
    // forward values back to requester using the standard message relay event.
    const std::string from_plugin = original_msg->value("from_plugin", std::string(""));
    if (!from_plugin.empty()) {
        json relay;
        relay["event"] = "message";
        relay["target_plugin"] = from_plugin;
        relay["target_node"] = original_msg->value("from_node", std::string(""));

        json relay_data = payload;
        relay_data["action"] = "slider_values";
        relay_data["from_plugin"] = "dual-slider";
        relay["data"] = relay_data;
        sendMessage(relay);
    }
}

void setBoth(double a, double b) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_slider_a = clampPercent(a);
    g_slider_b = clampPercent(b);
    g_updated_at_ms = nowMs();
}

void adjustOne(char target, const std::string& mode) {
    std::lock_guard<std::mutex> lock(g_mutex);
    double* ref = (target == 'b') ? &g_slider_b : &g_slider_a;

    if (mode == "inc") {
        *ref = clampPercent(*ref + kStep);
    } else if (mode == "dec") {
        *ref = clampPercent(*ref - kStep);
    } else if (mode == "reset") {
        *ref = 0.0;
    }
    g_updated_at_ms = nowMs();
}

void handleMessage(const json& msg) {
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

    if (action == "get_values") {
        emitValues("values", msg.value("request_id", ""), msg.value("source", "query"), &msg);
        return;
    }

    if (action == "set_values") {
        double a = msg.value("slider_a", g_slider_a);
        double b = msg.value("slider_b", g_slider_b);
        setBoth(a, b);
        emitValues("values", msg.value("request_id", ""), msg.value("source", "set_values"), &msg);
        return;
    }

    if (action == "adjust") {
        std::string target = msg.value("target", "a");
        std::string mode = msg.value("mode", "inc");
        adjustOne((target == "b") ? 'b' : 'a', mode);
        emitValues("values", msg.value("request_id", ""), msg.value("source", "adjust"), &msg);
        return;
    }

    if (action == "reset_all") {
        setBoth(0.0, 0.0);
        emitValues("values", msg.value("request_id", ""), msg.value("source", "reset_all"), &msg);
        return;
    }
}
} // namespace

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("dual-slider client started");

    g_updated_at_ms = nowMs();

    sendMessage({
        {"action", "ready"},
        {"plugin", "dual-slider"},
        {"role", "client"}
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
            spdlog::error("dual-slider client parse error: {}", e.what());
        }
    }

    spdlog::info("dual-slider client exiting");
    return 0;
}
