/**
 * File Manager Plugin - Client Side (runs on HoneyTea)
 *
 * Performs actual file system operations on the local machine.
 * Receives requests via stdin (JSON-line), sends results via stdout.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <filesystem>
#include <cstring>
#include <vector>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::mutex g_mutex;

// Configurable root directory to restrict access
static std::string g_root_dir = "/";

static const char* BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

static std::string encodeBase64(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < data.size()) {
        uint32_t chunk = (static_cast<uint32_t>(data[i]) << 16) |
                         (static_cast<uint32_t>(data[i + 1]) << 8) |
                         static_cast<uint32_t>(data[i + 2]);
        out.push_back(BASE64_CHARS[(chunk >> 18) & 0x3F]);
        out.push_back(BASE64_CHARS[(chunk >> 12) & 0x3F]);
        out.push_back(BASE64_CHARS[(chunk >> 6) & 0x3F]);
        out.push_back(BASE64_CHARS[chunk & 0x3F]);
        i += 3;
    }

    if (i < data.size()) {
        uint32_t chunk = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < data.size()) {
            chunk |= static_cast<uint32_t>(data[i + 1]) << 8;
        }

        out.push_back(BASE64_CHARS[(chunk >> 18) & 0x3F]);
        out.push_back(BASE64_CHARS[(chunk >> 12) & 0x3F]);
        if (i + 1 < data.size()) {
            out.push_back(BASE64_CHARS[(chunk >> 6) & 0x3F]);
        } else {
            out.push_back('=');
        }
        out.push_back('=');
    }

    return out;
}

static int decodeBase64Char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static bool decodeBase64(const std::string& text, std::vector<uint8_t>& out) {
    out.clear();
    if (text.size() % 4 != 0) {
        return false;
    }

    for (size_t i = 0; i < text.size(); i += 4) {
        int c0 = decodeBase64Char(text[i]);
        int c1 = decodeBase64Char(text[i + 1]);
        if (c0 < 0 || c1 < 0) {
            return false;
        }

        int c2 = text[i + 2] == '=' ? -2 : decodeBase64Char(text[i + 2]);
        int c3 = text[i + 3] == '=' ? -2 : decodeBase64Char(text[i + 3]);
        if (c2 == -1 || c3 == -1) {
            return false;
        }

        uint32_t chunk = (static_cast<uint32_t>(c0) << 18) |
                         (static_cast<uint32_t>(c1) << 12) |
                         (static_cast<uint32_t>(c2 < 0 ? 0 : c2) << 6) |
                         static_cast<uint32_t>(c3 < 0 ? 0 : c3);

        out.push_back(static_cast<uint8_t>((chunk >> 16) & 0xFF));
        if (c2 >= 0) {
            out.push_back(static_cast<uint8_t>((chunk >> 8) & 0xFF));
        }
        if (c3 >= 0) {
            out.push_back(static_cast<uint8_t>(chunk & 0xFF));
        }
    }

    return true;
}

// Ensure the path is within the allowed root
static bool isPathSafe(const std::string& path) {
    try {
        auto canonical_path = fs::weakly_canonical(path).lexically_normal();
        auto root_path = fs::weakly_canonical(g_root_dir).lexically_normal();

        std::string canonical = canonical_path.string();
        std::string root = root_path.string();

        if (root == "/") {
            return true;
        }

        if (canonical == root) {
            return true;
        }

        if (!root.empty() && root.back() == '/') {
            root.pop_back();
        }

        return canonical.rfind(root + "/", 0) == 0;
    } catch (...) {
        return false;
    }
}

static void handleList(const json& msg) {
    std::string path = msg.value("path", g_root_dir);
    std::string reqId = msg.value("request_id", "");

    if (!isPathSafe(path)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied: path outside allowed root";
        sendMessage(err);
        return;
    }

    json result;
    result["action"] = "list_result";
    result["request_id"] = reqId;
    result["path"] = path;

    json entries = json::array();
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            json item;
            item["name"] = entry.path().filename().string();
            item["is_dir"] = entry.is_directory();
            item["is_file"] = entry.is_regular_file();
            if (entry.is_regular_file()) {
                item["size"] = entry.file_size();
            }
            // Get modification time in a portable way
            auto ftime = fs::last_write_time(entry);
            auto duration = ftime.time_since_epoch();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
            item["modified"] = seconds;
            entries.push_back(item);
        }
    } catch (const fs::filesystem_error& e) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = e.what();
        sendMessage(err);
        return;
    }

    result["entries"] = entries;
    sendMessage(result);
}

static void handleStat(const json& msg) {
    std::string path = msg.value("path", "");
    std::string reqId = msg.value("request_id", "");

    if (!isPathSafe(path)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied";
        sendMessage(err);
        return;
    }

    json result;
    result["action"] = "stat_result";
    result["request_id"] = reqId;

    try {
        auto status = fs::status(path);
        result["exists"] = fs::exists(status);
        result["is_dir"] = fs::is_directory(status);
        result["is_file"] = fs::is_regular_file(status);
        if (fs::is_regular_file(status)) {
            result["size"] = fs::file_size(path);
        }
    } catch (const fs::filesystem_error& e) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = e.what();
        sendMessage(err);
        return;
    }
    sendMessage(result);
}

static void handleRead(const json& msg) {
    std::string path = msg.value("path", "");
    std::string reqId = msg.value("request_id", "");

    if (!isPathSafe(path)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied";
        sendMessage(err);
        return;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Cannot open file: " + path;
        sendMessage(err);
        return;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    json result;
    result["action"] = "read_result";
    result["request_id"] = reqId;
    result["path"] = path;
    result["data"] = oss.str();
    sendMessage(result);
}

static void handleWrite(const json& msg) {
    std::string path = msg.value("path", "");
    std::string data = msg.value("data", "");
    std::string reqId = msg.value("request_id", "");

    if (!isPathSafe(path)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied";
        sendMessage(err);
        return;
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Cannot write file: " + path;
        sendMessage(err);
        return;
    }

    ofs.write(data.c_str(), static_cast<std::streamsize>(data.size()));
    json result;
    result["action"] = "success";
    result["request_id"] = reqId;
    result["message"] = "File written";
    sendMessage(result);
}

static void handleDelete(const json& msg) {
    std::string path = msg.value("path", "");
    std::string reqId = msg.value("request_id", "");

    if (!isPathSafe(path)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied";
        sendMessage(err);
        return;
    }

    try {
        fs::remove_all(path);
        json result;
        result["action"] = "success";
        result["request_id"] = reqId;
        result["message"] = "Deleted";
        sendMessage(result);
    } catch (const fs::filesystem_error& e) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = e.what();
        sendMessage(err);
    }
}

static void handleMkdir(const json& msg) {
    std::string path = msg.value("path", "");
    std::string reqId = msg.value("request_id", "");

    if (!isPathSafe(path)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied";
        sendMessage(err);
        return;
    }

    try {
        fs::create_directories(path);
        json result;
        result["action"] = "success";
        result["request_id"] = reqId;
        result["message"] = "Directory created";
        sendMessage(result);
    } catch (const fs::filesystem_error& e) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = e.what();
        sendMessage(err);
    }
}

static void handleRename(const json& msg) {
    std::string from = msg.value("from", "");
    std::string to = msg.value("to", "");
    std::string reqId = msg.value("request_id", "");

    if (!isPathSafe(from) || !isPathSafe(to)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied";
        sendMessage(err);
        return;
    }

    try {
        fs::rename(from, to);
        json result;
        result["action"] = "success";
        result["request_id"] = reqId;
        result["message"] = "Renamed";
        sendMessage(result);
    } catch (const fs::filesystem_error& e) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = e.what();
        sendMessage(err);
    }
}

static void handleUpload(const json& msg) {
    std::string path = msg.value("path", "");
    std::string reqId = msg.value("request_id", "");
    std::string dataB64 = msg.value("data_b64", "");

    if (!isPathSafe(path)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied";
        sendMessage(err);
        return;
    }

    std::vector<uint8_t> data;
    if (!decodeBase64(dataB64, data)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Invalid base64 payload";
        sendMessage(err);
        return;
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Cannot write file: " + path;
        sendMessage(err);
        return;
    }

    ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    json result;
    result["action"] = "success";
    result["request_id"] = reqId;
    result["message"] = "Upload completed";
    result["path"] = path;
    result["size"] = data.size();
    sendMessage(result);
}

static void handleDownload(const json& msg) {
    std::string path = msg.value("path", "");
    std::string reqId = msg.value("request_id", "");

    if (!isPathSafe(path)) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Access denied";
        sendMessage(err);
        return;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        json err;
        err["action"] = "error";
        err["request_id"] = reqId;
        err["message"] = "Cannot read file: " + path;
        sendMessage(err);
        return;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    json result;
    result["action"] = "download_result";
    result["request_id"] = reqId;
    result["path"] = path;
    result["filename"] = fs::path(path).filename().string();
    result["size"] = data.size();
    result["data_b64"] = encodeBase64(data);
    sendMessage(result);
}

static void handleMessage(const json& msg) {
    std::string action = msg.value("action", "");

    if (action == "list")        handleList(msg);
    else if (action == "stat")   handleStat(msg);
    else if (action == "read")   handleRead(msg);
    else if (action == "write")  handleWrite(msg);
    else if (action == "delete") handleDelete(msg);
    else if (action == "mkdir")  handleMkdir(msg);
    else if (action == "rename") handleRename(msg);
    else if (action == "upload") handleUpload(msg);
    else if (action == "download") handleDownload(msg);
    else if (action == "ping") {
        json pong;
        pong["action"] = "pong";
        sendMessage(pong);
    }
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    // Optional: restrict root directory via argument
    if (argc > 1) {
        g_root_dir = argv[1];
    }

    spdlog::info("File Manager plugin client started (root={})", g_root_dir);

    json ready;
    ready["action"] = "ready";
    ready["plugin"] = "file-manager";
    ready["role"] = "client";
    sendMessage(ready);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            json msg = json::parse(line);
            handleMessage(msg);
        } catch (const json::parse_error& e) {
            spdlog::error("JSON parse error: {}", e.what());
        }
    }

    spdlog::info("File Manager plugin client exiting");
    return 0;
}
