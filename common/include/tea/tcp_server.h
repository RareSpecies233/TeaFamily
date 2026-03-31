#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include "tea/tcp_connection.h"

namespace tea {

class TcpServer {
public:
    using ConnectionCallback = std::function<void(TcpConnection::Ptr)>;
    using MessageCallback = TcpConnection::MessageCallback;

    TcpServer(uint16_t port, const std::string& bind_addr = "0.0.0.0");
    ~TcpServer();

    void setConnectionCallback(ConnectionCallback cb) { on_connect_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { on_message_ = std::move(cb); }
    void setDisconnectCallback(TcpConnection::CloseCallback cb) { on_disconnect_ = std::move(cb); }

    bool start();
    void stop();

    // Broadcast to all connections
    void broadcast(const Message& msg);

    // Get all connections
    std::vector<TcpConnection::Ptr> connections() const;

    // Get connection by peer address
    TcpConnection::Ptr getConnection(const std::string& peer_id) const;

    bool isRunning() const { return running_.load(); }

private:
    void acceptLoop();

    uint16_t port_;
    std::string bind_addr_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;

    ConnectionCallback on_connect_;
    MessageCallback on_message_;
    TcpConnection::CloseCallback on_disconnect_;

    mutable std::mutex conns_mutex_;
    std::map<int, TcpConnection::Ptr> connections_;
};

} // namespace tea
