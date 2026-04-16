#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace {

constexpr const char* kPluginName = "cam-lan-stream";
constexpr const char* kConfigFileName = "camera-config.json";
constexpr const char* kMediaMtxConfigName = "mediamtx.yml";
constexpr const char* kClientLogFileName = "cam-lan-stream-client.log";
constexpr size_t kRelayBufferSize = 64 * 1024;

template <typename T>
T clampValue(T value, T min_value, T max_value) {
    return std::max(min_value, std::min(value, max_value));
}

uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void closeFd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

bool pathExists(const std::string& path) {
    struct stat st {};
    return !path.empty() && ::stat(path.c_str(), &st) == 0;
}

bool ensureDirectory(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    if (pathExists(path)) {
        return true;
    }

    size_t pos = 0;
    while (true) {
        pos = path.find('/', pos + 1);
        const std::string segment = pos == std::string::npos ? path : path.substr(0, pos);
        if (segment.empty()) {
            if (pos == std::string::npos) {
                break;
            }
            continue;
        }
        if (!pathExists(segment) && ::mkdir(segment.c_str(), 0755) != 0 && errno != EEXIST) {
            return false;
        }
        if (pos == std::string::npos) {
            break;
        }
    }

    return pathExists(path);
}

std::string joinPath(const std::string& base, const std::string& child) {
    if (base.empty()) {
        return child;
    }
    if (child.empty()) {
        return base;
    }
    if (base.back() == '/') {
        return base + child;
    }
    return base + "/" + child;
}

std::string currentWorkingDirectory() {
    char buffer[PATH_MAX] = {0};
    if (::getcwd(buffer, sizeof(buffer) - 1)) {
        return buffer;
    }
    return ".";
}

std::string trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string normalizeStreamPath(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '-' || c == '_' || c == '.') {
            out.push_back(c);
        }
    }
    return out.empty() ? "rpi-camera" : out;
}

std::string jsonString(const json& source, const std::string& key, const std::string& fallback) {
    if (!source.contains(key) || source[key].is_null()) {
        return fallback;
    }
    if (source[key].is_string()) {
        return source[key].get<std::string>();
    }
    return source[key].dump();
}

int jsonInt(const json& source, const std::string& key, int fallback) {
    if (!source.contains(key) || source[key].is_null()) {
        return fallback;
    }
    try {
        return source[key].get<int>();
    } catch (...) {
        return fallback;
    }
}

double jsonDouble(const json& source, const std::string& key, double fallback) {
    if (!source.contains(key) || source[key].is_null()) {
        return fallback;
    }
    try {
        return source[key].get<double>();
    } catch (...) {
        return fallback;
    }
}

bool jsonBool(const json& source, const std::string& key, bool fallback) {
    if (!source.contains(key) || source[key].is_null()) {
        return fallback;
    }
    try {
        return source[key].get<bool>();
    } catch (...) {
        return fallback;
    }
}

std::string shellEnv(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

std::string yamlQuote(const std::string& value) {
    std::string escaped = "'";
    for (char c : value) {
        if (c == '\'') {
            escaped += "''";
        } else {
            escaped.push_back(c);
        }
    }
    escaped.push_back('\'');
    return escaped;
}

std::string findExecutable(const std::string& explicit_path, const std::vector<std::string>& candidates) {
    auto is_executable = [](const std::string& path) {
        return pathExists(path) && ::access(path.c_str(), X_OK) == 0;
    };

    if (!explicit_path.empty()) {
        if (explicit_path.front() == '/' || explicit_path.find('/') != std::string::npos) {
            return is_executable(explicit_path) ? explicit_path : std::string();
        }
    }

    const std::string path_env = shellEnv("PATH");
    std::vector<std::string> search_names = candidates;
    if (!explicit_path.empty()) {
        search_names.insert(search_names.begin(), explicit_path);
    }

    std::stringstream ss(path_env);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) {
            continue;
        }
        for (const auto& name : search_names) {
            const std::string candidate = joinPath(dir, name);
            if (is_executable(candidate)) {
                return candidate;
            }
        }
    }

    return {};
}

std::string detectPrimaryIPv4() {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(53);
        ::inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            sockaddr_in local_addr{};
            socklen_t len = sizeof(local_addr);
            if (::getsockname(fd, reinterpret_cast<sockaddr*>(&local_addr), &len) == 0) {
                char buffer[INET_ADDRSTRLEN] = {0};
                if (::inet_ntop(AF_INET, &local_addr.sin_addr, buffer, sizeof(buffer))) {
                    closeFd(fd);
                    return buffer;
                }
            }
        }
        closeFd(fd);
    }

    char hostname[256] = {0};
    if (::gethostname(hostname, sizeof(hostname) - 1) == 0) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* result = nullptr;
        if (::getaddrinfo(hostname, nullptr, &hints, &result) == 0) {
            for (addrinfo* item = result; item; item = item->ai_next) {
                if (!item->ai_addr || item->ai_addr->sa_family != AF_INET) {
                    continue;
                }
                auto* inet = reinterpret_cast<sockaddr_in*>(item->ai_addr);
                if (ntohl(inet->sin_addr.s_addr) == INADDR_LOOPBACK) {
                    continue;
                }
                char buffer[INET_ADDRSTRLEN] = {0};
                if (::inet_ntop(AF_INET, &inet->sin_addr, buffer, sizeof(buffer))) {
                    ::freeaddrinfo(result);
                    return buffer;
                }
            }
            ::freeaddrinfo(result);
        }
    }

    return "127.0.0.1";
}

std::string readFirstExistingFile(const std::vector<std::string>& candidates) {
    for (const auto& path : candidates) {
        if (pathExists(path)) {
            return path;
        }
    }
    return {};
}

struct CameraConfig {
    std::string public_host;
    int         camera_id = 0;
    int         width = 1296;
    int         height = 972;
    int         fps = 30;
    int         bitrate = 5000000;
    int         idr_period = 30;
    double      brightness = 0.0;
    double      contrast = 1.0;
    double      saturation = 1.0;
    double      sharpness = 1.0;
    std::string exposure = "normal";
    std::string awb = "auto";
    std::string denoise = "off";
    int         shutter = 0;
    double      gain = 0.0;
    double      ev = 0.0;
    bool        hflip = false;
    bool        vflip = false;
    std::string stream_path = "rpi-camera";
    int         rtsp_port = 8554;
    int         webrtc_port = 8889;
    int         webrtc_udp_port = 8189;
    int         yolo_port = 20000;
    std::string tuning_file;

    void sanitize() {
        width = clampValue(width, 320, 3840);
        height = clampValue(height, 240, 2160);
        fps = clampValue(fps, 1, 120);
        bitrate = clampValue(bitrate, 100000, 50000000);
        idr_period = clampValue(idr_period, 1, 240);
        brightness = clampValue(brightness, -1.0, 1.0);
        contrast = clampValue(contrast, 0.0, 16.0);
        saturation = clampValue(saturation, 0.0, 16.0);
        sharpness = clampValue(sharpness, 0.0, 16.0);
        shutter = std::max(shutter, 0);
        gain = std::max(gain, 0.0);
        ev = clampValue(ev, -10.0, 10.0);
        rtsp_port = clampValue(rtsp_port, 1, 65535);
        webrtc_port = clampValue(webrtc_port, 1, 65535);
        webrtc_udp_port = clampValue(webrtc_udp_port, 1, 65535);
        yolo_port = clampValue(yolo_port, 1, 65535);
        stream_path = normalizeStreamPath(stream_path);
        public_host = trim(public_host);
    }

    json toJson() const {
        return {
            {"public_host", public_host},
            {"camera_id", camera_id},
            {"width", width},
            {"height", height},
            {"fps", fps},
            {"bitrate", bitrate},
            {"idr_period", idr_period},
            {"brightness", brightness},
            {"contrast", contrast},
            {"saturation", saturation},
            {"sharpness", sharpness},
            {"exposure", exposure},
            {"awb", awb},
            {"denoise", denoise},
            {"shutter", shutter},
            {"gain", gain},
            {"ev", ev},
            {"hflip", hflip},
            {"vflip", vflip},
            {"stream_path", stream_path},
            {"rtsp_port", rtsp_port},
            {"webrtc_port", webrtc_port},
            {"webrtc_udp_port", webrtc_udp_port},
            {"yolo_port", yolo_port},
            {"tuning_file", tuning_file}
        };
    }

    void applyJson(const json& source) {
        public_host = jsonString(source, "public_host", public_host);
        camera_id = jsonInt(source, "camera_id", camera_id);
        width = jsonInt(source, "width", width);
        height = jsonInt(source, "height", height);
        fps = jsonInt(source, "fps", fps);
        bitrate = jsonInt(source, "bitrate", bitrate);
        idr_period = jsonInt(source, "idr_period", idr_period);
        brightness = jsonDouble(source, "brightness", brightness);
        contrast = jsonDouble(source, "contrast", contrast);
        saturation = jsonDouble(source, "saturation", saturation);
        sharpness = jsonDouble(source, "sharpness", sharpness);
        exposure = jsonString(source, "exposure", exposure);
        awb = jsonString(source, "awb", awb);
        denoise = jsonString(source, "denoise", denoise);
        shutter = jsonInt(source, "shutter", shutter);
        gain = jsonDouble(source, "gain", gain);
        ev = jsonDouble(source, "ev", ev);
        hflip = jsonBool(source, "hflip", hflip);
        vflip = jsonBool(source, "vflip", vflip);
        stream_path = jsonString(source, "stream_path", stream_path);
        rtsp_port = jsonInt(source, "rtsp_port", rtsp_port);
        webrtc_port = jsonInt(source, "webrtc_port", webrtc_port);
        webrtc_udp_port = jsonInt(source, "webrtc_udp_port", webrtc_udp_port);
        yolo_port = jsonInt(source, "yolo_port", yolo_port);
        tuning_file = jsonString(source, "tuning_file", tuning_file);
        sanitize();
    }
};

struct ChildProcess {
    pid_t pid = -1;
    int   stdin_fd = -1;
    int   stdout_fd = -1;
    int   stderr_fd = -1;
    bool  has_exit_status = false;
    int   last_exit_status = 0;

    ChildProcess() = default;
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    ChildProcess(ChildProcess&& other) noexcept { *this = std::move(other); }
    ChildProcess& operator=(ChildProcess&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        pid = other.pid;
        stdin_fd = other.stdin_fd;
        stdout_fd = other.stdout_fd;
        stderr_fd = other.stderr_fd;
        has_exit_status = other.has_exit_status;
        last_exit_status = other.last_exit_status;
        other.pid = -1;
        other.stdin_fd = -1;
        other.stdout_fd = -1;
        other.stderr_fd = -1;
        other.has_exit_status = false;
        other.last_exit_status = 0;
        return *this;
    }

    void closePipes() {
        closeFd(stdin_fd);
        closeFd(stdout_fd);
        closeFd(stderr_fd);
    }

    bool start(const std::vector<std::string>& args,
               const std::string& cwd,
               bool capture_stdout,
               bool capture_stderr,
               bool stderr_to_stdout) {
        if (args.empty()) {
            return false;
        }

        int stdout_pipe[2] = {-1, -1};
        int stderr_pipe[2] = {-1, -1};

        if (capture_stdout && ::pipe(stdout_pipe) != 0) {
            return false;
        }

        if (capture_stderr && !stderr_to_stdout && ::pipe(stderr_pipe) != 0) {
            closeFd(stdout_pipe[0]);
            closeFd(stdout_pipe[1]);
            return false;
        }

        pid = ::fork();
        if (pid < 0) {
            closeFd(stdout_pipe[0]);
            closeFd(stdout_pipe[1]);
            closeFd(stderr_pipe[0]);
            closeFd(stderr_pipe[1]);
            return false;
        }

        if (pid == 0) {
            int devnull = ::open("/dev/null", O_RDONLY);
            if (devnull >= 0) {
                ::dup2(devnull, STDIN_FILENO);
            }

            if (capture_stdout) {
                ::dup2(stdout_pipe[1], STDOUT_FILENO);
            }

            if (capture_stderr) {
                if (stderr_to_stdout && capture_stdout) {
                    ::dup2(stdout_pipe[1], STDERR_FILENO);
                } else if (!stderr_to_stdout) {
                    ::dup2(stderr_pipe[1], STDERR_FILENO);
                }
            }

            closeFd(stdout_pipe[0]);
            closeFd(stdout_pipe[1]);
            closeFd(stderr_pipe[0]);
            closeFd(stderr_pipe[1]);
            closeFd(devnull);

            if (!cwd.empty()) {
                ::chdir(cwd.c_str());
            }

            std::vector<char*> argv;
            argv.reserve(args.size() + 1);
            for (const auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            ::execvp(argv[0], argv.data());
            _exit(127);
        }

        if (capture_stdout) {
            closeFd(stdout_pipe[1]);
            stdout_fd = stdout_pipe[0];
        }
        if (capture_stderr && !stderr_to_stdout) {
            closeFd(stderr_pipe[1]);
            stderr_fd = stderr_pipe[0];
        }

        has_exit_status = false;
        last_exit_status = 0;
        return true;
    }

    bool poll(int* status_out = nullptr) {
        if (pid <= 0) {
            if (has_exit_status && status_out) {
                *status_out = last_exit_status;
            }
            return has_exit_status;
        }

        int status = 0;
        const pid_t result = ::waitpid(pid, &status, WNOHANG);
        if (result == 0) {
            return false;
        }
        if (result != pid) {
            return false;
        }

        if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_exit_status = 128 + WTERMSIG(status);
        } else {
            last_exit_status = status;
        }

        has_exit_status = true;
        pid = -1;
        closePipes();
        if (status_out) {
            *status_out = last_exit_status;
        }
        return true;
    }

    bool running() {
        return pid > 0 && !poll();
    }

    void stop() {
        if (pid > 0) {
            ::kill(pid, SIGINT);
            for (int i = 0; i < 15; ++i) {
                if (poll() || pid <= 0) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        if (pid > 0) {
            ::kill(pid, SIGTERM);
            for (int i = 0; i < 10; ++i) {
                if (poll() || pid <= 0) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        if (pid > 0) {
            ::kill(pid, SIGKILL);
            int status = 0;
            ::waitpid(pid, &status, 0);
            pid = -1;
        }

        closePipes();
    }
};

std::mutex       g_out_mutex;
std::mutex       g_state_mutex;
std::mutex       g_clients_mutex;
std::atomic<bool> g_running{true};
std::atomic<bool> g_stream_requested{false};
CameraConfig     g_config;
ChildProcess     g_mediamtx_process;
ChildProcess     g_relay_process;
std::thread      g_mediamtx_log_thread;
std::thread      g_relay_log_thread;
std::thread      g_relay_data_thread;
std::thread      g_yolo_accept_thread;
std::thread      g_monitor_thread;
int              g_yolo_server_fd = -1;
std::vector<int> g_yolo_clients;
std::string      g_last_error;
std::string      g_last_warning;
uint32_t         g_restart_failures = 0;
std::string      g_runtime_dir;

void sendJson(const json& payload) {
    std::lock_guard<std::mutex> lock(g_out_mutex);
    std::cout << payload.dump() << "\n";
    std::cout.flush();
}

bool configureLogging(std::string& warning) {
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            joinPath(g_runtime_dir, kClientLogFileName),
            true);

        console_sink->set_level(spdlog::level::info);
        file_sink->set_level(spdlog::level::trace);

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("cam-lan-stream-client", sinks.begin(), sinks.end());
        logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [pid %P] [tid %t] %v");
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);

        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);

        spdlog::info("cam-lan-stream client logger initialized: {}", joinPath(g_runtime_dir, kClientLogFileName));
        return true;
    } catch (const std::exception& e) {
        warning = std::string("failed to initialize file logger: ") + e.what();
        spdlog::set_level(spdlog::level::info);
        return false;
    }
}

void emitStreamWarning(const std::string& message) {
    sendJson({
        {"action", "stream_warning"},
        {"plugin", kPluginName},
        {"message", message}
    });
}

std::string configFilePath() {
    return joinPath(g_runtime_dir, kConfigFileName);
}

std::string mediaMtxConfigPath() {
    return joinPath(g_runtime_dir, kMediaMtxConfigName);
}

std::string clientLogFilePath() {
    return joinPath(g_runtime_dir, kClientLogFileName);
}

json buildSystemInfo() {
    json info;
    char hostname[256] = {0};
    if (::gethostname(hostname, sizeof(hostname) - 1) == 0) {
        info["hostname"] = hostname;
    } else {
        info["hostname"] = "unknown";
    }

    struct utsname uts{};
    if (::uname(&uts) == 0) {
        info["os"] = uts.sysname;
        info["kernel"] = uts.release;
        info["arch"] = uts.machine;
    } else {
        info["os"] = "unknown";
        info["kernel"] = "unknown";
        info["arch"] = "unknown";
    }

    info["rpi_camera_supported"] = pathExists("/dev/video0") && pathExists("/dev/media0");
    return info;
}

json buildCapabilities(const std::string& request_id = std::string()) {
    json payload = {
        {"action", "capabilities_result"},
        {"plugin", kPluginName},
        {"supported_actions", json::array({
            "describe",
            "discover",
            "status",
            "get_config",
            "set_config",
            "start_stream",
            "stop_stream",
            "restart_stream",
            "set_public_host"
        })},
        {"camera_controls", json::array({
            "camera_id",
            "width",
            "height",
            "fps",
            "bitrate",
            "idr_period",
            "brightness",
            "contrast",
            "saturation",
            "sharpness",
            "exposure",
            "awb",
            "denoise",
            "shutter",
            "gain",
            "ev",
            "hflip",
            "vflip",
            "stream_path",
            "rtsp_port",
            "webrtc_port",
            "webrtc_udp_port",
            "yolo_port",
            "public_host",
            "tuning_file"
        })},
        {"dependencies", {
            {"mediamtx_env", "TEA_CAM_STREAM_MEDIAMTX_BIN"},
            {"ffmpeg_env", "TEA_CAM_STREAM_FFMPEG_BIN"}
        }},
        {"transport", {
            {"frontend", "MediaMTX WebRTC / WHEP"},
            {"yolo", "raw H.264 over TCP"}
        }}
    };
    if (!request_id.empty()) {
        payload["request_id"] = request_id;
    }
    return payload;
}

void loadPersistedConfig() {
    g_config.sanitize();
    const std::string path = configFilePath();
    if (!pathExists(path)) {
        g_config.tuning_file = readFirstExistingFile({
            "/usr/share/libcamera/ipa/rpi/pisp/ov5647.json",
            "/usr/share/libcamera/ipa/rpi/vc4/ov5647.json"
        });
        return;
    }

    try {
        std::ifstream input(path);
        auto payload = json::parse(input);
        if (payload.is_object()) {
            g_config.applyJson(payload);
        }
    } catch (const std::exception& e) {
        g_last_warning = std::string("failed to load persisted camera config: ") + e.what();
        spdlog::warn("{}", g_last_warning);
    }

    if (g_config.tuning_file.empty()) {
        g_config.tuning_file = readFirstExistingFile({
            "/usr/share/libcamera/ipa/rpi/pisp/ov5647.json",
            "/usr/share/libcamera/ipa/rpi/vc4/ov5647.json"
        });
    }
    g_config.sanitize();
}

void savePersistedConfig() {
    try {
        ensureDirectory(g_runtime_dir);
        std::ofstream output(configFilePath());
        output << g_config.toJson().dump(2);
    } catch (const std::exception& e) {
        g_last_warning = std::string("failed to persist camera config: ") + e.what();
        spdlog::warn("{}", g_last_warning);
    }
}

std::string effectivePublicHostLocked() {
    return g_config.public_host.empty() ? detectPrimaryIPv4() : g_config.public_host;
}

std::string buildMediaMtxConfigText(const CameraConfig& config, const std::string& public_host) {
    std::ostringstream out;
    out << "logLevel: info\n";
    out << "logDestinations: [stdout]\n";
    out << "api: false\n";
    out << "metrics: false\n";
    out << "pprof: false\n";
    out << "playback: false\n";
    out << "rtsp: true\n";
    out << "rtspTransports: [tcp]\n";
    out << "rtspAddress: :" << config.rtsp_port << "\n";
    out << "hls: false\n";
    out << "webrtc: true\n";
    out << "webrtcAddress: :" << config.webrtc_port << "\n";
    out << "webrtcAllowOrigins: ['*']\n";
    out << "webrtcLocalUDPAddress: :" << config.webrtc_udp_port << "\n";
    out << "webrtcLocalTCPAddress: ''\n";
    out << "webrtcIPsFromInterfaces: true\n";
    if (!public_host.empty()) {
        out << "webrtcAdditionalHosts: [" << yamlQuote(public_host) << "]\n";
    }
    out << "paths:\n";
    out << "  " << config.stream_path << ":\n";
    out << "    source: rpiCamera\n";
    out << "    rpiCameraCamID: " << config.camera_id << "\n";
    out << "    rpiCameraWidth: " << config.width << "\n";
    out << "    rpiCameraHeight: " << config.height << "\n";
    out << "    rpiCameraFPS: " << config.fps << "\n";
    out << "    rpiCameraBitrate: " << config.bitrate << "\n";
    out << "    rpiCameraCodec: softwareH264\n";
    out << "    rpiCameraSoftwareH264Profile: 'high'\n";
    out << "    rpiCameraIDRPeriod: " << config.idr_period << "\n";
    out << "    rpiCameraBrightness: " << config.brightness << "\n";
    out << "    rpiCameraContrast: " << config.contrast << "\n";
    out << "    rpiCameraSaturation: " << config.saturation << "\n";
    out << "    rpiCameraSharpness: " << config.sharpness << "\n";
    out << "    rpiCameraExposure: " << yamlQuote(config.exposure) << "\n";
    out << "    rpiCameraAWB: " << yamlQuote(config.awb) << "\n";
    out << "    rpiCameraDenoise: " << yamlQuote(config.denoise) << "\n";
    out << "    rpiCameraShutter: " << config.shutter << "\n";
    out << "    rpiCameraGain: " << config.gain << "\n";
    out << "    rpiCameraEV: " << config.ev << "\n";
    out << "    rpiCameraHFlip: " << (config.hflip ? "true" : "false") << "\n";
    out << "    rpiCameraVFlip: " << (config.vflip ? "true" : "false") << "\n";
    if (!config.tuning_file.empty()) {
        out << "    rpiCameraTuningFile: " << yamlQuote(config.tuning_file) << "\n";
    }
    return out.str();
}

void writeMediaMtxConfig() {
    ensureDirectory(g_runtime_dir);
    std::ofstream output(mediaMtxConfigPath());
    output << buildMediaMtxConfigText(g_config, effectivePublicHostLocked());
}

void removeClosedClientsLocked() {
    g_yolo_clients.erase(
        std::remove_if(g_yolo_clients.begin(), g_yolo_clients.end(), [](int fd) { return fd < 0; }),
        g_yolo_clients.end());
}

void closeAllClientsLocked() {
    for (int& client : g_yolo_clients) {
        closeFd(client);
    }
    g_yolo_clients.clear();
}

ssize_t sendNoSignal(int fd, const void* data, size_t size) {
#ifdef MSG_NOSIGNAL
    return ::send(fd, data, size, MSG_NOSIGNAL);
#else
    return ::send(fd, data, size, 0);
#endif
}

void broadcastRelayBuffer(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(g_clients_mutex);
    for (int& client : g_yolo_clients) {
        if (client < 0) {
            continue;
        }
        const ssize_t sent = sendNoSignal(client, data, size);
        if (sent < 0 || static_cast<size_t>(sent) != size) {
            closeFd(client);
        }
    }
    removeClosedClientsLocked();
}

void joinThread(std::thread& thread) {
    if (thread.joinable()) {
        thread.join();
    }
}

void startLogReaderThread(std::thread& thread, int fd, const std::string& prefix) {
    if (fd < 0) {
        return;
    }
    thread = std::thread([fd, prefix]() mutable {
        std::string buffer;
        buffer.reserve(4096);
        std::array<char, 512> chunk{};
        while (g_running.load()) {
            const ssize_t count = ::read(fd, chunk.data(), chunk.size());
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                spdlog::warn("{}log reader read failed: {}", prefix, std::strerror(errno));
                break;
            }
            if (count == 0) {
                break;
            }
            buffer.append(chunk.data(), static_cast<size_t>(count));
            size_t pos = 0;
            while (true) {
                size_t newline = buffer.find('\n', pos);
                if (newline == std::string::npos) {
                    buffer.erase(0, pos);
                    break;
                }
                const std::string line = trim(buffer.substr(pos, newline - pos));
                if (!line.empty()) {
                    spdlog::info("{}{}", prefix, line);
                }
                pos = newline + 1;
            }
        }
        if (!buffer.empty()) {
            const std::string line = trim(buffer);
            if (!line.empty()) {
                spdlog::info("{}{}", prefix, line);
            }
        }
        spdlog::info("{}log reader stopped", prefix);
    });
}

void startRelayDataThread() {
    const int fd = g_relay_process.stdout_fd;
    if (fd < 0) {
        return;
    }
    g_relay_data_thread = std::thread([fd]() mutable {
        std::array<uint8_t, kRelayBufferSize> buffer{};
        while (g_running.load()) {
            const ssize_t count = ::read(fd, buffer.data(), buffer.size());
            if (count <= 0) {
                break;
            }
            broadcastRelayBuffer(buffer.data(), static_cast<size_t>(count));
        }
    });
}

bool openYoloServerLocked(std::string& error) {
    closeFd(g_yolo_server_fd);
    closeAllClientsLocked();

    g_yolo_server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (g_yolo_server_fd < 0) {
        error = "failed to create YOLO TCP server socket";
        return false;
    }

    int reuse = 1;
    ::setsockopt(g_yolo_server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_NOSIGPIPE
    ::setsockopt(g_yolo_server_fd, SOL_SOCKET, SO_NOSIGPIPE, &reuse, sizeof(reuse));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(g_config.yolo_port));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(g_yolo_server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        error = "failed to bind YOLO TCP port " + std::to_string(g_config.yolo_port) + ": " + std::strerror(errno);
        closeFd(g_yolo_server_fd);
        return false;
    }

    if (::listen(g_yolo_server_fd, 4) != 0) {
        error = "failed to listen YOLO TCP port " + std::to_string(g_config.yolo_port) + ": " + std::strerror(errno);
        closeFd(g_yolo_server_fd);
        return false;
    }

    return true;
}

void startAcceptThreadLocked() {
    const int server_fd = g_yolo_server_fd;
    if (server_fd < 0) {
        return;
    }

    g_yolo_accept_thread = std::thread([server_fd]() {
        while (g_running.load()) {
            sockaddr_in remote{};
            socklen_t len = sizeof(remote);
            const int client_fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&remote), &len);
            if (client_fd < 0) {
                if (!g_running.load()) {
                    break;
                }
                if (errno == EINTR) {
                    continue;
                }
                break;
            }

            int flags = ::fcntl(client_fd, F_GETFL, 0);
            if (flags >= 0) {
                ::fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
            }
#ifdef SO_NOSIGPIPE
            int one = 1;
            ::setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif

            std::lock_guard<std::mutex> lock(g_clients_mutex);
            g_yolo_clients.push_back(client_fd);
        }
    });
}

bool prepareRuntimeDependencies(std::string& mediamtx_bin, std::string& ffmpeg_bin, std::string& error) {
    mediamtx_bin = findExecutable(shellEnv("TEA_CAM_STREAM_MEDIAMTX_BIN"), {"mediamtx", "MediaMTX"});
    ffmpeg_bin = findExecutable(shellEnv("TEA_CAM_STREAM_FFMPEG_BIN"), {"ffmpeg"});

    if (mediamtx_bin.empty()) {
        error = "MediaMTX executable not found. Set TEA_CAM_STREAM_MEDIAMTX_BIN or install mediamtx.";
        spdlog::error("{}", error);
        return false;
    }
    if (ffmpeg_bin.empty()) {
        error = "ffmpeg executable not found. Set TEA_CAM_STREAM_FFMPEG_BIN or install ffmpeg.";
        spdlog::error("{}", error);
        return false;
    }

    return true;
}

bool platformReady(std::string& error) {
#ifdef __linux__
    if (!pathExists("/dev/video0") || !pathExists("/dev/media0")) {
        error = "Raspberry Pi camera devices (/dev/video0, /dev/media0) are not available";
        spdlog::error("{}", error);
        return false;
    }
    return true;
#else
    error = "cam-lan-stream HoneyTea runtime currently requires Linux / Raspberry Pi camera stack";
    spdlog::error("{}", error);
    return false;
#endif
}

void stopStreamingServices();

bool startStreamingServices(const std::string& request_id, std::string& error) {
    spdlog::info("startStreamingServices called, request_id={}", request_id.empty() ? "-" : request_id);
    stopStreamingServices();

    std::string platform_error;
    if (!platformReady(platform_error)) {
        error = platform_error;
        return false;
    }

    std::string mediamtx_bin;
    std::string ffmpeg_bin;
    if (!prepareRuntimeDependencies(mediamtx_bin, ffmpeg_bin, error)) {
        return false;
    }

    try {
        writeMediaMtxConfig();
        spdlog::info("wrote MediaMTX config: {}", mediaMtxConfigPath());
    } catch (const std::exception& e) {
        error = std::string("failed to write mediamtx.yml: ") + e.what();
        spdlog::error("{}", error);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        if (!openYoloServerLocked(error)) {
            spdlog::error("{}", error);
            return false;
        }
        spdlog::info("YOLO TCP server listening on 0.0.0.0:{}", g_config.yolo_port);
        startAcceptThreadLocked();

        spdlog::info(
            "starting MediaMTX: bin={}, rtsp_port={}, webrtc_port={}, webrtc_udp_port={}",
            mediamtx_bin,
            g_config.rtsp_port,
            g_config.webrtc_port,
            g_config.webrtc_udp_port);
        if (!g_mediamtx_process.start(
            {mediamtx_bin, mediaMtxConfigPath()},
                g_runtime_dir,
                true,
                true,
                true)) {
            error = "failed to start MediaMTX process";
            spdlog::error("{}", error);
            closeFd(g_yolo_server_fd);
            return false;
        }
        spdlog::info("MediaMTX started, pid={}", g_mediamtx_process.pid);
        startLogReaderThread(g_mediamtx_log_thread, g_mediamtx_process.stdout_fd, "[mediamtx] ");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(900));

    const std::string input_url = "rtsp://127.0.0.1:" + std::to_string(g_config.rtsp_port) + "/" + g_config.stream_path;
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        spdlog::info("starting relay ffmpeg: bin={}, input={}", ffmpeg_bin, input_url);
        if (!g_relay_process.start(
                {ffmpeg_bin,
                 "-nostdin",
                 "-loglevel",
                 "warning",
                 "-rtsp_transport",
                 "tcp",
                 "-fflags",
                 "nobuffer",
                 "-flags",
                 "low_delay",
                 "-i",
                 input_url,
                 "-an",
                 "-c:v",
                 "copy",
                 "-bsf:v",
                 "dump_extra",
                 "-f",
                 "h264",
                 "pipe:1"},
                g_runtime_dir,
                true,
                true,
                false)) {
            error = "failed to start ffmpeg H.264 relay";
            spdlog::error("{}", error);
            g_mediamtx_process.stop();
            return false;
        }
        spdlog::info("relay ffmpeg started, pid={}", g_relay_process.pid);
        startLogReaderThread(g_relay_log_thread, g_relay_process.stderr_fd, "[relay] ");
        startRelayDataThread();
    }

    g_stream_requested.store(true);
    g_last_error.clear();
    g_last_warning.clear();
    g_restart_failures = 0;
    sendJson({
        {"action", "stream_state"},
        {"plugin", kPluginName},
        {"request_id", request_id},
        {"state", "running"},
        {"message", "MediaMTX and H.264 relay started"}
    });
    spdlog::info("stream services started successfully");
    return true;
}

void stopStreamingServices() {
    spdlog::info("stopStreamingServices called");
    g_stream_requested.store(false);

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        closeFd(g_yolo_server_fd);
        g_relay_process.stop();
        g_mediamtx_process.stop();
    }

    {
        std::lock_guard<std::mutex> clients_lock(g_clients_mutex);
        closeAllClientsLocked();
    }

    joinThread(g_relay_data_thread);
    joinThread(g_relay_log_thread);
    joinThread(g_mediamtx_log_thread);
    joinThread(g_yolo_accept_thread);
    spdlog::info("stream services stopped");
}

void pollProcessesLocked() {
    int status = 0;
    if (g_mediamtx_process.poll(&status)) {
        g_last_error = "MediaMTX exited with status " + std::to_string(status);
        spdlog::error("{}", g_last_error);
    }

    if (g_relay_process.poll(&status)) {
        g_last_error = "H.264 relay exited with status " + std::to_string(status);
        spdlog::error("{}", g_last_error);
    }
}

json buildStatusPayload(const std::string& request_id = std::string(), const std::string& action = "status_result") {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    pollProcessesLocked();

    const bool mediamtx_running = g_mediamtx_process.pid > 0;
    const bool relay_running = g_relay_process.pid > 0;

    size_t yolo_clients = 0;
    {
        std::lock_guard<std::mutex> clients_lock(g_clients_mutex);
        removeClosedClientsLocked();
        yolo_clients = g_yolo_clients.size();
    }

    std::string state = "stopped";
    if (g_stream_requested.load()) {
        state = (mediamtx_running && relay_running) ? "running" : "starting";
    }
    if (!g_last_error.empty() && (!mediamtx_running || !relay_running)) {
        state = "error";
    }

    const std::string public_host = effectivePublicHostLocked();
    json payload = {
        {"action", action},
        {"plugin", kPluginName},
        {"state", state},
        {"camera_config", g_config.toJson()},
        {"service", {
            {"active", mediamtx_running && relay_running},
            {"desired_active", g_stream_requested.load()},
            {"mediamtx_running", mediamtx_running},
            {"relay_running", relay_running},
            {"mediamtx_pid", g_mediamtx_process.pid},
            {"relay_pid", g_relay_process.pid},
            {"yolo_clients", yolo_clients}
        }},
        {"endpoints", {
            {"public_host", public_host},
            {"rtsp_url", "rtsp://" + public_host + ":" + std::to_string(g_config.rtsp_port) + "/" + g_config.stream_path},
            {"webrtc_player_url", "http://" + public_host + ":" + std::to_string(g_config.webrtc_port) + "/" + g_config.stream_path},
            {"webrtc_whep_url", "http://" + public_host + ":" + std::to_string(g_config.webrtc_port) + "/" + g_config.stream_path + "/whep"},
            {"yolo_tcp_url", "tcp://" + public_host + ":" + std::to_string(g_config.yolo_port)}
        }},
        {"dependencies", {
            {"mediamtx_bin", findExecutable(shellEnv("TEA_CAM_STREAM_MEDIAMTX_BIN"), {"mediamtx", "MediaMTX"})},
            {"ffmpeg_bin", findExecutable(shellEnv("TEA_CAM_STREAM_FFMPEG_BIN"), {"ffmpeg"})}
        }},
        {"system", buildSystemInfo()},
        {"last_error", g_last_error},
        {"last_warning", g_last_warning},
        {"restart_failures", g_restart_failures}
    };

    if (!request_id.empty()) {
        payload["request_id"] = request_id;
    }

    return payload;
}

json buildConfigPayload(const std::string& request_id = std::string()) {
    auto payload = buildStatusPayload(request_id, "config_result");
    payload["camera_config"] = g_config.toJson();
    return payload;
}

void sendStreamError(const std::string& request_id, const std::string& message) {
    auto payload = buildStatusPayload(request_id, "stream_error");
    payload["message"] = message;
    payload["last_error"] = message;
    sendJson(payload);
}

void monitorLoop() {
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        bool should_restart = false;
        std::string reason;

        {
            std::lock_guard<std::mutex> lock(g_state_mutex);
            pollProcessesLocked();
            if (g_stream_requested.load() &&
                ((g_mediamtx_process.pid <= 0) || (g_relay_process.pid <= 0))) {
                should_restart = true;
                reason = g_last_error.empty() ? "stream subprocess exited unexpectedly" : g_last_error;
            }
        }

        if (!should_restart) {
            continue;
        }

        ++g_restart_failures;
        emitStreamWarning("video service exited unexpectedly, restarting: " + reason);
        spdlog::warn("video service exited unexpectedly, restart attempt #{}: {}", g_restart_failures, reason);
        std::string restart_error;
        if (!startStreamingServices(std::string(), restart_error)) {
            g_stream_requested.store(false);
            g_last_error = restart_error;
            spdlog::error("restart failed: {}", restart_error);
            sendStreamError(std::string(), restart_error);
        }
    }
}

void handleSetConfig(const json& msg) {
    const std::string request_id = msg.value("request_id", std::string(""));
    const json config = msg.value("camera_config", json::object());
    spdlog::info("received set_config, request_id={}", request_id.empty() ? "-" : request_id);

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        g_config.applyJson(config);
        savePersistedConfig();
    }

    if (g_stream_requested.load()) {
        std::string error;
        if (!startStreamingServices(request_id, error)) {
            g_last_error = error;
            sendStreamError(request_id, error);
            return;
        }
    }

    sendJson(buildConfigPayload(request_id));
}

void handleStart(const json& msg) {
    const std::string request_id = msg.value("request_id", std::string(""));
    spdlog::info("received start_stream, request_id={}", request_id.empty() ? "-" : request_id);
    std::string error;
    if (!startStreamingServices(request_id, error)) {
        g_last_error = error;
        sendStreamError(request_id, error);
        return;
    }
    sendJson(buildStatusPayload(request_id));
}

void handleStop(const json& msg) {
    const std::string request_id = msg.value("request_id", std::string(""));
    spdlog::info("received stop_stream, request_id={}", request_id.empty() ? "-" : request_id);
    stopStreamingServices();
    sendJson(buildStatusPayload(request_id));
}

void handleMessage(const json& msg) {
    const std::string cmd = msg.value("cmd", std::string(""));
    const std::string action = msg.value("action", std::string(""));
    const std::string request_id = msg.value("request_id", std::string(""));

    if (cmd == "start") {
        return;
    }

    if (cmd == "stop") {
        spdlog::info("received cmd=stop, shutting down client plugin");
        g_running.store(false);
        stopStreamingServices();
        sendJson({{"action", "stopped"}, {"plugin", kPluginName}});
        return;
    }

    if (cmd == "status") {
        sendJson(buildStatusPayload(request_id));
        return;
    }

    if (action == "ping") {
        sendJson({
            {"action", "pong"},
            {"plugin", kPluginName},
            {"request_id", request_id}
        });
        return;
    }

    if (action == "describe" || action == "capabilities" || action == "discover") {
        auto payload = buildCapabilities(request_id);
        if (action == "discover") {
            payload["action"] = "discover_result";
            payload["status"] = buildStatusPayload(request_id);
        }
        sendJson(payload);
        return;
    }

    if (action == "status") {
        sendJson(buildStatusPayload(request_id));
        return;
    }

    if (action == "get_config") {
        sendJson(buildConfigPayload(request_id));
        return;
    }

    if (action == "set_config") {
        handleSetConfig(msg);
        return;
    }

    if (action == "set_public_host") {
        handleSetConfig({
            {"request_id", request_id},
            {"camera_config", {{"public_host", msg.value("public_host", std::string(""))}}}
        });
        return;
    }

    if (action == "start_stream") {
        handleStart(msg);
        return;
    }

    if (action == "stop_stream") {
        handleStop(msg);
        return;
    }

    if (action == "restart_stream") {
        stopStreamingServices();
        handleStart(msg);
        return;
    }

    sendJson({
        {"action", "stream_error"},
        {"plugin", kPluginName},
        {"request_id", request_id},
        {"message", "unsupported action: " + action}
    });
    spdlog::warn("unsupported action received: {}", action);
}

}  // namespace

int main() {
    ::signal(SIGPIPE, SIG_IGN);
    spdlog::set_level(spdlog::level::info);

    g_runtime_dir = joinPath(currentWorkingDirectory(), "runtime");
    ensureDirectory(g_runtime_dir);

    std::string logger_warning;
    if (!configureLogging(logger_warning)) {
        g_last_warning = logger_warning;
    }
    if (!logger_warning.empty()) {
        spdlog::warn("{}", logger_warning);
    }

    loadPersistedConfig();
    spdlog::info("cam-lan-stream client started, runtime_dir={}", g_runtime_dir);
    spdlog::info("camera config file: {}", configFilePath());
    spdlog::info("log file: {}", clientLogFilePath());

    g_monitor_thread = std::thread(monitorLoop);

    sendJson({
        {"action", "ready"},
        {"plugin", kPluginName},
        {"role", "client"},
        {"summary", "HoneyTea-side Raspberry Pi camera controller"},
        {"camera_config", g_config.toJson()}
    });

    std::string line;
    while (g_running.load() && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        try {
            auto msg = json::parse(line);
            handleMessage(msg);
        } catch (const std::exception& e) {
            spdlog::warn("invalid json message: {}", e.what());
            sendJson({
                {"action", "stream_error"},
                {"plugin", kPluginName},
                {"message", std::string("invalid json message: ") + e.what()}
            });
        }
    }

    g_running.store(false);
    stopStreamingServices();
    joinThread(g_monitor_thread);
    return 0;
}