#include "plugin_manager.h"
#include <spdlog/spdlog.h>
#include <tea/logger.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace fs = std::filesystem;

PluginManager::PluginManager(const std::string& plugins_dir)
    : plugins_dir_(plugins_dir) {
    fs::create_directories(plugins_dir_);
}

PluginManager::~PluginManager() {
    stopAll();
    stopMonitor();
}

void PluginManager::scanPlugins() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!fs::exists(plugins_dir_)) return;

    for (const auto& entry : fs::directory_iterator(plugins_dir_)) {
        if (entry.is_directory()) {
            loadManifest(entry.path().string());
        }
    }
    spdlog::info("PluginManager: scanned {} plugins", manifests_.size());
}

void PluginManager::loadManifest(const std::string& dir) {
    std::string manifest_path = dir + "/plugin.json";
    if (!fs::exists(manifest_path)) return;

    try {
        std::ifstream f(manifest_path);
        auto j = tea::json::parse(f);
        auto manifest = tea::PluginManifest::fromJson(j);
        if (manifest.name.empty()) return;

        // LemonTea plugin run policy.
        std::string plugin_type = j.value("plugin_type", manifest.plugin_type);
        std::string role = j.value("role", std::string("both"));
        bool run_on_lemon = (plugin_type != "honey-only");
        if (!j.contains("plugin_type")) {
            run_on_lemon = (role != "client");
        }

        std::string binary_rel = manifest.binary;
        if (binary_rel.empty()) {
            binary_rel = manifest.server_binary;
        }
        if (binary_rel.empty()) {
            binary_rel = j.value("server_binary", std::string(""));
        }

        std::string binary;
        if (!binary_rel.empty()) {
            binary = dir + "/" + binary_rel;
            if (!fs::exists(binary)) {
                binary = dir + "/" + fs::path(binary_rel).filename().string();
            }
            if (fs::exists(binary)) {
                binary = fs::absolute(binary).lexically_normal().string();
            }
        }

        manifests_[manifest.name] = manifest;

        if (run_on_lemon && !process_mgr_.hasProcess(manifest.name)) {
            if (binary.empty() || !fs::exists(binary)) {
                spdlog::warn("Server plugin binary not found for {}: {}", manifest.name,
                            binary_rel.empty() ? std::string("<empty>") : binary_rel);
                return;
            }
            process_mgr_.registerProcess(manifest.name, binary, {}, dir);
        }
        spdlog::info("Plugin loaded: {} v{}", manifest.name, manifest.version);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load manifest {}: {}", manifest_path, e.what());
    }
}

bool PluginManager::installPlugin(const std::string& name, const std::vector<uint8_t>& data) {
    std::string plugin_dir = plugins_dir_ + "/" + name;
    std::string tar_path = "/tmp/tea_plugin_" + name + ".tar.gz";

    try {
        std::ofstream out(tar_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        out.close();

        fs::create_directories(plugin_dir);
        std::string cmd = "tar -xzf " + tar_path + " -C " + plugin_dir;
        int ret = std::system(cmd.c_str());
        fs::remove(tar_path);

        if (ret != 0) {
            spdlog::error("Failed to extract plugin: {}", name);
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        loadManifest(plugin_dir);
        spdlog::info("Plugin installed: {}", name);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Plugin install failed: {}", e.what());
        return false;
    }
}

bool PluginManager::uninstallPlugin(const std::string& name) {
    stopPlugin(name);
    std::lock_guard<std::mutex> lock(mutex_);
    process_mgr_.unregisterProcess(name);
    manifests_.erase(name);

    try {
        fs::remove_all(plugins_dir_ + "/" + name);
        return true;
    } catch (...) {
        return false;
    }
}

bool PluginManager::startPlugin(const std::string& name) {
    process_mgr_.setOutputCallback([this](const std::string& pname, const std::string& line) {
        // Log every line of plugin stdout/stderr to a plugin-specific sub-logger
        auto plog = tea::Logger::getPluginLogger(pname);
        try {
            auto j = tea::json::parse(line);
            std::string event = j.value("event", j.value("action", std::string("")));
            std::string message = j.value("message", std::string(""));
            if (event == "error") {
                plog->error("[stdout] {}", line);
            } else if (event == "log" && !message.empty()) {
                plog->info("[stdout] {}", message);
            } else {
                plog->info("[stdout] {}", line);
            }
            if (on_plugin_output_) {
                on_plugin_output_(pname, j);
            }
        } catch (...) {
            // Non-JSON output — treat as raw log line
            plog->info("[stdout] {}", line);
            if (on_plugin_output_) {
                on_plugin_output_(pname, {{"event", "log"}, {"message", line}});
            }
        }
    });

    spdlog::info("Starting plugin '{}' ...", name);
    bool ok = process_mgr_.startProcess(name);
    if (ok) {
        tea::json cmd = {{"cmd", "start"}};
        process_mgr_.sendToProcess(name, cmd.dump());
        spdlog::info("Plugin '{}' start command sent", name);
    } else {
        spdlog::error("Failed to start plugin '{}'", name);
    }
    return ok;
}

bool PluginManager::stopPlugin(const std::string& name) {
    spdlog::info("Stopping plugin '{}' ...", name);
    tea::json cmd = {{"cmd", "stop"}};
    process_mgr_.sendToProcess(name, cmd.dump());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    bool ok = process_mgr_.stopProcess(name);
    spdlog::info("Plugin '{}' stopped: {}", name, ok ? "ok" : "failed");
    return ok;
}

void PluginManager::stopAll() {
    process_mgr_.stopAll();
}

std::vector<tea::PluginInfo> PluginManager::getPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<tea::PluginInfo> result;
    for (const auto& [name, manifest] : manifests_) {
        tea::PluginInfo info;
        info.name = manifest.name;
        info.version = manifest.version;
        info.description = manifest.description;
        info.plugin_type = manifest.plugin_type;
        info.binary_path = manifest.binary;
        info.comm_type = manifest.comm_type;
        info.comm_port = manifest.comm_port;
        info.auto_start = manifest.auto_start;
        info.has_frontend = manifest.has_frontend;
        info.frontend_path = manifest.frontend_entry;
        auto proc = process_mgr_.getProcessInfo(name);
        info.state = proc.state;
        info.pid = proc.pid;
        result.push_back(info);
    }
    return result;
}

tea::PluginInfo PluginManager::getPlugin(const std::string& name) const {
    auto all = getPlugins();
    for (auto& p : all) if (p.name == name) return p;
    return {};
}

bool PluginManager::hasPlugin(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return manifests_.count(name) > 0;
}

std::vector<tea::PluginManifest> PluginManager::getManifests() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<tea::PluginManifest> result;
    for (const auto& [n, m] : manifests_) result.push_back(m);
    return result;
}

bool PluginManager::sendToPlugin(const std::string& name, const tea::json& msg) {
    return process_mgr_.sendToProcess(name, msg.dump());
}

void PluginManager::startMonitor() { process_mgr_.startMonitor(); }
void PluginManager::stopMonitor() { process_mgr_.stopMonitor(); }
