#include "tea/udp_socket.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <cstring>

namespace tea {

UdpSocket::UdpSocket() {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        spdlog::error("UDP socket creation failed: {}", strerror(errno));
    }
}

UdpSocket::~UdpSocket() {
    stopRecv();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool UdpSocket::bind(uint16_t port, const std::string& addr) {
    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, addr.c_str(), &sa.sin_addr);

    if (::bind(fd_, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        spdlog::error("UDP bind failed on port {}: {}", port, strerror(errno));
        return false;
    }
    spdlog::info("UDP socket bound to {}:{}", addr, port);
    return true;
}

bool UdpSocket::sendTo(const std::string& addr, uint16_t port, const std::vector<uint8_t>& data) {
    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, addr.c_str(), &sa.sin_addr);

    ssize_t sent = ::sendto(fd_, data.data(), data.size(), 0,
                             (struct sockaddr*)&sa, sizeof(sa));
    return sent == static_cast<ssize_t>(data.size());
}

bool UdpSocket::sendTo(const std::string& addr, uint16_t port, const std::string& data) {
    return sendTo(addr, port, std::vector<uint8_t>(data.begin(), data.end()));
}

void UdpSocket::startRecv() {
    receiving_.store(true);
    recv_thread_ = std::thread(&UdpSocket::recvLoop, this);
}

void UdpSocket::stopRecv() {
    receiving_.store(false);
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
}

void UdpSocket::recvLoop() {
    uint8_t buf[65536];
    while (receiving_.load()) {
        struct pollfd pfd;
        pfd.fd = fd_;
        pfd.events = POLLIN;
        int ret = ::poll(&pfd, 1, 500);
        if (ret <= 0) continue;

        struct sockaddr_in from_addr{};
        socklen_t from_len = sizeof(from_addr);
        ssize_t n = ::recvfrom(fd_, buf, sizeof(buf), 0,
                               (struct sockaddr*)&from_addr, &from_len);
        if (n <= 0) continue;

        char addr_buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, addr_buf, sizeof(addr_buf));
        uint16_t from_port = ntohs(from_addr.sin_port);

        if (on_recv_) {
            std::vector<uint8_t> data(buf, buf + n);
            on_recv_(addr_buf, from_port, data);
        }
    }
}

} // namespace tea
