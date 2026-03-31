#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include "tea/protocol.h"

namespace tea {

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using Ptr = std::shared_ptr<TcpConnection>;
    using MessageCallback = std::function<void(Ptr, const Message&)>;
    using CloseCallback = std::function<void(Ptr)>;

    TcpConnection(int fd, const std::string& peer_addr, uint16_t peer_port);
    ~TcpConnection();

    // Non-copyable
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // Start reading in a background thread
    void start();
    void stop();

    // Send a message
    bool send(const Message& msg);

    // Send raw bytes
    bool sendRaw(const std::vector<uint8_t>& data);

    // Callbacks
    void setMessageCallback(MessageCallback cb) { on_message_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { on_close_ = std::move(cb); }

    // Getters
    int fd() const { return fd_; }
    const std::string& peerAddr() const { return peer_addr_; }
    uint16_t peerPort() const { return peer_port_; }
    bool isConnected() const { return connected_.load(); }

    // User data
    void setUserData(const std::string& key, const std::string& val);
    std::string getUserData(const std::string& key) const;

    // Connect to remote (static factory)
    static Ptr connect(const std::string& host, uint16_t port, int timeout_ms = 5000);

private:
    void readLoop();

    int fd_;
    std::string peer_addr_;
    uint16_t peer_port_;
    std::atomic<bool> connected_{true};

    MessageCallback on_message_;
    CloseCallback on_close_;

    std::thread read_thread_;
    std::vector<uint8_t> recv_buf_;

    mutable std::mutex data_mutex_;
    std::map<std::string, std::string> user_data_;

    std::mutex send_mutex_;
};

} // namespace tea
