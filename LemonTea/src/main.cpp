#include <csignal>
#include <iostream>
#include <filesystem>
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
