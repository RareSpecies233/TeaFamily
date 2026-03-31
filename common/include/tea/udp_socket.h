#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace tea {

class UdpSocket {
public:
    using RecvCallback = std::function<void(const std::string& from_addr, uint16_t from_port,
                                            const std::vector<uint8_t>& data)>;

    UdpSocket();
    ~UdpSocket();

    bool bind(uint16_t port, const std::string& addr = "0.0.0.0");
    bool sendTo(const std::string& addr, uint16_t port, const std::vector<uint8_t>& data);
    bool sendTo(const std::string& addr, uint16_t port, const std::string& data);

    void setRecvCallback(RecvCallback cb) { on_recv_ = std::move(cb); }

    // Start receiving in background
    void startRecv();
    void stopRecv();

    int fd() const { return fd_; }
    bool isBound() const { return fd_ >= 0; }

private:
    void recvLoop();

    int fd_ = -1;
    std::atomic<bool> receiving_{false};
    std::thread recv_thread_;
    RecvCallback on_recv_;
};

} // namespace tea
