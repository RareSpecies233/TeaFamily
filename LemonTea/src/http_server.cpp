#include "http_server.h"
#include "lemon_tea.h"
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

struct HttpApi::Impl {
    httplib::Server svr;
};

HttpApi::HttpApi(LemonTea& lemon, uint16_t port)
    : lemon_(lemon), port_(port), impl_(std::make_unique<Impl>()) {}

HttpApi::~HttpApi() {
    stop();
}

bool HttpApi::start() {
    setupRoutes();
    running_.store(true);
    server_thread_ = std::thread([this]() {
        spdlog::info("HTTP API listening on 0.0.0.0:{}", port_);
        if (!impl_->svr.listen("0.0.0.0", port_)) {
            spdlog::error("HTTP server failed to start on port {}", port_);
        }
    });
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

void HttpApi::stop() {
    running_.store(false);
    impl_->svr.stop();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

void HttpApi::setupRoutes() {
    auto& svr = impl_->svr;

    // CORS middleware
    svr.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // ===== Status =====
    svr.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        json status = {
            {"name", "LemonTea"},
            {"node_id", lemon_.config().get<std::string>("node_id", "")},
            {"status", "running"},
            {"clients_count", lemon_.getClients().size()},
            {"plugins_count", lemon_.getPlugins().size()},
            {"timestamp", tea::nowMs()}
        };
        res.set_content(status.dump(), "application/json");
    });

    // ===== Clients (HoneyTea instances) =====
    svr.Get("/api/clients", [this](const httplib::Request&, httplib::Response& res) {
        json list = json::array();
        for (const auto& c : lemon_.getClients()) {
            list.push_back({
                {"node_id", c.node_id},
                {"address", c.address},
                {"port", c.port},
                {"connected", c.connected},
                {"last_heartbeat", c.last_heartbeat},
                {"plugins", c.cached_plugins}
            });
        }
        res.set_content(json{{"clients", list}}.dump(), "application/json");
    });

    svr.Get(R"(/api/clients/(\w[\w-]*)/plugins)", [this](const httplib::Request& req, httplib::Response& res) {
        auto node_id = req.matches[1].str();
        lemon_.requestRemotePluginList(node_id);
        // Return cached
        for (const auto& c : lemon_.getClients()) {
            if (c.node_id == node_id) {
                res.set_content(json{{"plugins", c.cached_plugins}}.dump(), "application/json");
                return;
            }
        }
        res.status = 404;
        res.set_content(R"({"error":"client not found"})", "application/json");
    });

    // ===== Local Plugins =====
    svr.Get("/api/plugins", [this](const httplib::Request&, httplib::Response& res) {
        json list = json::array();
        for (const auto& p : lemon_.getPlugins()) {
            list.push_back({
                {"name", p.name}, {"version", p.version},
                {"description", p.description},
                {"plugin_type", p.plugin_type},
                {"state", tea::pluginStateStr(p.state)},
                {"pid", p.pid}, {"has_frontend", p.has_frontend},
                {"auto_start", p.auto_start}
            });
        }
        res.set_content(json{{"plugins", list}}.dump(), "application/json");
    });

    svr.Post(R"(/api/plugins/(\w[\w-]*)/start)", [this](const httplib::Request& req, httplib::Response& res) {
        auto name = req.matches[1].str();
        bool ok = lemon_.startPlugin(name);
        res.set_content(json{{"success", ok}, {"name", name}}.dump(), "application/json");
    });

    svr.Post(R"(/api/plugins/(\w[\w-]*)/stop)", [this](const httplib::Request& req, httplib::Response& res) {
        auto name = req.matches[1].str();
        bool ok = lemon_.stopPlugin(name);
        res.set_content(json{{"success", ok}, {"name", name}}.dump(), "application/json");
    });

    svr.Post("/api/plugins/install", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_file("plugin")) {
            res.status = 400;
            res.set_content(R"({"error":"missing plugin file"})", "application/json");
            return;
        }
        auto file = req.get_file_value("plugin");
        std::string name = req.get_param_value("name");
        if (name.empty()) {
            name = fs::path(file.filename).stem().string();
        }

        std::vector<uint8_t> data(file.content.begin(), file.content.end());
        bool ok = lemon_.installPlugin(name, data);
        res.set_content(json{{"success", ok}, {"name", name}}.dump(), "application/json");
    });

    svr.Delete(R"(/api/plugins/(\w[\w-]*))", [this](const httplib::Request& req, httplib::Response& res) {
        auto name = req.matches[1].str();
        bool ok = lemon_.uninstallPlugin(name);
        res.set_content(json{{"success", ok}, {"name", name}}.dump(), "application/json");
    });

    // ===== Remote Plugin Operations (on HoneyTea) =====
    svr.Post(R"(/api/clients/(\w[\w-]*)/plugins/(\w[\w-]*)/start)",
        [this](const httplib::Request& req, httplib::Response& res) {
        auto node_id = req.matches[1].str();
        auto name = req.matches[2].str();
        bool ok = lemon_.startRemotePlugin(node_id, name);
        res.set_content(json{{"success", ok}}.dump(), "application/json");
    });

    svr.Post(R"(/api/clients/(\w[\w-]*)/plugins/(\w[\w-]*)/stop)",
        [this](const httplib::Request& req, httplib::Response& res) {
        auto node_id = req.matches[1].str();
        auto name = req.matches[2].str();
        bool ok = lemon_.stopRemotePlugin(node_id, name);
        res.set_content(json{{"success", ok}}.dump(), "application/json");
    });

    svr.Delete(R"(/api/clients/(\w[\w-]*)/plugins/(\w[\w-]*))",
        [this](const httplib::Request& req, httplib::Response& res) {
        auto node_id = req.matches[1].str();
        auto name = req.matches[2].str();
        bool ok = lemon_.uninstallRemotePlugin(node_id, name);
        res.set_content(json{{"success", ok}, {"name", name}}.dump(), "application/json");
    });

    svr.Post(R"(/api/clients/(\w[\w-]*)/plugins/install)",
        [this](const httplib::Request& req, httplib::Response& res) {
        auto node_id = req.matches[1].str();
        if (!req.has_file("plugin")) {
            res.status = 400;
            res.set_content(R"({"error":"missing plugin file"})", "application/json");
            return;
        }
        auto file = req.get_file_value("plugin");
        std::string name = req.get_param_value("name");
        if (name.empty()) name = fs::path(file.filename).stem().string();

        std::vector<uint8_t> data(file.content.begin(), file.content.end());
        bool ok = lemon_.installRemotePlugin(node_id, name, data);
        res.set_content(json{{"success", ok}, {"name", name}}.dump(), "application/json");
    });

    // ===== Plugin messaging (relay) =====
    svr.Post("/api/plugin-message", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string node_id = body.value("node_id", "");
            std::string plugin = body.value("plugin", "");
            auto data = body.value("data", json::object());
            bool ok = lemon_.sendPluginMessage(node_id, plugin, data);
            res.set_content(json{{"success", ok}}.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
        }
    });

    // ===== Binary Update =====
    svr.Post("/api/update/lemon-tea", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_file("binary")) {
            res.status = 400;
            res.set_content(R"({"error":"missing binary file"})", "application/json");
            return;
        }
        auto file = req.get_file_value("binary");
        std::string temp_path = "/tmp/lemontea_update_" + std::to_string(tea::nowMs());
        {
            std::ofstream out(temp_path, std::ios::binary);
            out.write(file.content.data(), file.content.size());
        }
        bool ok = lemon_.requestSelfUpdate(temp_path);
        res.set_content(json{{"success", ok}}.dump(), "application/json");
    });

    svr.Post(R"(/api/update/honey-tea/(\w[\w-]*))", [this](const httplib::Request& req, httplib::Response& res) {
        auto node_id = req.matches[1].str();
        if (!req.has_file("binary")) {
            res.status = 400;
            res.set_content(R"({"error":"missing binary file"})", "application/json");
            return;
        }
        auto file = req.get_file_value("binary");
        // Forward binary to HoneyTea via TCP file transfer
        std::vector<uint8_t> data(file.content.begin(), file.content.end());
        auto conn = lemon_.sendPluginMessage(node_id, "__update__",
            {{"size", data.size()}});
        res.set_content(json{{"success", true}, {"message", "update initiated"}}.dump(), "application/json");
    });

    // ===== Frontend Plugin Files =====
    svr.Get(R"(/api/frontend-plugins)", [this](const httplib::Request&, httplib::Response& res) {
        json list = json::array();
        std::string dir = lemon_.frontendPluginsDir();
        if (fs::exists(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_directory()) {
                    std::string manifest_path = entry.path().string() + "/plugin.json";
                    if (fs::exists(manifest_path)) {
                        try {
                            std::ifstream f(manifest_path);
                            auto j = json::parse(f);
                            list.push_back(j);
                        } catch (...) {}
                    }
                }
            }
        }
        res.set_content(json{{"plugins", list}}.dump(), "application/json");
    });

    svr.Get(R"(/api/frontend-plugins/(\w[\w-]*)/(.+))",
        [this](const httplib::Request& req, httplib::Response& res) {
        auto name = req.matches[1].str();
        auto file = req.matches[2].str();

        // Sanitize path to prevent directory traversal
        if (file.find("..") != std::string::npos) {
            res.status = 403;
            return;
        }

        std::string full_path = lemon_.frontendPluginsDir() + "/" + name + "/" + file;
        if (!fs::exists(full_path)) {
            res.status = 404;
            return;
        }

        std::ifstream f(full_path, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

        // Guess content type
        std::string ext = fs::path(file).extension().string();
        std::string ct = "application/octet-stream";
        if (ext == ".js")   ct = "application/javascript";
        if (ext == ".vue")  ct = "application/javascript";
        if (ext == ".css")  ct = "text/css";
        if (ext == ".html") ct = "text/html";
        if (ext == ".json") ct = "application/json";
        if (ext == ".svg")  ct = "image/svg+xml";

        res.set_content(content, ct);
    });

    // Upload frontend plugin
    svr.Post("/api/frontend-plugins/install", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_file("plugin")) {
            res.status = 400;
            res.set_content(R"({"error":"missing plugin file"})", "application/json");
            return;
        }
        auto file = req.get_file_value("plugin");
        std::string name = req.get_param_value("name");
        if (name.empty()) name = fs::path(file.filename).stem().string();

        std::string plugin_dir = lemon_.frontendPluginsDir() + "/" + name;
        std::string tar_path = "/tmp/tea_fe_plugin_" + name + ".tar.gz";

        try {
            {
                std::ofstream out(tar_path, std::ios::binary);
                out.write(file.content.data(), file.content.size());
            }
            fs::create_directories(plugin_dir);
            std::string cmd = "tar -xzf " + tar_path + " -C " + plugin_dir;
            int ret = std::system(cmd.c_str());
            fs::remove(tar_path);

            bool ok = (ret == 0);
            res.set_content(json{{"success", ok}, {"name", name}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Delete(R"(/api/frontend-plugins/(\w[\w-]*))", [this](const httplib::Request& req, httplib::Response& res) {
        auto name = req.matches[1].str();
        std::string plugin_dir = lemon_.frontendPluginsDir() + "/" + name;
        try {
            if (fs::exists(plugin_dir)) {
                fs::remove_all(plugin_dir);
            }
            res.set_content(json{{"success", true}, {"name", name}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"success", false}, {"error", e.what()}}.dump(), "application/json");
        }
    });

    spdlog::info("HTTP API routes configured");
}
