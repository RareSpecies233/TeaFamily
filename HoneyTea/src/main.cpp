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
#include "honey_tea.h"

namespace fs = std::filesystem;

static std::atomic<bool> g_running{true};
static HoneyTea* g_instance = nullptr;

static void signalHandler(int sig) {
    spdlog::info("HoneyTea received signal {}, shutting down...", sig);
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
                spdlog::warn("Parent guard closed, HoneyTea will exit");
                g_running.store(false);
                ::raise(SIGTERM);
                break;
            }
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                spdlog::warn("Parent guard read failed (errno={}), HoneyTea will exit", errno);
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

    std::string log_path = config.get<std::string>("log_path", "logs/honeytea.log");
    tea::Logger::init("HoneyTea", log_path);
    spdlog::info("HoneyTea bootstrap: pid={}, cwd='{}', config='{}'",
                 ::getpid(), fs::current_path().string(), fs::absolute(config_path).string());
    spdlog::info("HoneyTea starting...");

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    startParentGuardMonitor();

    HoneyTea honey;
    g_instance = &honey;

    if (!honey.init(config)) {
        spdlog::error("HoneyTea initialization failed");
        return 1;
    }

    spdlog::info("HoneyTea config summary: server={}:{}, udp_port={}, plugins_dir='{}', heartbeat_interval_sec={}, reconnect_interval_sec={}",
                 config.get<std::string>("server_host", "127.0.0.1"),
                 config.get<int>("server_port", tea::DEFAULT_TCP_PORT),
                 config.get<int>("udp_port", tea::DEFAULT_UDP_BASE_PORT),
                 config.get<std::string>("plugins_dir", "plugins"),
                 config.get<int>("heartbeat_interval_sec", 5),
                 config.get<int>("reconnect_interval_sec", 3));

    honey.run();

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    honey.shutdown();
    spdlog::info("HoneyTea exited.");
    return 0;
}
