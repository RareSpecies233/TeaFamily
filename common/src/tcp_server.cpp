#include "tea/tcp_server.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

namespace tea {

TcpServer::TcpServer(uint16_t port, const std::string& bind_addr)
    : port_(port), bind_addr_(bind_addr) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        spdlog::error("TcpServer: socket creation failed: {}", strerror(errno));
        return false;
    }

    int on = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    inet_pton(AF_INET, bind_addr_.c_str(), &addr.sin_addr);

    if (::bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        spdlog::error("TcpServer: bind failed on port {}: {}", port_, strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (::listen(listen_fd_, 16) < 0) {
        spdlog::error("TcpServer: listen failed: {}", strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    running_.store(true);
    accept_thread_ = std::thread(&TcpServer::acceptLoop, this);

    spdlog::info("TcpServer listening on {}:{}", bind_addr_, port_);
    return true;
}

void TcpServer::stop() {
    running_.store(false);
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    std::lock_guard<std::mutex> lock(conns_mutex_);
    for (auto& [fd, conn] : connections_) {
        conn->stop();
    }
    connections_.clear();
}

void TcpServer::broadcast(const Message& msg) {
    std::lock_guard<std::mutex> lock(conns_mutex_);
    for (auto& [fd, conn] : connections_) {
        if (conn->isConnected()) {
            conn->send(msg);
        }
    }
}

std::vector<TcpConnection::Ptr> TcpServer::connections() const {
    std::lock_guard<std::mutex> lock(conns_mutex_);
    std::vector<TcpConnection::Ptr> result;
    for (const auto& [fd, conn] : connections_) {
        result.push_back(conn);
    }
    return result;
}

TcpConnection::Ptr TcpServer::getConnection(const std::string& peer_id) const {
    std::lock_guard<std::mutex> lock(conns_mutex_);
    for (const auto& [fd, conn] : connections_) {
        if (conn->getUserData("node_id") == peer_id) {
            return conn;
        }
    }
    return nullptr;
}

void TcpServer::acceptLoop() {
    while (running_.load()) {
        struct pollfd pfd;
        pfd.fd = listen_fd_;
        pfd.events = POLLIN;
        int ret = ::poll(&pfd, 1, 500);
        if (ret <= 0) continue;

        struct sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd_, (struct sockaddr*)&client_addr, &len);
        if (client_fd < 0) {
            if (errno != EINTR && errno != EAGAIN) {
                spdlog::error("Accept failed: {}", strerror(errno));
            }
            continue;
        }

        char addr_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_buf, sizeof(addr_buf));
        uint16_t client_port = ntohs(client_addr.sin_port);

        // Enable TCP keep-alive and disable Nagle
        int on = 1;
        setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

        auto conn = std::make_shared<TcpConnection>(client_fd, addr_buf, client_port);
        conn->setMessageCallback(on_message_);
        conn->setCloseCallback([this](TcpConnection::Ptr c) {
            spdlog::info("Client disconnected: {}:{}", c->peerAddr(), c->peerPort());
            {
                std::lock_guard<std::mutex> lock(conns_mutex_);
                connections_.erase(c->fd());
            }
            if (on_disconnect_) on_disconnect_(c);
        });

        {
            std::lock_guard<std::mutex> lock(conns_mutex_);
            connections_[client_fd] = conn;
        }

        conn->start();
        spdlog::info("New connection from {}:{}", addr_buf, client_port);

        if (on_connect_) on_connect_(conn);
    }
}

} // namespace tea
