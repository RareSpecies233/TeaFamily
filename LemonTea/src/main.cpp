#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <thread>
#include <unistd.h>
#include <tea/common.h>
#include <tea/config.h>
#include <tea/logger.h>
#include "lemon_tea.h"
#include "http_server.h"

namespace fs = std::filesystem;

static std::atomic<bool> g_running{true};

static void signalHandler(int sig) {
    spdlog::info("LemonTea received signal {}, shutting down...", sig);
    g_running.store(false);
}

static void startParentGuardMonitor() {
    const char* guard_fd_env = std::getenv("TEA_PARENT_GUARD_FD");
    if (!guard_fd_env || !*guard_fd_env) {
        return;
    }

    int guard_fd = -1;
    try {
        guard_fd = std::stoi(guard_fd_env);
    } catch (...) {
        return;
    }

    if (guard_fd < 0) {
        return;
    }

    std::thread([guard_fd]() {
        char byte = 0;
        while (true) {
            const ssize_t n = ::read(guard_fd, &byte, 1);
            if (n == 0) {
                spdlog::warn("Parent guard closed, LemonTea will exit");
                g_running.store(false);
                ::raise(SIGTERM);
                break;
            }
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                spdlog::warn("Parent guard read failed (errno={}), LemonTea will exit", errno);
                g_running.store(false);
                ::raise(SIGTERM);
                break;
            }
        }
        ::close(guard_fd);
    }).detach();
}

int main(int argc, char* argv[]) {
    std::string config_path = "config.json";
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            config_path = argv[i + 1];
            break;
        }
    }

    tea::Config config;
    if (!config.load(config_path)) {
        std::cerr << "Failed to load config: " << config_path << std::endl;
        return 1;
    }

    std::string log_path = config.get<std::string>("log_path", "logs/lemontea.log");
    fs::create_directories(fs::path(log_path).parent_path());
    tea::Logger::init("LemonTea", log_path);
    spdlog::info("LemonTea starting...");

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    startParentGuardMonitor();

    LemonTea lemon;
    if (!lemon.init(config)) {
        spdlog::error("LemonTea initialization failed");
        return 1;
    }

    // Start HTTP API
    uint16_t http_port = config.get<int>("http_port", tea::DEFAULT_HTTP_PORT);
    HttpApi http_api(lemon, http_port);
    if (!http_api.start()) {
        spdlog::error("Failed to start HTTP API on port {}", http_port);
        return 1;
    }

    lemon.run();

    spdlog::info("LemonTea is running. HTTP API on port {}", http_port);

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    http_api.stop();
    lemon.shutdown();
    spdlog::info("LemonTea exited.");
    return 0;
}
