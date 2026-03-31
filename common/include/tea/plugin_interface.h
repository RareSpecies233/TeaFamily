#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace tea {

using json = nlohmann::json;

// Plugin manifest structure (plugin.json)
// {
//   "name": "ssh",
//   "version": "1.0.0",
//   "description": "SSH remote terminal plugin",
//   "binary": "ssh_plugin",
//   "comm_type": "stdio",         // "stdio" | "tcp" | "udp"
//   "comm_port": 0,               // only for tcp/udp
//   "auto_start": false,
//   "has_frontend": true,
//   "frontend_entry": "SSHView.vue",
//   "frontend_files": ["SSHView.vue"],
//   "capabilities": ["terminal"],
//   "role": "both"                // "client" | "server" | "both"
// }

struct PluginManifest {
    std::string name;
    std::string version;
    std::string description;
    std::string binary;
    std::string comm_type = "stdio";
    uint16_t    comm_port = 0;
    bool        auto_start = false;
    bool        has_frontend = false;
    std::string frontend_entry;
    std::vector<std::string> frontend_files;
    std::vector<std::string> capabilities;
    std::string role = "both";  // client, server, both

    static PluginManifest fromJson(const json& j) {
        PluginManifest m;
        m.name = j.value("name", "");
        m.version = j.value("version", "0.0.0");
        m.description = j.value("description", "");
        m.binary = j.value("binary", "");
        m.comm_type = j.value("comm_type", "stdio");
        m.comm_port = j.value("comm_port", 0);
        m.auto_start = j.value("auto_start", false);
        m.has_frontend = j.value("has_frontend", false);
        m.frontend_entry = j.value("frontend_entry", "");
        m.frontend_files = j.value("frontend_files", std::vector<std::string>{});
        m.capabilities = j.value("capabilities", std::vector<std::string>{});
        m.role = j.value("role", "both");
        return m;
    }

    json toJson() const {
        return json{
            {"name", name},
            {"version", version},
            {"description", description},
            {"binary", binary},
            {"comm_type", comm_type},
            {"comm_port", comm_port},
            {"auto_start", auto_start},
            {"has_frontend", has_frontend},
            {"frontend_entry", frontend_entry},
            {"frontend_files", frontend_files},
            {"capabilities", capabilities},
            {"role", role}
        };
    }
};

// Standard messages between parent and child process (stdio JSON-line protocol)
// Parent -> Child:
//   {"cmd":"start"}
//   {"cmd":"stop"}
//   {"cmd":"status"}
//   {"cmd":"message","from":"<node_id>","data":{...}}
//
// Child -> Parent:
//   {"event":"ready"}
//   {"event":"status","state":"running","info":{...}}
//   {"event":"message","target":"<node_id>","data":{...}}
//   {"event":"error","message":"..."}
//   {"event":"stopped"}

} // namespace tea
