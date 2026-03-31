#include "tea/tcp_connection.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

namespace tea {

TcpConnection::TcpConnection(int fd, const std::string& peer_addr, uint16_t peer_port)
    : fd_(fd), peer_addr_(peer_addr), peer_port_(peer_port) {
    recv_buf_.reserve(65536);
}

TcpConnection::~TcpConnection() {
    stop();
}

void TcpConnection::start() {
    read_thread_ = std::thread([self = shared_from_this()]() {
        self->readLoop();
    });
}

void TcpConnection::stop() {
    if (connected_.exchange(false)) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
}

bool TcpConnection::send(const Message& msg) {
    auto data = msg.serialize();
    return sendRaw(data);
}

bool TcpConnection::sendRaw(const std::vector<uint8_t>& data) {
    if (!connected_.load()) return false;
    std::lock_guard<std::mutex> lock(send_mutex_);

    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = ::send(fd_, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            if (errno == EINTR) continue;
            spdlog::error("Send failed to {}:{}: {}", peer_addr_, peer_port_, strerror(errno));
            return false;
        }
        sent += n;
    }
    return true;
}

void TcpConnection::readLoop() {
    uint8_t buf[65536];
    while (connected_.load()) {
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLIN;
        int ret = ::poll(&pfd, 1, 500);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;

        ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }

        recv_buf_.insert(recv_buf_.end(), buf, buf + n);

        // Parse messages
        while (recv_buf_.size() >= MSG_HEADER_SIZE) {
            Message msg;
            int consumed = Message::deserialize(recv_buf_.data(), recv_buf_.size(), msg);
            if (consumed == 0) break;  // incomplete
            if (consumed < 0) {
                spdlog::error("Message parse error from {}:{}", peer_addr_, peer_port_);
                recv_buf_.clear();
                break;
            }
            recv_buf_.erase(recv_buf_.begin(), recv_buf_.begin() + consumed);
            if (on_message_) {
                on_message_(shared_from_this(), msg);
            }
        }
    }

    connected_.store(false);
    if (on_close_) {
        on_close_(shared_from_this());
    }
}

void TcpConnection::setUserData(const std::string& key, const std::string& val) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    user_data_[key] = val;
}

std::string TcpConnection::getUserData(const std::string& key) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = user_data_.find(key);
    return it != user_data_.end() ? it->second : "";
}

TcpConnection::Ptr TcpConnection::connect(const std::string& host, uint16_t port, int timeout_ms) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        spdlog::error("Socket creation failed: {}", strerror(errno));
        return nullptr;
    }

    // Set non-blocking for connect
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        spdlog::error("Invalid address: {}", host);
        ::close(fd);
        return nullptr;
    }

    int ret = ::connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        spdlog::error("Connect failed to {}:{}: {}", host, port, strerror(errno));
        ::close(fd);
        return nullptr;
    }

    if (ret < 0) {
        // Wait for connection
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        ret = ::poll(&pfd, 1, timeout_ms);
        if (ret <= 0) {
            spdlog::error("Connect timeout to {}:{}", host, port);
            ::close(fd);
            return nullptr;
        }
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) {
            spdlog::error("Connect error to {}:{}: {}", host, port, strerror(err));
            ::close(fd);
            return nullptr;
        }
    }

    // Set back to blocking
    fcntl(fd, F_SETFL, flags);

    // Enable TCP keep-alive
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    // Disable Nagle
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

    spdlog::info("Connected to {}:{}", host, port);
    auto conn = std::make_shared<TcpConnection>(fd, host, port);
    return conn;
}

} // namespace tea
