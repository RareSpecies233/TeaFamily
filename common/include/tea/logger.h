#pragma once

#include <string>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace tea {

class Logger {
public:
    static std::string activeLogPath() {
        return active_log_path_;
    }

    static void init(const std::string& name, const std::string& log_path,
                     size_t max_size = 5 * 1024 * 1024, size_t max_files = 3) {
        const std::string file_path = resolveLogFilePath(name, log_path);
        const auto file_parent = std::filesystem::path(file_path).parent_path();
        if (!file_parent.empty()) {
            std::filesystem::create_directories(file_parent);
        }

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file_path, max_size, max_files);
        file_sink->set_level(spdlog::level::debug);

        auto logger = std::make_shared<spdlog::logger>(
            name, spdlog::sinks_init_list{console_sink, file_sink});
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v");
        logger->flush_on(spdlog::level::info);

        spdlog::set_default_logger(logger);
        active_log_path_ = file_path;
        spdlog::info("{} logger initialized, configured path: {}, active log file: {}", name, log_path, file_path);
    }

private:
    static std::string sanitizeProgramName(const std::string& name) {
        std::string out = name;
        std::replace_if(out.begin(), out.end(),
                        [](char c) {
                            const bool is_digit = (c >= '0' && c <= '9');
                            const bool is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                            return !(is_digit || is_alpha || c == '-' || c == '_');
                        },
                        '_');
        return out.empty() ? std::string("TeaProgram") : out;
    }

    static std::string nowForFilename() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);

        std::tm tm_snapshot{};
#if defined(_WIN32)
        localtime_s(&tm_snapshot, &tt);
#else
        localtime_r(&tt, &tm_snapshot);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_snapshot, "%Y-%m-%d_%H-%M-%S");
        return oss.str();
    }

    static std::string resolveLogDir(const std::string& configured_path) {
        std::filesystem::path p(configured_path.empty() ? "logs" : configured_path);

        if (configured_path.empty()) {
            return p.string();
        }

        std::error_code ec;
        if (std::filesystem::exists(p, ec) && std::filesystem::is_directory(p, ec)) {
            return p.string();
        }

        if (p.has_filename() && p.has_extension()) {
            const auto parent = p.parent_path();
            return parent.empty() ? std::string(".") : parent.string();
        }

        return p.string();
    }

    static std::string resolveLogFilePath(const std::string& name, const std::string& configured_path) {
        const auto dir = std::filesystem::path(resolveLogDir(configured_path));
        const std::string filename = sanitizeProgramName(name) + "_" + nowForFilename() + ".log";
        return (dir / filename).string();
    }

    inline static std::string active_log_path_;
};

} // namespace tea
