#pragma once

// LemonTea uses the same PluginManager as HoneyTea
// This is a copy with identical interface since both share the plugin interface
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <tea/common.h>
#include <tea/plugin_interface.h>
#include <tea/process_manager.h>
#include <nlohmann/json.hpp>

class PluginManager {
public:
    PluginManager(const std::string& plugins_dir);
    ~PluginManager();

    void scanPlugins();
    bool installPlugin(const std::string& name, const std::vector<uint8_t>& data);
    bool uninstallPlugin(const std::string& name);
    bool startPlugin(const std::string& name);
    bool stopPlugin(const std::string& name);
    void stopAll();

    std::vector<tea::PluginInfo> getPlugins() const;
    tea::PluginInfo getPlugin(const std::string& name) const;
    bool hasPlugin(const std::string& name) const;
    std::vector<tea::PluginManifest> getManifests() const;

    bool sendToPlugin(const std::string& name, const tea::json& msg);

    using PluginOutputCallback = std::function<void(const std::string& plugin, const tea::json& msg)>;
    void setPluginOutputCallback(PluginOutputCallback cb) { on_plugin_output_ = std::move(cb); }

    void startMonitor();
    void stopMonitor();

private:
    void loadManifest(const std::string& dir);

    std::string plugins_dir_;
    mutable std::mutex mutex_;
    std::map<std::string, tea::PluginManifest> manifests_;
    tea::ProcessManager process_mgr_;
    PluginOutputCallback on_plugin_output_;
};
