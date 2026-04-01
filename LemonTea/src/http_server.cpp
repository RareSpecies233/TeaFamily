#include "http_server.h"
#include "lemon_tea.h"
#include <httplib.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

struct UploadedArchive {
    fs::path tar_path;
    fs::path extract_dir;
    fs::path package_root;
};

struct FrontendInstallResult {
    bool success = false;
    std::string name;
    std::string error;
};

struct UnifiedPackageMeta {
    std::string name;
    std::string version;
    std::string plugin_type;
    bool has_frontend = false;
};

bool isValidPluginName(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    for (char c : name) {
        const auto uc = static_cast<unsigned char>(c);
        if (!(std::isalnum(uc) || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

std::string shellEscape(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out += "'";
    return out;
}

fs::path makeTempPath(const std::string& prefix, const std::string& suffix) {
    static std::atomic<uint64_t> seq{0};
    return fs::temp_directory_path() /
           (prefix + std::to_string(tea::nowMs()) + "_" + std::to_string(seq.fetch_add(1)) + suffix);
}

bool writeFile(const fs::path& path, const std::string& content, std::string& error) {
    try {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) {
            error = "cannot create temp file";
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out.good()) {
            error = "failed writing temp file";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool extractArchive(const fs::path& archive, const fs::path& dest, std::string& error) {
    fs::create_directories(dest);
    const std::string cmd =
        "tar -xzf " + shellEscape(archive.string()) + " -C " + shellEscape(dest.string());
    const int ret = std::system(cmd.c_str());
    if (ret != 0) {
        error = "failed to extract archive";
        return false;
    }
    return true;
}

std::optional<json> readJsonFile(const fs::path& path, std::string& error) {
    try {
        std::ifstream in(path);
        if (!in.is_open()) {
            error = "cannot open json file: " + path.string();
            return std::nullopt;
        }
        return json::parse(in);
    } catch (const std::exception& e) {
        error = e.what();
        return std::nullopt;
    }
}

std::optional<fs::path> detectPackageRoot(const fs::path& extract_dir) {
    if (fs::exists(extract_dir / "plugin.json") || fs::exists(extract_dir / "frontend" / "plugin.json")) {
        return extract_dir;
    }

    for (const auto& entry : fs::directory_iterator(extract_dir)) {
        if (!entry.is_directory()) {
            continue;
        }
        const fs::path candidate = entry.path();
        if (fs::exists(candidate / "plugin.json") || fs::exists(candidate / "frontend" / "plugin.json")) {
            return candidate;
        }
    }

    return std::nullopt;
}

bool copyDirectoryContents(const fs::path& from, const fs::path& to, std::string& error) {
    try {
        fs::create_directories(to);
        for (const auto& entry : fs::directory_iterator(from)) {
            const fs::path dst = to / entry.path().filename();
            fs::copy(entry.path(), dst,
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

std::optional<UploadedArchive> unpackArchive(const std::string& content,
                                             const std::string& prefix,
                                             std::string& error) {
    UploadedArchive archive;
    archive.tar_path = makeTempPath(prefix, ".tar.gz");
    archive.extract_dir = makeTempPath(prefix + "_extract_", "");

    if (!writeFile(archive.tar_path, content, error)) {
        return std::nullopt;
    }

    if (!extractArchive(archive.tar_path, archive.extract_dir, error)) {
        return std::nullopt;
    }

    auto root = detectPackageRoot(archive.extract_dir);
    if (!root) {
        error = "plugin.json not found in archive";
        return std::nullopt;
    }

    archive.package_root = *root;
    return archive;
}

void cleanupArchive(const UploadedArchive& archive) {
    std::error_code ec;
    if (!archive.tar_path.empty()) {
        fs::remove(archive.tar_path, ec);
    }
    if (!archive.extract_dir.empty()) {
        fs::remove_all(archive.extract_dir, ec);
    }
}

std::string inferRuntimePluginName(const UploadedArchive& archive, const std::string& fallback) {
    std::string error;
    auto root_manifest = readJsonFile(archive.package_root / "plugin.json", error);
    if (root_manifest) {
        const std::string name = root_manifest->value("name", std::string(""));
        if (isValidPluginName(name)) {
            return name;
        }
    }
    std::string stem = fs::path(fallback).stem().string();
    if (isValidPluginName(stem)) {
        return stem;
    }
    return {};
}

FrontendInstallResult installFrontendFromArchive(const UploadedArchive& archive,
                                                 const std::string& frontend_plugins_dir,
                                                 const std::string& fallback_name = "") {
    FrontendInstallResult result;

    fs::path source_dir;
    fs::path manifest_path;

    if (fs::exists(archive.package_root / "frontend" / "plugin.json")) {
        source_dir = archive.package_root / "frontend";
        manifest_path = source_dir / "plugin.json";
    } else if (fs::exists(archive.package_root / "plugin.json")) {
        source_dir = archive.package_root;
        manifest_path = source_dir / "plugin.json";
    } else {
        result.error = "frontend manifest not found";
        return result;
    }

    std::string error;
    auto manifest = readJsonFile(manifest_path, error);
    if (!manifest) {
        result.error = "invalid frontend manifest: " + error;
        return result;
    }

    if (source_dir == archive.package_root && !manifest->contains("entry")) {
        result.error = "archive does not contain frontend plugin manifest";
        return result;
    }

    std::string plugin_name = manifest->value("name", fallback_name);
    if (!isValidPluginName(plugin_name)) {
        result.error = "invalid plugin name in frontend manifest";
        return result;
    }

    (*manifest)["name"] = plugin_name;

    try {
        std::ofstream out(manifest_path);
        out << manifest->dump(2);
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }

    const fs::path dest_dir = fs::path(frontend_plugins_dir) / plugin_name;
    std::error_code ec;
    fs::remove_all(dest_dir, ec);
    fs::create_directories(dest_dir, ec);

    if (!copyDirectoryContents(source_dir, dest_dir, error)) {
        result.error = "failed to copy frontend files: " + error;
        return result;
    }

    result.success = true;
    result.name = plugin_name;
    return result;
}

bool parseUnifiedMeta(const UploadedArchive& archive, UnifiedPackageMeta& meta, std::string& error) {
    auto root_manifest = readJsonFile(archive.package_root / "plugin.json", error);
    if (!root_manifest) {
        return false;
    }

    meta.name = root_manifest->value("name", std::string(""));
    meta.version = root_manifest->value("version", std::string(""));
    meta.plugin_type = root_manifest->value("plugin_type", std::string("distributed"));
    meta.has_frontend = root_manifest->value("has_frontend", false) ||
                        fs::exists(archive.package_root / "frontend");

    if (!isValidPluginName(meta.name)) {
        error = "invalid plugin name in plugin.json";
        return false;
    }

    if (meta.plugin_type != "distributed" && meta.plugin_type != "lemon-only" &&
        meta.plugin_type != "honey-only") {
        meta.plugin_type = "distributed";
    }

    return true;
}

std::vector<std::string> connectedHoneyNodes(const LemonTea& lemon) {
    std::vector<std::string> connected_nodes;
    for (const auto& client : lemon.getClients()) {
        if (client.connected) {
            connected_nodes.push_back(client.node_id);
        }
    }
    return connected_nodes;
}

}  // namespace

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
        // Give HoneyTea a short time to respond so the frontend gets fresher state.
        std::this_thread::sleep_for(std::chrono::milliseconds(160));
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
        std::string name = req.has_param("name") ? req.get_param_value("name") : "";

        std::string parse_error;
        auto archive = unpackArchive(file.content, "tea_local_plugin_", parse_error);
        if (!archive) {
            res.status = 400;
            res.set_content(json{{"error", parse_error}}.dump(), "application/json");
            return;
        }

        if (name.empty()) {
            name = inferRuntimePluginName(*archive, file.filename);
        }
        cleanupArchive(*archive);

        if (!isValidPluginName(name)) {
            res.status = 400;
            res.set_content(json{{"error", "invalid plugin name"}}.dump(), "application/json");
            return;
        }

        std::vector<uint8_t> data(file.content.begin(), file.content.end());
        bool ok = lemon_.installPlugin(name, data);
        res.set_content(json{{"success", ok}, {"name", name}}.dump(), "application/json");
    });

    svr.Post("/api/plugins/inspect", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_file("plugin")) {
            res.status = 400;
            res.set_content(R"({"error":"missing plugin file"})", "application/json");
            return;
        }

        auto file = req.get_file_value("plugin");
        std::string parse_error;
        auto archive = unpackArchive(file.content, "tea_inspect_plugin_", parse_error);
        if (!archive) {
            res.status = 400;
            res.set_content(json{{"error", parse_error}}.dump(), "application/json");
            return;
        }

        UnifiedPackageMeta meta;
        if (!parseUnifiedMeta(*archive, meta, parse_error)) {
            cleanupArchive(*archive);
            res.status = 400;
            res.set_content(json{{"error", parse_error}}.dump(), "application/json");
            return;
        }

        auto root_manifest = readJsonFile(archive->package_root / "plugin.json", parse_error);
        if (!root_manifest) {
            cleanupArchive(*archive);
            res.status = 400;
            res.set_content(json{{"error", "invalid plugin manifest: " + parse_error}}.dump(), "application/json");
            return;
        }

        json frontend_manifest = json::object();
        if (fs::exists(archive->package_root / "frontend" / "plugin.json")) {
            auto frontend = readJsonFile(archive->package_root / "frontend" / "plugin.json", parse_error);
            if (frontend) {
                frontend_manifest = *frontend;
            }
        }

        auto connected_nodes = connectedHoneyNodes(lemon_);
        json info = {
            {"name", meta.name},
            {"version", meta.version},
            {"plugin_type", meta.plugin_type},
            {"description", root_manifest->value("description", std::string(""))},
            {"author", root_manifest->value("author", std::string(""))},
            {"depends_on", root_manifest->value("depends_on", json::array())},
            {"capabilities", root_manifest->value("capabilities", json::array())},
            {"has_frontend", meta.has_frontend},
            {"frontend", frontend_manifest},
            {"distribution_targets", json::array({"LemonTea", "HoneyTea", "OrangeTea"})},
            {"connected_honey_nodes", connected_nodes},
            {"connected_honey_count", connected_nodes.size()},
            {"file_name", file.filename},
            {"file_size", file.content.size()}
        };

        cleanupArchive(*archive);
        res.set_content(json{{"success", true}, {"plugin", info}}.dump(), "application/json");
    });

    svr.Post("/api/plugins/install-unified", [this](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_file("plugin")) {
            res.status = 400;
            res.set_content(R"({"error":"missing plugin file"})", "application/json");
            return;
        }

        auto file = req.get_file_value("plugin");
        std::string parse_error;
        auto archive = unpackArchive(file.content, "tea_unified_plugin_", parse_error);
        if (!archive) {
            res.status = 400;
            res.set_content(json{{"error", parse_error}}.dump(), "application/json");
            return;
        }

        UnifiedPackageMeta meta;
        if (!parseUnifiedMeta(*archive, meta, parse_error)) {
            cleanupArchive(*archive);
            res.status = 400;
            res.set_content(json{{"error", parse_error}}.dump(), "application/json");
            return;
        }

        std::vector<uint8_t> runtime_data(file.content.begin(), file.content.end());

        bool local_ok = lemon_.installPlugin(meta.name, runtime_data);

        bool frontend_ok = false;
        std::string frontend_name;
        std::string frontend_error;
        auto frontend = installFrontendFromArchive(*archive, lemon_.frontendPluginsDir(), meta.name);
        frontend_ok = frontend.success;
        frontend_name = frontend.name;
        frontend_error = frontend.error;
        if (!frontend_ok) {
            spdlog::warn("Unified frontend install failed for {}: {}", meta.name, frontend_error);
        }

        std::vector<json> remote_results;
        bool remote_ok = true;
        std::vector<std::string> warnings;

        auto targets = connectedHoneyNodes(lemon_);
        if (targets.empty()) {
            remote_ok = false;
            warnings.push_back("no online HoneyTea clients, remote install skipped");
        }
        for (const auto& node_id : targets) {
            bool ok = lemon_.installRemotePlugin(node_id, meta.name, runtime_data);
            remote_results.push_back({{"node_id", node_id}, {"success", ok}});
            remote_ok = remote_ok && ok;
        }

        cleanupArchive(*archive);

        const bool overall_ok = local_ok && frontend_ok && remote_ok;
        res.set_content(json{
            {"success", overall_ok},
            {"name", meta.name},
            {"version", meta.version},
            {"plugin_type", meta.plugin_type},
            {"local_installed", local_ok},
            {"frontend_installed", frontend_ok},
            {"frontend_name", frontend_name},
            {"frontend_error", frontend_error},
            {"remote_results", remote_results},
            {"warnings", warnings}
        }.dump(), "application/json");
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
        std::string name = req.has_param("name") ? req.get_param_value("name") : "";

        std::string parse_error;
        auto archive = unpackArchive(file.content, "tea_remote_plugin_", parse_error);
        if (!archive) {
            res.status = 400;
            res.set_content(json{{"error", parse_error}}.dump(), "application/json");
            return;
        }

        if (name.empty()) {
            name = inferRuntimePluginName(*archive, file.filename);
        }
        cleanupArchive(*archive);

        if (!isValidPluginName(name)) {
            res.status = 400;
            res.set_content(json{{"error", "invalid plugin name"}}.dump(), "application/json");
            return;
        }

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
        std::string fallback_name = req.has_param("name") ? req.get_param_value("name") : "";

        std::string parse_error;
        auto archive = unpackArchive(file.content, "tea_frontend_plugin_", parse_error);
        if (!archive) {
            res.status = 400;
            res.set_content(json{{"error", parse_error}}.dump(), "application/json");
            return;
        }

        auto result = installFrontendFromArchive(*archive, lemon_.frontendPluginsDir(), fallback_name);
        cleanupArchive(*archive);

        if (!result.success) {
            res.status = 400;
            res.set_content(json{{"error", result.error}}.dump(), "application/json");
            return;
        }

        res.set_content(json{{"success", true}, {"name", result.name}}.dump(), "application/json");
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
