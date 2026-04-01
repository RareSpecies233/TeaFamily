/**
 * SSH Plugin - Client Side (runs on HoneyTea)
 *
 * Opens a PTY (pseudo-terminal) to provide local shell access.
 * Receives input from server-side plugin (via parent stdio),
 * writes to PTY, and sends PTY output back.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#if __has_include(<util.h>)
#include <util.h>         // macOS
#elif __has_include(<pty.h>)
#include <pty.h>          // Linux
#endif

using json = nlohmann::json;

static std::mutex g_out_mutex;

struct PtySession {
    int master_fd = -1;
    pid_t child_pid = -1;
    std::atomic<bool> active{false};
    std::thread reader;
};

static std::mutex g_sessions_mutex;
static std::unordered_map<std::string, std::unique_ptr<PtySession>> g_sessions;

static void sendMessage(const json& msg) {
    std::lock_guard<std::mutex> lock(g_out_mutex);
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

static void ptyReaderThread(const std::string& sessionId, int masterFd) {
    char buf[4096];
    while (true) {
        ssize_t n = read(masterFd, buf, sizeof(buf));
        if (n <= 0) break;
        std::string data(buf, static_cast<size_t>(n));
        json msg;
        msg["action"] = "output";
        msg["session_id"] = sessionId;
        msg["data"] = data;
        sendMessage(msg);
    }

    json msg;
    msg["action"] = "session_ended";
    msg["session_id"] = sessionId;
    sendMessage(msg);
}

static void createSession(const std::string& sessionId, int cols, int rows) {
    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    int masterFd;
    pid_t pid;

    pid = forkpty(&masterFd, nullptr, nullptr, &ws);
    if (pid < 0) {
        spdlog::error("forkpty failed: {}", strerror(errno));
        json err;
        err["action"] = "error";
        err["session_id"] = sessionId;
        err["message"] = "Failed to create PTY";
        sendMessage(err);
        return;
    }

    if (pid == 0) {
        // Child process: exec shell
        const char* shell = getenv("SHELL");
        if (!shell) shell = "/bin/sh";
        setenv("TERM", "xterm-256color", 1);
        execlp(shell, shell, "-l", nullptr);
        _exit(1);
    }

    // Parent
    auto session = std::make_unique<PtySession>();
    session->master_fd = masterFd;
    session->child_pid = pid;
    session->active = true;
    session->reader = std::thread(ptyReaderThread, sessionId, masterFd);

    {
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        g_sessions[sessionId] = std::move(session);
    }

    spdlog::info("PTY session created: {} (pid={})", sessionId, pid);
}

static void closeSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(g_sessions_mutex);
    auto it = g_sessions.find(sessionId);
    if (it == g_sessions.end()) return;

    auto& session = it->second;
    session->active = false;
    if (session->master_fd >= 0) {
        close(session->master_fd);
        session->master_fd = -1;
    }
    if (session->child_pid > 0) {
        kill(session->child_pid, SIGTERM);
        waitpid(session->child_pid, nullptr, WNOHANG);
    }
    if (session->reader.joinable()) {
        session->reader.detach();
    }
    g_sessions.erase(it);
    spdlog::info("PTY session closed: {}", sessionId);
}

static void handleMessage(const json& msg) {
    std::string action = msg.value("action", "");

    if (action == "create_session") {
        std::string sessionId = msg.value("session_id", "");
        int cols = msg.value("cols", 80);
        int rows = msg.value("rows", 24);
        createSession(sessionId, cols, rows);

    } else if (action == "input") {
        std::string sessionId = msg.value("session_id", "");
        std::string data = msg.value("data", "");
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        auto it = g_sessions.find(sessionId);
        if (it != g_sessions.end() && it->second->master_fd >= 0) {
            write(it->second->master_fd, data.c_str(), data.size());
        }

    } else if (action == "resize") {
        std::string sessionId = msg.value("session_id", "");
        int cols = msg.value("cols", 80);
        int rows = msg.value("rows", 24);
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        auto it = g_sessions.find(sessionId);
        if (it != g_sessions.end() && it->second->master_fd >= 0) {
            struct winsize ws {};
            ws.ws_col = static_cast<unsigned short>(cols);
            ws.ws_row = static_cast<unsigned short>(rows);
            ioctl(it->second->master_fd, TIOCSWINSZ, &ws);
        }

    } else if (action == "close_session") {
        std::string sessionId = msg.value("session_id", "");
        closeSession(sessionId);

    } else if (action == "ping") {
        json pong;
        pong["action"] = "pong";
        sendMessage(pong);
    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("SSH plugin client started");

    json ready;
    ready["action"] = "ready";
    ready["plugin"] = "ssh";
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

    // Cleanup all sessions
    {
        std::lock_guard<std::mutex> lock(g_sessions_mutex);
        for (auto& [id, session] : g_sessions) {
            if (session->master_fd >= 0) close(session->master_fd);
            if (session->child_pid > 0) kill(session->child_pid, SIGTERM);
        }
        g_sessions.clear();
    }

    spdlog::info("SSH plugin client exiting");
    return 0;
}
