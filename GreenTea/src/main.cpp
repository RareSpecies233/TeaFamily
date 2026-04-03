#include <csignal>
#include <iostream>
#include <filesystem>
#include <unistd.h>
#include <tea/common.h>
#include <tea/config.h>
#include <tea/logger.h>
#include "watchdog.h"
#include "updater.h"

namespace fs = std::filesystem;

static std::atomic<bool> g_running{true};

static void signalHandler(int sig) {
    spdlog::info("GreenTea received signal {}, shutting down...", sig);
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    // Determine config path
    std::string config_path = "config.json";
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--config") {
            config_path = argv[i + 1];
            break;
        }
    }

    // Load config
    tea::Config config;
    if (!config.load(config_path)) {
        std::cerr << "Failed to load config: " << config_path << std::endl;
        return 1;
    }

    // Init logger
    std::string log_path = config.get<std::string>("log_path", "logs/greentea.log");
    tea::Logger::init("GreenTea", log_path);
    const auto config_abs = fs::absolute(config_path);
    const auto config_dir = config_abs.parent_path();
    spdlog::info("GreenTea bootstrap: pid={}, cwd='{}', config='{}'",
                 ::getpid(), fs::current_path().string(), config_abs.string());
    spdlog::info("GreenTea starting...");

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGCHLD, SIG_DFL);

    // Create watchdog
    Watchdog watchdog;
    watchdog.loadConfig(config, config_dir.string());

    // Create updater
    uint16_t update_port = config.get<int>("listen_port", tea::DEFAULT_GREENTEA_PORT);
    Updater updater(update_port);
    updater.setUpdateCallback([&watchdog](const std::string& name, const std::string& temp_path) -> bool {
        return watchdog.updateBinary(name, temp_path);
    });

    // Start
    watchdog.start();
    if (!updater.start()) {
        spdlog::error("Failed to start updater on port {}", update_port);
    }

    spdlog::info("GreenTea is running. Press Ctrl+C to stop.");

    // Main loop
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Shutdown
    spdlog::info("GreenTea shutting down...");
    updater.stop();
    watchdog.stop();
    spdlog::info("GreenTea stopped.");

    return 0;
}
