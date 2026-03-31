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
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::mutex g_mutex;

// Configurable root directory to restrict access
static std::string g_root_dir = "/";

static void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

// Ensure the path is within the allowed root
static bool isPathSafe(const std::string& path) {
    try {
        auto canonical = fs::weakly_canonical(path);
        auto root = fs::weakly_canonical(g_root_dir);
        auto [rootEnd, _] = std::mismatch(root.string().begin(), root.string().end(),
                                           canonical.string().begin(), canonical.string().end());
        return rootEnd == root.string().end();
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
            auto ftime = fs::last_write_time(entry);
            auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
                std::chrono::file_clock::to_sys(ftime));
            item["modified"] = sctp.time_since_epoch().count();
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

static void handleMessage(const json& msg) {
    std::string action = msg.value("action", "");

    if (action == "list")        handleList(msg);
    else if (action == "stat")   handleStat(msg);
    else if (action == "read")   handleRead(msg);
    else if (action == "write")  handleWrite(msg);
    else if (action == "delete") handleDelete(msg);
    else if (action == "mkdir")  handleMkdir(msg);
    else if (action == "rename") handleRename(msg);
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
