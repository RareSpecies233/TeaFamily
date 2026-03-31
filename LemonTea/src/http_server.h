#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>

class LemonTea;

class HttpApi {
public:
    HttpApi(LemonTea& lemon, uint16_t port);
    ~HttpApi();

    bool start();
    void stop();

private:
    void setupRoutes();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    LemonTea& lemon_;
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
};
