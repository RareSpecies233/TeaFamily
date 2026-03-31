#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <tea/common.h>
#include <tea/config.h>
#include <tea/plugin_interface.h>
#include <tea/process_manager.h>
#include <nlohmann/json.hpp>

namespace tea { class TcpConnection; class UdpSocket; }

class PluginManager {
public:
    PluginManager(const std::string& plugins_dir);
    ~PluginManager();

    // Scan plugins directory for manifests
    void scanPlugins();

    // Install a plugin from tarball data
    bool installPlugin(const std::string& name, const std::vector<uint8_t>& data);

    // Uninstall
    bool uninstallPlugin(const std::string& name);

    // Start/stop
    bool startPlugin(const std::string& name);
    bool stopPlugin(const std::string& name);
    void stopAll();

    // Get info
    std::vector<tea::PluginInfo> getPlugins() const;
    tea::PluginInfo getPlugin(const std::string& name) const;
    bool hasPlugin(const std::string& name) const;

    // Get manifests
    std::vector<tea::PluginManifest> getManifests() const;

    // Forward message from network to plugin
    bool sendToPlugin(const std::string& name, const tea::json& msg);

    // Callback when plugin sends a message out
    using PluginOutputCallback = std::function<void(const std::string& plugin, const tea::json& msg)>;
    void setPluginOutputCallback(PluginOutputCallback cb) { on_plugin_output_ = std::move(cb); }

    // Start monitoring
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
