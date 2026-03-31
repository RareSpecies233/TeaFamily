#pragma once

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace tea {

class Logger {
public:
    static void init(const std::string& name, const std::string& log_path,
                     size_t max_size = 5 * 1024 * 1024, size_t max_files = 3) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_path, max_size, max_files);
        file_sink->set_level(spdlog::level::debug);

        auto logger = std::make_shared<spdlog::logger>(
            name, spdlog::sinks_init_list{console_sink, file_sink});
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v");
        logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(logger);
        spdlog::info("{} logger initialized, log file: {}", name, log_path);
    }
};

} // namespace tea
