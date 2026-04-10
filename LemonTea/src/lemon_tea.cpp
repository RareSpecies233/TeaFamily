#include "lemon_tea.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace fs = std::filesystem;

LemonTea::LemonTea() {}

LemonTea::~LemonTea() {
    shutdown();
}

bool LemonTea::init(const tea::Config& config) {
    config_ = config;
    node_id_ = config.get<std::string>("node_id", "lemon-001");
    node_name_ = config.get<std::string>("node_name", "LemonTea-Default");
    tcp_port_ = config.get<int>("tcp_port", tea::DEFAULT_TCP_PORT);
    udp_port_ = config.get<int>("udp_port", tea::DEFAULT_UDP_BASE_PORT + 1);
    greentea_host_ = config.get<std::string>("greentea_host", "127.0.0.1");
    greentea_port_ = config.get<int>("greentea_port", tea::DEFAULT_GREENTEA_PORT);
    heartbeat_timeout_ = config.get<int>("heartbeat_timeout_sec", 15);
    frontend_plugins_dir_ = config.get<std::string>("frontend_plugins_dir", "frontend_plugins");

    fs::create_directories(frontend_plugins_dir_);

    // Setup plugins
    std::string plugins_dir = config.get<std::string>("plugins_dir", "plugins");
    plugin_mgr_ = std::make_unique<PluginManager>(plugins_dir);
    plugin_mgr_->scanPlugins();
    plugin_mgr_->setPluginOutputCallback(
        [this](const std::string& plugin, const tea::json& msg) {
            handlePluginOutput(plugin, msg);
        }
    );

    // Setup TCP server
    tcp_server_ = std::make_unique<tea::TcpServer>(tcp_port_);
    tcp_server_->setConnectionCallback([this](tea::TcpConnection::Ptr conn) {
        onClientConnect(conn);
    });
    tcp_server_->setMessageCallback([this](tea::TcpConnection::Ptr conn, const tea::Message& msg) {
        onClientMessage(conn, msg);
    });
    tcp_server_->setDisconnectCallback([this](tea::TcpConnection::Ptr conn) {
        onClientDisconnect(conn);
    });

    // Setup UDP
    udp_socket_ = std::make_unique<tea::UdpSocket>();
    if (udp_socket_->bind(udp_port_)) {
        udp_socket_->setRecvCallback(
            [this](const std::string& addr, uint16_t port, const std::vector<uint8_t>& data) {
                spdlog::debug("UDP received from {}:{}, {} bytes", addr, port, data.size());
            }
        );
        udp_socket_->startRecv();
    }

    spdlog::info("LemonTea initialized: id={}, tcp_port={}", node_id_, tcp_port_);
    return true;
}

void LemonTea::run() {
    running_.store(true);

    // Start TCP server
    if (!tcp_server_->start()) {
        spdlog::error("Failed to start TCP server");
        return;
    }

    // Start plugin monitor
    plugin_mgr_->startMonitor();

    // Auto-start plugins
    for (const auto& p : plugin_mgr_->getPlugins()) {
        if (p.auto_start) {
            plugin_mgr_->startPlugin(p.name);
        }
    }

    // Start heartbeat check
    heartbeat_thread_ = std::thread(&LemonTea::heartbeatCheckLoop, this);

    spdlog::info("LemonTea is running");
}

void LemonTea::shutdown() {
    if (!running_.exchange(false)) {
        return;
    }

    spdlog::info("LemonTea shutting down...");

    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();

    if (plugin_mgr_) {
        plugin_mgr_->stopAll();
        plugin_mgr_->stopMonitor();
    }

    if (tcp_server_) tcp_server_->stop();
    if (udp_socket_) udp_socket_->stopRecv();

    spdlog::info("LemonTea stopped");
}

void LemonTea::onClientConnect(tea::TcpConnection::Ptr conn) {
    spdlog::info("New client connection from {}:{}", conn->peerAddr(), conn->peerPort());
}

void LemonTea::onClientMessage(tea::TcpConnection::Ptr conn, const tea::Message& msg) {
    switch (msg.type) {
        case tea::MsgType::HANDSHAKE: {
            std::string node_id = msg.payload.value("node_id", "");
            std::string role = msg.payload.value("role", "");
            conn->setUserData("node_id", node_id);
            conn->setUserData("role", role);

            // Keep a single active connection per node_id to avoid routing commands
            // (such as remote plugin install) to stale duplicate sockets.
            std::vector<tea::TcpConnection::Ptr> stale_connections;
            for (const auto& c : tcp_server_->connections()) {
                if (!c || c->fd() == conn->fd()) {
                    continue;
                }
                if (c->getUserData("node_id") == node_id) {
                    stale_connections.push_back(c);
                }
            }
            for (const auto& stale : stale_connections) {
                spdlog::warn("Duplicate connection for node {} detected, closing stale {}:{}",
                             node_id, stale->peerAddr(), stale->peerPort());
                stale->stop();
            }

            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                ClientInfo ci;
                ci.node_id = node_id;
                ci.address = conn->peerAddr();
                ci.port = conn->peerPort();
                ci.connected = true;
                ci.last_heartbeat = tea::nowMs();
                clients_[node_id] = ci;
                fd_to_node_[conn->fd()] = node_id;
            }

            conn->send({tea::MsgType::HANDSHAKE_ACK,
                       {{"node_id", node_id_}, {"role", "LemonTea"}}});
            spdlog::info("Client handshake: {} ({})", node_id, role);

            // Request plugin list from new client
            conn->send(tea::Message::makePluginListReq());
            break;
        }

        case tea::MsgType::HEARTBEAT: {
            std::string node_id = conn->getUserData("node_id");
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                auto it = clients_.find(node_id);
                if (it != clients_.end()) {
                    it->second.last_heartbeat = tea::nowMs();
                }
            }
            conn->send({tea::MsgType::HEARTBEAT_ACK, {{"timestamp", tea::nowMs()}}});
            break;
        }

        case tea::MsgType::PLUGIN_LIST_RSP: {
            std::string node_id = conn->getUserData("node_id");
            auto plugins = msg.payload.value("plugins", tea::json::array());
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                auto it = clients_.find(node_id);
                if (it != clients_.end()) {
                    it->second.cached_plugins = plugins;
                }
            }
            spdlog::info("Received plugin list from {}: {} plugins",
                        node_id, plugins.size());
            break;
        }

        case tea::MsgType::PLUGIN_START_ACK:
        case tea::MsgType::PLUGIN_STOP_ACK: {
            std::string name = msg.payload.value("name", "");
            bool success = msg.payload.value("success", false);
            spdlog::info("Plugin {} {}: {}",
                        name, msg.type == tea::MsgType::PLUGIN_START_ACK ? "start" : "stop",
                        success ? "ok" : "failed");
            // Keep client-side plugin status fresh in cached list for frontend polling.
            conn->send(tea::Message::makePluginListReq());
            break;
        }

        case tea::MsgType::PLUGIN_INSTALL_ACK: {
            std::string node_id = conn->getUserData("node_id");
            std::string name = msg.payload.value("name", "");
            if (msg.payload.contains("ready") && msg.payload.value("ready", false) &&
                !msg.payload.contains("success")) {
                spdlog::info("Plugin {} install transfer ready", name);
                break;
            }

            bool success = msg.payload.value("success", false);
            std::string error = msg.payload.value("error", std::string(""));

            {
                std::lock_guard<std::mutex> lock(remote_install_mutex_);
                auto key = remoteInstallKey(node_id, name);
                remote_install_results_[key] = RemoteInstallResult{
                    true,
                    success,
                    error
                };
            }
            remote_install_cv_.notify_all();

            spdlog::info("Plugin {} install: {}", name, success ? "ok" : "failed");
            conn->send(tea::Message::makePluginListReq());
            break;
        }

        case tea::MsgType::FILE_TRANSFER_ACK: {
            // Optional flow-control ACK, currently not required for command handling.
            break;
        }

        case tea::MsgType::CHILD_MSG: {
            // Forward child message to target
            std::string plugin = msg.payload.value("plugin", "");
            std::string target = msg.payload.value("target", "");
            auto data = msg.payload.value("data", tea::json::object());

            if (target == node_id_ || target.empty()) {
                // For local plugins
                plugin_mgr_->sendToPlugin(plugin, data);
            } else {
                // Forward to other client
                auto target_conn = tcp_server_->getConnection(target);
                if (target_conn) {
                    target_conn->send(msg);
                }
            }
            break;
        }

        case tea::MsgType::UPDATE_BINARY: {
            spdlog::info("Update binary request received");
            break;
        }

        case tea::MsgType::DISCONNECT: {
            std::string node_id = msg.payload.value("node_id", "");
            spdlog::info("Client {} disconnecting gracefully", node_id);
            break;
        }

        default:
            spdlog::debug("Unhandled message type: 0x{:04x}", static_cast<uint16_t>(msg.type));
            break;
    }
}

void LemonTea::onClientDisconnect(tea::TcpConnection::Ptr conn) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = fd_to_node_.find(conn->fd());
    if (it != fd_to_node_.end()) {
        std::string node_id = it->second;
        fd_to_node_.erase(it);

        bool still_connected = false;
        for (const auto& [fd, mapped_node] : fd_to_node_) {
            if (mapped_node == node_id) {
                still_connected = true;
                break;
            }
        }

        auto cit = clients_.find(node_id);
        if (cit != clients_.end()) {
            cit->second.connected = still_connected;
        }

        if (still_connected) {
            spdlog::warn("Client {} disconnected on one socket, but another socket is still active", node_id);
        } else {
            spdlog::info("Client disconnected: {}", node_id);
        }
    }
}

void LemonTea::heartbeatCheckLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        int64_t now = tea::nowMs();
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& [id, client] : clients_) {
            if (client.connected &&
                (now - client.last_heartbeat) > heartbeat_timeout_ * 1000) {
                spdlog::warn("Client {} heartbeat timeout", id);
                client.connected = false;
            }
        }
    }
}

void LemonTea::handlePluginOutput(const std::string& plugin, const tea::json& msg) {
    std::string event = msg.value("event", msg.value("action", std::string("")));

    spdlog::debug("Plugin '{}' output: event='{}', payload={}", plugin, event,
                  msg.dump(-1, ' ', false, tea::json::error_handler_t::replace));

    if (event != "log") {
        recordPluginEvent(plugin, msg);
    }

    if (event == "message" || event == "plugin_message") {
        // target_plugin + target_node enables A->B plugin messaging on LemonTea or remote HoneyTea.
        std::string target_plugin = msg.value("target_plugin", msg.value("plugin", std::string("")));
        std::string target_node = msg.value("target_node", msg.value("target", std::string("")));
        auto data = msg.value("data", tea::json::object());
        if (!data.is_object()) {
            data = tea::json{{"value", data}};
        }
        data["from_plugin"] = plugin;
        data["from_node"] = node_id_;

        if (target_plugin.empty()) {
            spdlog::warn("Plugin '{}' emitted message without target_plugin", plugin);
            return;
        }

        if (target_node.empty() || target_node == node_id_) {
            if (!plugin_mgr_->sendToPlugin(target_plugin, data)) {
                spdlog::warn("Failed to deliver plugin message {} -> {} on LemonTea", plugin, target_plugin);
            }
        } else {
            if (!sendPluginMessage(target_node, target_plugin, data)) {
                spdlog::warn("Failed to relay plugin message {} -> {}@{}", plugin, target_plugin, target_node);
            }
        }
    } else if (event == "ready") {
        spdlog::info("Local plugin '{}' is ready", plugin);
    } else if (event == "error") {
        spdlog::error("Local plugin '{}' error: {}", plugin, msg.value("message", "unknown"));
    } else if (event == "log") {
        spdlog::info("[plugin:{}] {}", plugin, msg.value("message", ""));
    } else if (!event.empty()) {
        spdlog::debug("Local plugin '{}' event '{}': {}", plugin, event,
                      msg.dump(-1, ' ', false, tea::json::error_handler_t::replace));
    }
}

void LemonTea::recordPluginEvent(const std::string& plugin, const tea::json& msg) {
    std::lock_guard<std::mutex> lock(plugin_events_mutex_);
    PluginEvent event;
    event.seq = ++plugin_event_seq_;
    event.timestamp = tea::nowMs();
    event.plugin = plugin;
    event.data = msg;

    plugin_events_.push_back(std::move(event));
    while (plugin_events_.size() > plugin_event_capacity_) {
        plugin_events_.pop_front();
    }
    plugin_events_cv_.notify_all();
}

bool LemonTea::startRemotePlugin(const std::string& node_id, const std::string& name) {
    auto conn = tcp_server_->getConnection(node_id);
    if (!conn) return false;
    conn->send(tea::Message::makePluginStart(name));
    return true;
}

bool LemonTea::stopRemotePlugin(const std::string& node_id, const std::string& name) {
    auto conn = tcp_server_->getConnection(node_id);
    if (!conn) return false;
    conn->send(tea::Message::makePluginStop(name));
    return true;
}

bool LemonTea::uninstallRemotePlugin(const std::string& node_id, const std::string& name) {
    auto conn = tcp_server_->getConnection(node_id);
    if (!conn) return false;
    conn->send({tea::MsgType::PLUGIN_UNINSTALL, {{"name", name}}});
    return true;
}

void LemonTea::requestRemotePluginList(const std::string& node_id) {
    auto conn = tcp_server_->getConnection(node_id);
    if (conn) {
        conn->send(tea::Message::makePluginListReq());
    }
}

bool LemonTea::installRemotePlugin(const std::string& node_id, const std::string& name,
                                    const std::vector<uint8_t>& data) {
    auto conn = tcp_server_->getConnection(node_id);
    if (!conn) return false;

    {
        std::lock_guard<std::mutex> lock(remote_install_mutex_);
        remote_install_results_.erase(remoteInstallKey(node_id, name));
    }

    // Send install command
    conn->send({tea::MsgType::PLUGIN_INSTALL, {{"name", name}}});

    // Send file transfer and keep transfer_id consistent across begin/data/end.
    auto begin = tea::Message::makeFileTransferBegin(name + ".tar.gz", data.size());
    std::string transfer_id = begin.payload.value("transfer_id", std::string(""));
    conn->send(begin);

    // Send data in chunks
    constexpr size_t CHUNK_SIZE = 32768;
    for (size_t offset = 0; offset < data.size(); offset += CHUNK_SIZE) {
        size_t chunk_len = std::min(CHUNK_SIZE, data.size() - offset);
        std::vector<uint8_t> chunk(data.begin() + offset, data.begin() + offset + chunk_len);
        conn->send(tea::Message::makeFileTransferData(transfer_id, chunk, offset));
    }

    conn->send(tea::Message::makeFileTransferEnd(transfer_id));

    const auto key = remoteInstallKey(node_id, name);
    std::unique_lock<std::mutex> lock(remote_install_mutex_);
    const auto got_ack = remote_install_cv_.wait_for(
        lock,
        std::chrono::milliseconds(8000),
        [&]() {
            auto it = remote_install_results_.find(key);
            return it != remote_install_results_.end() && it->second.ready;
        });

    if (!got_ack) {
        spdlog::warn("Remote plugin install timeout: node={}, plugin={}", node_id, name);
        return false;
    }

    auto it = remote_install_results_.find(key);
    if (it == remote_install_results_.end()) {
        return false;
    }

    const bool success = it->second.success;
    const std::string error = it->second.error;
    remote_install_results_.erase(it);

    if (!success) {
        spdlog::warn("Remote plugin install failed: node={}, plugin={}, error={}",
                     node_id, name, error.empty() ? "unknown" : error);
    }

    return success;
}

std::vector<LemonTea::ClientInfo> LemonTea::getClients() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::vector<ClientInfo> result;
    for (const auto& [id, info] : clients_) {
        result.push_back(info);
    }
    return result;
}

bool LemonTea::sendPluginMessage(const std::string& node_id, const std::string& plugin,
                                  const tea::json& data) {
    if (node_id == node_id_ || node_id.empty()) {
        return plugin_mgr_->sendToPlugin(plugin, data);
    }

    auto conn = tcp_server_->getConnection(node_id);
    if (!conn) return false;
    conn->send(tea::Message::makeChildMsg(plugin, node_id, data));
    return true;
}

tea::json LemonTea::getPluginEvents(const std::string& plugin, uint64_t after_seq, size_t limit) const {
    if (limit == 0) {
        limit = 100;
    }
    limit = std::min(limit, static_cast<size_t>(1000));

    tea::json events = tea::json::array();

    std::lock_guard<std::mutex> lock(plugin_events_mutex_);
    for (const auto& event : plugin_events_) {
        if (event.seq <= after_seq) {
            continue;
        }
        if (!plugin.empty() && event.plugin != plugin) {
            continue;
        }

        events.push_back({
            {"seq", event.seq},
            {"timestamp", event.timestamp},
            {"plugin", event.plugin},
            {"data", event.data}
        });

        if (events.size() >= limit) {
            break;
        }
    }

    return tea::json{{"events", events}, {"next_seq", plugin_event_seq_}};
}

uint64_t LemonTea::currentPluginEventSeq() const {
    std::lock_guard<std::mutex> lock(plugin_events_mutex_);
    return plugin_event_seq_;
}

bool LemonTea::waitPluginEventResponse(const std::string& plugin,
                                       const std::string& request_id,
                                       const std::vector<std::string>& expected_actions,
                                       uint64_t after_seq,
                                       int timeout_ms,
                                       tea::json& response,
                                       uint64_t* response_seq) const {
    std::unordered_set<std::string> action_set(expected_actions.begin(), expected_actions.end());

    const auto matches = [&](const PluginEvent& event) {
        if (event.seq <= after_seq) {
            return false;
        }
        if (!plugin.empty() && event.plugin != plugin) {
            return false;
        }

        if (!request_id.empty()) {
            if (!event.data.contains("request_id") ||
                event.data.value("request_id", std::string("")) != request_id) {
                return false;
            }
        }

        if (!action_set.empty()) {
            const std::string action = event.data.value("action", event.data.value("event", std::string("")));
            if (action_set.count(action) == 0) {
                return false;
            }
        }

        return true;
    };

    auto findMatching = [&](tea::json& out, uint64_t* out_seq) {
        for (const auto& event : plugin_events_) {
            if (!matches(event)) {
                continue;
            }
            out = event.data;
            if (out_seq) {
                *out_seq = event.seq;
            }
            return true;
        }
        return false;
    };

    std::unique_lock<std::mutex> lock(plugin_events_mutex_);
    if (findMatching(response, response_seq)) {
        return true;
    }

    if (timeout_ms < 0) {
        timeout_ms = 0;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (true) {
        if (plugin_events_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            return findMatching(response, response_seq);
        }
        if (findMatching(response, response_seq)) {
            return true;
        }
    }
}

bool LemonTea::requestSelfUpdate(const std::string& new_binary_path) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(greentea_port_);
    inet_pton(AF_INET, greentea_host_.c_str(), &addr.sin_addr);

    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(fd);
        spdlog::error("Cannot connect to GreenTea for self-update");
        return false;
    }

    std::ifstream in(new_binary_path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) { ::close(fd); return false; }
    size_t size = in.tellg();
    in.seekg(0);
    std::vector<char> data(size);
    in.read(data.data(), size);
    in.close();

    tea::json header = {{"name", "LemonTea"}, {"size", size}};
    std::string header_str = header.dump() + "\n";
    ::send(fd, header_str.data(), header_str.size(), 0);

    size_t sent = 0;
    while (sent < size) {
        ssize_t n = ::send(fd, data.data() + sent, size - sent, 0);
        if (n <= 0) break;
        sent += n;
    }

    char buf[1024];
    ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    ::close(fd);

    if (n > 0) {
        buf[n] = '\0';
        try {
            auto resp = tea::json::parse(buf);
            return resp.value("status", "") == "ok";
        } catch (...) {}
    }
    return false;
}
