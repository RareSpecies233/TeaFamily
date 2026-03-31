#include "updater.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

Updater::Updater(uint16_t port) : port_(port) {
    fs::create_directories(temp_dir_);
}

Updater::~Updater() {
    stop();
}

bool Updater::start() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        spdlog::error("Updater: socket creation failed: {}", strerror(errno));
        return false;
    }

    int on = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // Only localhost

    if (::bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        spdlog::error("Updater: bind failed on port {}: {}", port_, strerror(errno));
        ::close(listen_fd_);
        return false;
    }

    if (::listen(listen_fd_, 4) < 0) {
        spdlog::error("Updater: listen failed: {}", strerror(errno));
        ::close(listen_fd_);
        return false;
    }

    running_.store(true);
    accept_thread_ = std::thread(&Updater::acceptLoop, this);
    spdlog::info("Updater listening on 127.0.0.1:{}", port_);
    return true;
}

void Updater::stop() {
    running_.store(false);
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

void Updater::acceptLoop() {
    while (running_.load()) {
        struct pollfd pfd;
        pfd.fd = listen_fd_;
        pfd.events = POLLIN;
        int ret = ::poll(&pfd, 1, 500);
        if (ret <= 0) continue;

        int client = ::accept(listen_fd_, nullptr, nullptr);
        if (client < 0) continue;

        // Handle in a detached thread
        std::thread([this, client]() {
            handleClient(client);
        }).detach();
    }
}

void Updater::handleClient(int client_fd) {
    // Protocol:
    // 1. Client sends JSON header: {"name":"LemonTea","size":12345}\n
    // 2. Client sends binary data of 'size' bytes
    // 3. Server responds: {"status":"ok"}\n or {"status":"error","message":"..."}\n

    // Read header line
    std::string header_line;
    char c;
    while (::recv(client_fd, &c, 1, 0) == 1) {
        if (c == '\n') break;
        header_line += c;
        if (header_line.size() > 4096) {
            ::close(client_fd);
            return;
        }
    }

    try {
        auto header = json::parse(header_line);
        std::string name = header.value("name", "");
        size_t size = header.value("size", 0);

        if (name.empty() || size == 0 || size > 512 * 1024 * 1024) {
            std::string resp = R"({"status":"error","message":"invalid header"})" "\n";
            ::send(client_fd, resp.data(), resp.size(), 0);
            ::close(client_fd);
            return;
        }

        spdlog::info("Updater: receiving binary for '{}', size: {} bytes", name, size);

        // Receive binary data
        std::string temp_path = temp_dir_ + "/" + name + "_update";
        std::ofstream out(temp_path, std::ios::binary);
        if (!out.is_open()) {
            std::string resp = R"({"status":"error","message":"cannot create temp file"})" "\n";
            ::send(client_fd, resp.data(), resp.size(), 0);
            ::close(client_fd);
            return;
        }

        size_t received = 0;
        char buf[65536];
        while (received < size) {
            size_t to_read = std::min(sizeof(buf), size - received);
            ssize_t n = ::recv(client_fd, buf, to_read, 0);
            if (n <= 0) break;
            out.write(buf, n);
            received += n;
        }
        out.close();

        if (received != size) {
            spdlog::error("Updater: incomplete transfer for '{}': {}/{}", name, received, size);
            fs::remove(temp_path);
            std::string resp = R"({"status":"error","message":"incomplete transfer"})" "\n";
            ::send(client_fd, resp.data(), resp.size(), 0);
            ::close(client_fd);
            return;
        }

        // Make executable
        fs::permissions(temp_path, fs::perms::owner_exec | fs::perms::group_exec,
                       fs::perm_options::add);

        // Invoke callback
        bool ok = false;
        if (on_update_) {
            ok = on_update_(name, temp_path);
        }

        std::string resp;
        if (ok) {
            resp = R"({"status":"ok"})" "\n";
            spdlog::info("Updater: binary update successful for '{}'", name);
        } else {
            resp = R"({"status":"error","message":"update failed"})" "\n";
            spdlog::error("Updater: binary update failed for '{}'", name);
        }
        ::send(client_fd, resp.data(), resp.size(), 0);

    } catch (const std::exception& e) {
        spdlog::error("Updater: error handling client: {}", e.what());
        std::string resp = R"({"status":"error","message":"server error"})" "\n";
        ::send(client_fd, resp.data(), resp.size(), 0);
    }

    ::close(client_fd);
}
