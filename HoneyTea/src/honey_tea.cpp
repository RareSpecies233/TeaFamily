#include "honey_tea.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

uint8_t fromHex(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + c - 'a');
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + c - 'A');
    return 0;
}

std::vector<uint8_t> decodeHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        return {};
    }
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        uint8_t hi = fromHex(hex[i]);
        uint8_t lo = fromHex(hex[i + 1]);
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

std::string inferPluginNameFromArchive(const std::string& filename) {
    if (filename.empty()) {
        return {};
    }
    std::string name = filename;
    constexpr const char* TAR_GZ = ".tar.gz";
    constexpr const char* TGZ = ".tgz";
    if (name.size() > std::strlen(TAR_GZ) &&
        name.compare(name.size() - std::strlen(TAR_GZ), std::strlen(TAR_GZ), TAR_GZ) == 0) {
        name.resize(name.size() - std::strlen(TAR_GZ));
    } else if (name.size() > std::strlen(TGZ) &&
               name.compare(name.size() - std::strlen(TGZ), std::strlen(TGZ), TGZ) == 0) {
        name.resize(name.size() - std::strlen(TGZ));
    } else {
        name = fs::path(name).stem().string();
    }
    return name;
}

}  // namespace

HoneyTea::HoneyTea() {}

HoneyTea::~HoneyTea() {
    shutdown();
}

bool HoneyTea::init(const tea::Config& config) {
    config_ = config;
    node_id_ = config.get<std::string>("node_id", "honey-001");
    node_name_ = config.get<std::string>("node_name", "HoneyTea-Default");
    server_host_ = config.get<std::string>("server_host", "127.0.0.1");
    server_port_ = config.get<int>("server_port", tea::DEFAULT_TCP_PORT);
    udp_port_ = config.get<int>("udp_port", tea::DEFAULT_UDP_BASE_PORT);
    greentea_host_ = config.get<std::string>("greentea_host", "127.0.0.1");
    greentea_port_ = config.get<int>("greentea_port", tea::DEFAULT_GREENTEA_PORT);
    heartbeat_interval_ = config.get<int>("heartbeat_interval_sec", 5);
    reconnect_interval_ = config.get<int>("reconnect_interval_sec", 3);

    // Setup plugins
    std::string plugins_dir = config.get<std::string>("plugins_dir", "plugins");
    plugin_mgr_ = std::make_unique<PluginManager>(plugins_dir);
    plugin_mgr_->scanPlugins();
    plugin_mgr_->setPluginOutputCallback(
        [this](const std::string& plugin, const tea::json& msg) {
            handlePluginOutput(plugin, msg);
        }
    );

    // Setup UDP
    udp_socket_ = std::make_unique<tea::UdpSocket>();
    if (udp_socket_->bind(udp_port_)) {
        udp_socket_->setRecvCallback(
            [this](const std::string& addr, uint16_t port, const std::vector<uint8_t>& data) {
                // Forward UDP data to server if connected
                if (server_conn_ && server_conn_->isConnected()) {
                    tea::json payload = {
                        {"from_addr", addr},
                        {"from_port", port},
                        {"data", std::string(data.begin(), data.end())}
                    };
                    server_conn_->send({tea::MsgType::CHILD_MSG_UDP, payload});
                }
            }
        );
        udp_socket_->startRecv();
    }

    spdlog::info("HoneyTea initialized: id={}, server={}:{}", node_id_, server_host_, server_port_);
    return true;
}

void HoneyTea::run() {
    running_.store(true);

    // Start plugin monitor
    plugin_mgr_->startMonitor();

    // Auto-start plugins
    for (const auto& p : plugin_mgr_->getPlugins()) {
        if (p.auto_start) {
            plugin_mgr_->startPlugin(p.name);
        }
    }

    // Connect to server
    connectToServer();

    // Start heartbeat
    heartbeat_thread_ = std::thread(&HoneyTea::heartbeatLoop, this);

    // Start reconnect loop
    reconnect_thread_ = std::thread(&HoneyTea::reconnectLoop, this);

    spdlog::info("HoneyTea is running");
}

void HoneyTea::shutdown() {
    if (!running_.exchange(false)) {
        return;
    }

    spdlog::info("HoneyTea shutting down...");

    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    if (reconnect_thread_.joinable()) reconnect_thread_.join();

    // Stop plugins
    if (plugin_mgr_) {
        plugin_mgr_->stopAll();
        plugin_mgr_->stopMonitor();
    }

    // Disconnect
    if (server_conn_) {
        server_conn_->send({tea::MsgType::DISCONNECT, {{"node_id", node_id_}}});
        server_conn_->stop();
        server_conn_.reset();
    }

    if (udp_socket_) {
        udp_socket_->stopRecv();
    }

    spdlog::info("HoneyTea stopped");
}

void HoneyTea::connectToServer() {
    spdlog::info("Connecting to LemonTea at {}:{}", server_host_, server_port_);
    auto conn = tea::TcpConnection::connect(server_host_, server_port_);
    if (!conn) {
        spdlog::error("Failed to connect to server");
        connected_.store(false);
        return;
    }

    server_conn_ = conn;
    server_conn_->setMessageCallback(
        [this](tea::TcpConnection::Ptr c, const tea::Message& msg) {
            onMessage(c, msg);
        }
    );
    server_conn_->setCloseCallback(
        [this](tea::TcpConnection::Ptr c) {
            onDisconnect(c);
        }
    );
    server_conn_->start();

    // Send handshake
    server_conn_->send(tea::Message::makeHandshake(node_id_, "HoneyTea"));
    connected_.store(true);
    spdlog::info("Connected to LemonTea");
}

void HoneyTea::onMessage(tea::TcpConnection::Ptr conn, const tea::Message& msg) {
    switch (msg.type) {
        case tea::MsgType::HANDSHAKE_ACK:
            spdlog::info("Handshake acknowledged by server");
            break;

        case tea::MsgType::HEARTBEAT_ACK:
            break;

        case tea::MsgType::PLUGIN_LIST_REQ: {
            auto plugins = plugin_mgr_->getPlugins();
            tea::json list = tea::json::array();
            for (const auto& p : plugins) {
                list.push_back({
                    {"name", p.name}, {"version", p.version},
                    {"plugin_type", p.plugin_type},
                    {"state", tea::pluginStateStr(p.state)},
                    {"pid", p.pid}, {"has_frontend", p.has_frontend}
                });
            }
            conn->send(tea::Message::makePluginListRsp(list));
            break;
        }

        case tea::MsgType::PLUGIN_START: {
            std::string name = msg.payload.value("name", "");
            bool ok = plugin_mgr_->startPlugin(name);
            conn->send({tea::MsgType::PLUGIN_START_ACK,
                       {{"name", name}, {"success", ok}}});
            break;
        }

        case tea::MsgType::PLUGIN_STOP: {
            std::string name = msg.payload.value("name", "");
            bool ok = plugin_mgr_->stopPlugin(name);
            conn->send({tea::MsgType::PLUGIN_STOP_ACK,
                       {{"name", name}, {"success", ok}}});
            break;
        }

        case tea::MsgType::PLUGIN_UNINSTALL: {
            std::string name = msg.payload.value("name", "");
            bool ok = plugin_mgr_->uninstallPlugin(name);
            conn->send({tea::MsgType::PLUGIN_STOP_ACK,
                       {{"name", name}, {"success", ok}, {"op", "uninstall"}}});
            break;
        }

        case tea::MsgType::PLUGIN_INSTALL: {
            std::string name = msg.payload.value("name", "");
            // This triggers a file transfer flow
            spdlog::info("Plugin install request received: {}", name);
            {
                std::lock_guard<std::mutex> lock(transfer_mutex_);
                pending_install_name_ = name;
            }
            conn->send({tea::MsgType::PLUGIN_INSTALL_ACK,
                       {{"name", name}, {"ready", true}}});
            break;
        }

        case tea::MsgType::CHILD_MSG: {
            std::string plugin = msg.payload.value("plugin", "");
            auto data = msg.payload.value("data", tea::json::object());
            plugin_mgr_->sendToPlugin(plugin, data);
            break;
        }

        case tea::MsgType::FILE_TRANSFER_BEGIN: {
            const std::string transfer_id = msg.payload.value("transfer_id", std::string(""));
            const std::string filename = msg.payload.value("filename", std::string(""));
            const size_t total_size = msg.payload.value("total_size", static_cast<size_t>(0));

            if (transfer_id.empty()) {
                spdlog::error("FILE_TRANSFER_BEGIN missing transfer_id");
                conn->send(tea::Message::makeError("missing transfer_id"));
                break;
            }

            IncomingTransfer transfer;
            {
                std::lock_guard<std::mutex> lock(transfer_mutex_);
                transfer.plugin_name = pending_install_name_;
                if (transfer.plugin_name.empty()) {
                    transfer.plugin_name = inferPluginNameFromArchive(filename);
                }
            }
            transfer.total_size = total_size;
            if (total_size > 0) {
                transfer.data.reserve(total_size);
            }
            const std::string plugin_name_for_log = transfer.plugin_name;

            {
                std::lock_guard<std::mutex> lock(transfer_mutex_);
                incoming_transfers_[transfer_id] = std::move(transfer);
            }

            spdlog::info("File transfer starting: transfer_id={}, plugin={}, filename={}, total_size={}",
                         transfer_id, plugin_name_for_log, filename, total_size);
            conn->send({tea::MsgType::FILE_TRANSFER_ACK,
                       {{"transfer_id", transfer_id}}});
            break;
        }

        case tea::MsgType::FILE_TRANSFER_DATA: {
            const std::string transfer_id = msg.payload.value("transfer_id", std::string(""));
            const size_t offset = msg.payload.value("offset", static_cast<size_t>(0));
            const std::string hex_data = msg.payload.value("data", std::string(""));

            auto chunk = decodeHex(hex_data);
            if (chunk.empty() && !hex_data.empty()) {
                spdlog::warn("Received invalid hex payload for transfer {}", transfer_id);
                break;
            }

            std::lock_guard<std::mutex> lock(transfer_mutex_);
            auto it = incoming_transfers_.find(transfer_id);
            if (it == incoming_transfers_.end()) {
                spdlog::warn("FILE_TRANSFER_DATA for unknown transfer_id={}", transfer_id);
                break;
            }

            auto& transfer = it->second;
            const size_t required = offset + chunk.size();
            if (transfer.data.size() < required) {
                transfer.data.resize(required);
            }
            std::copy(chunk.begin(), chunk.end(), transfer.data.begin() + static_cast<std::ptrdiff_t>(offset));
            transfer.received_size = std::max(transfer.received_size, required);
            break;
        }

        case tea::MsgType::FILE_TRANSFER_END: {
            std::string transfer_id = msg.payload.value("transfer_id", "");

            IncomingTransfer transfer;
            {
                std::lock_guard<std::mutex> lock(transfer_mutex_);
                auto it = incoming_transfers_.find(transfer_id);
                if (it == incoming_transfers_.end()) {
                    spdlog::warn("FILE_TRANSFER_END for unknown transfer_id={}", transfer_id);
                    break;
                }
                transfer = std::move(it->second);
                incoming_transfers_.erase(it);
                pending_install_name_.clear();
            }

            if (transfer.plugin_name.empty()) {
                spdlog::error("Cannot install plugin: transfer {} has empty plugin_name", transfer_id);
                conn->send({tea::MsgType::PLUGIN_INSTALL_ACK,
                           {{"name", ""}, {"success", false}, {"error", "missing plugin name"}}});
                break;
            }

            if (transfer.received_size < transfer.total_size) {
                spdlog::warn("Transfer {} incomplete: received={} expected={}",
                             transfer_id, transfer.received_size, transfer.total_size);
            }

            if (transfer.received_size < transfer.data.size()) {
                transfer.data.resize(transfer.received_size);
            }

            const bool ok = plugin_mgr_->installPlugin(transfer.plugin_name, transfer.data);
            spdlog::info("File transfer completed: transfer_id={}, plugin={}, install={}",
                         transfer_id, transfer.plugin_name, ok ? "ok" : "failed");
            conn->send({tea::MsgType::PLUGIN_INSTALL_ACK,
                       {{"name", transfer.plugin_name}, {"success", ok}}});
            break;
        }

        default:
            spdlog::debug("Unhandled message type: 0x{:04x}", static_cast<uint16_t>(msg.type));
            break;
    }
}

void HoneyTea::onDisconnect(tea::TcpConnection::Ptr conn) {
    spdlog::warn("Disconnected from server");
    connected_.store(false);
}

void HoneyTea::heartbeatLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(heartbeat_interval_));
        if (server_conn_ && server_conn_->isConnected()) {
            server_conn_->send(tea::Message::makeHeartbeat());
        }
    }
}

void HoneyTea::reconnectLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(reconnect_interval_));
        if (!connected_.load() && running_.load()) {
            spdlog::info("Attempting to reconnect...");
            connectToServer();
        }
    }
}

void HoneyTea::handlePluginOutput(const std::string& plugin, const tea::json& msg) {
    std::string event = msg.value("event", msg.value("action", std::string("")));

    spdlog::debug("Plugin '{}' output: event='{}', payload={}", plugin, event,
                  msg.dump(-1, ' ', false, tea::json::error_handler_t::replace));

    if (event == "message" || event == "plugin_message") {
        // Forward to server
        std::string target = msg.value("target_node", msg.value("target", std::string("")));
        std::string target_plugin = msg.value("target_plugin", msg.value("plugin", plugin));
        auto data = msg.value("data", tea::json::object());
        if (!data.is_object()) {
            data = tea::json{{"value", data}};
        }
        data["from_plugin"] = plugin;
        data["from_node"] = node_id_;
        if (server_conn_ && server_conn_->isConnected()) {
            server_conn_->send(tea::Message::makeChildMsg(target_plugin, target, data));
        }
    } else if (!event.empty() && event != "ready" && event != "error" && event != "log") {
        // Forward plugin action output back to LemonTea so frontend can consume
        // SSH stream output, file manager result payloads, and monitor metrics.
        std::string target = msg.value("target_node", msg.value("target", std::string("")));
        std::string target_plugin = msg.value("target_plugin", plugin);
        auto data = msg;
        if (!data.is_object()) {
            data = tea::json{{"value", data}};
        }
        data["from_plugin"] = plugin;
        data["from_node"] = node_id_;

        if (server_conn_ && server_conn_->isConnected()) {
            server_conn_->send(tea::Message::makeChildMsg(target_plugin, target, data));
        }
    } else if (event == "ready") {
        spdlog::info("Plugin '{}' is ready", plugin);
    } else if (event == "error") {
        spdlog::error("Plugin '{}' error: {}", plugin, msg.value("message", "unknown"));
    } else if (event == "log") {
        spdlog::info("[plugin:{}] {}", plugin, msg.value("message", ""));
    } else if (!event.empty()) {
        spdlog::debug("Plugin '{}' event '{}': {}", plugin, event,
                      msg.dump(-1, ' ', false, tea::json::error_handler_t::replace));
    }
}

bool HoneyTea::requestSelfUpdate(const std::string& new_binary_path) {
    // Connect to local GreenTea and send the new binary
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

    // Read binary file
    std::ifstream in(new_binary_path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        ::close(fd);
        return false;
    }
    size_t size = in.tellg();
    in.seekg(0);
    std::vector<char> data(size);
    in.read(data.data(), size);
    in.close();

    // Send header
    tea::json header = {{"name", "HoneyTea"}, {"size", size}};
    std::string header_str = header.dump() + "\n";
    ::send(fd, header_str.data(), header_str.size(), 0);

    // Send data
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = ::send(fd, data.data() + sent, size - sent, 0);
        if (n <= 0) break;
        sent += n;
    }

    // Read response
    char buf[1024];
    ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    ::close(fd);

    if (n > 0) {
        buf[n] = '\0';
        try {
            auto resp = tea::json::parse(buf);
            if (resp.value("status", "") == "ok") {
                spdlog::info("Self-update request sent successfully");
                return true;
            }
        } catch (...) {}
    }

    spdlog::error("Self-update request failed");
    return false;
}
