#include "lemon_tea.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
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
    spdlog::info("LemonTea shutting down...");
    running_.store(false);

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
        auto cit = clients_.find(node_id);
        if (cit != clients_.end()) {
            cit->second.connected = false;
        }
        fd_to_node_.erase(it);
        spdlog::info("Client disconnected: {}", node_id);
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
    std::string event = msg.value("event", "");
    if (event == "message") {
        std::string target = msg.value("target", "");
        auto data = msg.value("data", tea::json::object());
        sendPluginMessage(target, plugin, data);
    } else if (event == "ready") {
        spdlog::info("Local plugin '{}' is ready", plugin);
    } else if (event == "error") {
        spdlog::error("Local plugin '{}' error: {}", plugin, msg.value("message", "unknown"));
    }
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

    // Send install command
    conn->send({tea::MsgType::PLUGIN_INSTALL, {{"name", name}}});

    // Send file transfer
    std::string transfer_id = std::to_string(tea::nowMs());
    conn->send(tea::Message::makeFileTransferBegin(name + ".tar.gz", data.size()));

    // Send data in chunks
    constexpr size_t CHUNK_SIZE = 32768;
    for (size_t offset = 0; offset < data.size(); offset += CHUNK_SIZE) {
        size_t chunk_len = std::min(CHUNK_SIZE, data.size() - offset);
        std::vector<uint8_t> chunk(data.begin() + offset, data.begin() + offset + chunk_len);
        conn->send(tea::Message::makeFileTransferData(transfer_id, chunk, offset));
    }

    conn->send(tea::Message::makeFileTransferEnd(transfer_id));
    return true;
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
