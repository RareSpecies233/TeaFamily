#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace {
std::mutex g_state_mutex;
std::mutex g_stdout_mutex;
std::mutex g_udp_peers_mutex;

std::atomic<bool> g_running{true};

int g_udp_fd = -1;
std::thread g_udp_thread;

std::string g_udp_bind_host = "127.0.0.1";
uint16_t g_udp_port = 19731;

double g_slider_a = 0.0;
double g_slider_b = 0.0;
int64_t g_updated_at_ms = 0;

constexpr double kMinValue = -100.0;
constexpr double kMaxValue = 100.0;
constexpr double kStep = 10.0;

struct UdpPeer {
    sockaddr_in addr{};
    int64_t last_seen_ms = 0;
};

std::unordered_map<std::string, UdpPeer> g_udp_peers;

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

double clampPercent(double v) {
    return std::max(kMinValue, std::min(kMaxValue, v));
}

void sendStdout(const json& msg) {
    std::lock_guard<std::mutex> lock(g_stdout_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

std::string peerKey(const sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

bool sendUdpJson(const json& payload, const sockaddr_in& peer) {
    if (g_udp_fd < 0) {
        return false;
    }

    const std::string body = payload.dump();
    const auto sent = ::sendto(g_udp_fd,
                               body.data(),
                               body.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&peer),
                               sizeof(peer));
    return sent == static_cast<ssize_t>(body.size());
}

void rememberUdpPeer(const sockaddr_in& peer) {
    std::lock_guard<std::mutex> lock(g_udp_peers_mutex);
    g_udp_peers[peerKey(peer)] = UdpPeer{peer, nowMs()};
}

void broadcastUdpJson(const json& payload, const sockaddr_in* exclude_peer = nullptr) {
    if (g_udp_fd < 0) {
        return;
    }

    std::vector<UdpPeer> peers;
    {
        std::lock_guard<std::mutex> lock(g_udp_peers_mutex);
        const int64_t cutoff = nowMs() - 120000;
        for (auto it = g_udp_peers.begin(); it != g_udp_peers.end();) {
            if (it->second.last_seen_ms < cutoff) {
                it = g_udp_peers.erase(it);
                continue;
            }
            peers.push_back(it->second);
            ++it;
        }
    }

    std::string exclude;
    if (exclude_peer) {
        exclude = peerKey(*exclude_peer);
    }

    for (const auto& peer : peers) {
        if (!exclude.empty() && peerKey(peer.addr) == exclude) {
            continue;
        }
        sendUdpJson(payload, peer.addr);
    }
}

json buildValuesPayload(const std::string& action,
                        const std::string& request_id,
                        const std::string& source) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
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

void relayToRequestingPluginIfNeeded(const json& request, const json& values_payload) {
    const std::string from_plugin = request.value("from_plugin", std::string(""));
    if (from_plugin.empty()) {
        return;
    }

    json relay;
    relay["event"] = "message";
    relay["target_plugin"] = from_plugin;
    relay["target_node"] = request.value("from_node", std::string(""));

    json data = values_payload;
    data["action"] = "slider_values";
    data["from_plugin"] = "dual-slider";
    relay["data"] = data;

    sendStdout(relay);
}

void setBothValues(double a, double b) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_slider_a = clampPercent(a);
    g_slider_b = clampPercent(b);
    g_updated_at_ms = nowMs();
}

void adjustOneValue(char target, const std::string& mode) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
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

json transportInfo(const std::string& request_id = "") {
    json payload;
    payload["action"] = "transport_info";
    if (!request_id.empty()) {
        payload["request_id"] = request_id;
    }
    payload["udp_enabled"] = g_udp_fd >= 0;
    payload["udp_host"] = g_udp_bind_host;
    payload["udp_port"] = g_udp_port;
    payload["browser_note"] = "OrangeTea 浏览器环境无法直接使用 UDP，前端使用低延迟 HTTP 快通道。";
    return payload;
}

void emitValuesAndNotify(const std::string& request_id,
                         const std::string& source,
                         const json* original_msg,
                         const sockaddr_in* udp_peer,
                         bool notify_stdout,
                         bool notify_udp_broadcast) {
    const auto values_payload = buildValuesPayload("values", request_id, source);

    if (notify_stdout) {
        sendStdout(values_payload);
    }
    if (udp_peer) {
        sendUdpJson(values_payload, *udp_peer);
    }
    if (notify_udp_broadcast) {
        broadcastUdpJson(values_payload, udp_peer);
    }
    if (original_msg) {
        relayToRequestingPluginIfNeeded(*original_msg, values_payload);
    }
}

void processMessage(const json& msg, const sockaddr_in* udp_peer = nullptr) {
    const std::string action = msg.value("action", "");
    const std::string cmd = msg.value("cmd", "");
    const std::string request_id = msg.value("request_id", std::string(""));

    if (cmd == "stop") {
        sendStdout({{"action", "stopped"}, {"plugin", "dual-slider"}});
        return;
    }

    if (action == "ping") {
        json pong = {{"action", "pong"}};
        if (!request_id.empty()) {
            pong["request_id"] = request_id;
        }
        if (udp_peer) {
            sendUdpJson(pong, *udp_peer);
        } else {
            sendStdout(pong);
        }
        return;
    }

    if (action == "get_transport") {
        const auto payload = transportInfo(request_id);
        if (udp_peer) {
            sendUdpJson(payload, *udp_peer);
        } else {
            sendStdout(payload);
        }
        return;
    }

    if (action == "get_values") {
        emitValuesAndNotify(request_id,
                            msg.value("source", "query"),
                            &msg,
                            udp_peer,
                            !udp_peer,
                            false);
        return;
    }

    if (action == "set_values") {
        const double a = msg.value("slider_a", g_slider_a);
        const double b = msg.value("slider_b", g_slider_b);
        setBothValues(a, b);
        emitValuesAndNotify(request_id,
                            msg.value("source", "set_values"),
                            &msg,
                            udp_peer,
                            true,
                            true);
        return;
    }

    if (action == "adjust") {
        const std::string target = msg.value("target", "a");
        const std::string mode = msg.value("mode", "inc");
        adjustOneValue((target == "b") ? 'b' : 'a', mode);
        emitValuesAndNotify(request_id,
                            msg.value("source", "adjust"),
                            &msg,
                            udp_peer,
                            true,
                            true);
        return;
    }

    if (action == "reset_all") {
        setBothValues(0.0, 0.0);
        emitValuesAndNotify(request_id,
                            msg.value("source", "reset_all"),
                            &msg,
                            udp_peer,
                            true,
                            true);
        return;
    }

    if (!action.empty() || !cmd.empty()) {
        json err;
        err["action"] = "error";
        if (!request_id.empty()) {
            err["request_id"] = request_id;
        }
        err["message"] = "unsupported action: " + action;

        if (udp_peer) {
            sendUdpJson(err, *udp_peer);
        } else {
            sendStdout(err);
        }
    }
}

void udpLoop() {
    char buf[4096];

    while (g_running.load()) {
        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);
        const auto n = ::recvfrom(g_udp_fd,
                                  buf,
                                  sizeof(buf) - 1,
                                  0,
                                  reinterpret_cast<sockaddr*>(&peer),
                                  &peer_len);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            spdlog::error("dual-slider udp recv error: {}", std::strerror(errno));
            break;
        }

        if (n == 0) {
            continue;
        }

        buf[n] = '\0';
        rememberUdpPeer(peer);

        try {
            const auto msg = json::parse(buf);
            processMessage(msg, &peer);
        } catch (const json::parse_error& e) {
            json err;
            err["action"] = "error";
            err["message"] = std::string("invalid udp json: ") + e.what();
            sendUdpJson(err, peer);
        }
    }
}

void stopUdp() {
    g_running.store(false);

    if (g_udp_thread.joinable()) {
        g_udp_thread.join();
    }

    if (g_udp_fd >= 0) {
        ::close(g_udp_fd);
        g_udp_fd = -1;
    }
}

void configureUdpFromEnv() {
    if (const char* host = std::getenv("TEA_DUAL_SLIDER_UDP_BIND"); host && *host) {
        g_udp_bind_host = host;
    }

    if (const char* port = std::getenv("TEA_DUAL_SLIDER_UDP_PORT"); port && *port) {
        try {
            const int parsed = std::stoi(port);
            if (parsed > 0 && parsed <= 65535) {
                g_udp_port = static_cast<uint16_t>(parsed);
            }
        } catch (...) {
            spdlog::warn("Invalid TEA_DUAL_SLIDER_UDP_PORT='{}', fallback to {}", port, g_udp_port);
        }
    }
}

void startUdp() {
    configureUdpFromEnv();

    g_udp_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (g_udp_fd < 0) {
        spdlog::error("dual-slider udp socket create failed: {}", std::strerror(errno));
        return;
    }

    int reuse = 1;
    ::setsockopt(g_udp_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    const int flags = fcntl(g_udp_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(g_udp_fd, F_SETFL, flags | O_NONBLOCK);
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_udp_port);

    if (g_udp_bind_host == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (::inet_pton(AF_INET, g_udp_bind_host.c_str(), &addr.sin_addr) <= 0) {
        spdlog::warn("dual-slider invalid udp bind host '{}', fallback to 127.0.0.1", g_udp_bind_host);
        g_udp_bind_host = "127.0.0.1";
        ::inet_pton(AF_INET, g_udp_bind_host.c_str(), &addr.sin_addr);
    }

    if (::bind(g_udp_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::error("dual-slider udp bind {}:{} failed: {}",
                      g_udp_bind_host,
                      g_udp_port,
                      std::strerror(errno));
        ::close(g_udp_fd);
        g_udp_fd = -1;
        return;
    }

    spdlog::info("dual-slider udp listening on {}:{}", g_udp_bind_host, g_udp_port);
    g_udp_thread = std::thread(udpLoop);
}

}  // namespace

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("dual-slider server started");

    g_updated_at_ms = nowMs();

    sendStdout({
        {"action", "ready"},
        {"plugin", "dual-slider"},
        {"role", "server"}
    });

    startUdp();
    sendStdout(transportInfo());

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            const auto msg = json::parse(line);
            processMessage(msg, nullptr);
        } catch (const json::parse_error& e) {
            spdlog::error("dual-slider server parse error: {}", e.what());
        }
    }

    stopUdp();
    spdlog::info("dual-slider server exiting");
    return 0;
}
