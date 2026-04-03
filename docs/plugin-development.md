# TeaFamily 插件开发指南

## 概述

TeaFamily 插件系统允许开发者扩展 HoneyTea（客户端）和 LemonTea（服务端）的功能。每个插件是一个独立的可执行文件，通过 **stdio JSON-line 协议** 与父进程通信。

## 插件结构

一个完整的插件包含以下部分：

```
plugins/my-plugin/
├── plugin.json              # 插件清单
├── server/                  # 可选：服务端组件（运行在 LemonTea 上）
│   ├── CMakeLists.txt
│   └── src/
│       └── main.cpp
├── client/                  # 可选：客户端组件（运行在 HoneyTea 上）
│   ├── CMakeLists.txt
│   └── src/
│       └── main.cpp
└── frontend/                # 可选：前端组件（动态加载到 OrangeTea）
    ├── plugin.json
    └── index.js
```

建议按插件类型组织目录：
- `distributed`：通常包含 `server/` + `client/`，可选 `frontend/`
- `lemon-only`：包含 `server/`，可选 `frontend/`，通常不包含 `client/`
- `honey-only`：包含 `client/`，可选 `frontend/`，通常不包含 `server/`

## 插件清单 (plugin.json)

根目录清单：

```json
{
  "name": "my-plugin",
  "version": "1.0.0",
  "description": "我的自定义插件",
  "author": "Your Name",
  "plugin_type": "distributed",
  "depends_on": ["another-plugin"],
  "capabilities": ["custom-feature"],
  "communication": "stdio",
  "has_frontend": true,
  "server_binary": "server/tea-plugin-my-server",
  "client_binary": "client/tea-plugin-my-client"
}
```

| 字段 | 必需 | 说明 |
|------|------|------|
| `name` | ✅ | 插件唯一标识符 |
| `version` | ✅ | 语义化版本号 |
| `description` | ✅ | 插件描述 |
| `author` | ❌ | 作者信息 |
| `plugin_type` | ✅ | 插件类型：`distributed` / `lemon-only` / `honey-only` |
| `depends_on` | ❌ | 依赖的其他 LemonTea 插件（用于插件间通信） |
| `capabilities` | ❌ | 功能标签列表 |
| `communication` | ✅ | 通信方式：`stdio` / `tcp` / `udp` |
| `has_frontend` | ❌ | 是否包含前端组件 |
| `server_binary` | ❌ | 服务端可执行文件相对路径 |
| `client_binary` | ❌ | 客户端可执行文件相对路径 |

说明：
- `distributed`：同一个插件需要在 LemonTea 与 HoneyTea 同时安装。
- `lemon-only`：插件只在 LemonTea 安装与运行，HoneyTea 不注册该插件进程。
- `honey-only`：插件只在 HoneyTea 安装与运行，LemonTea 不注册该插件进程。

## 通信协议

### stdio JSON-line 协议

插件通过 **stdin** 接收消息，通过 **stdout** 发送消息。每条消息是一行 JSON：

```
{"action": "some_action", "key": "value"}\n
```

**注意**：
- 每条消息以 `\n` 换行符结尾
- 不要向 stdout 输出非 JSON 内容
- 调试日志应写入 stderr 或使用 spdlog 的 stderr sink

### 必须实现的消息

#### 1. 就绪通知

插件启动后必须立即发送：

```json
{"action": "ready", "plugin": "my-plugin", "role": "server"}
```

`role` 为 `"server"` 或 `"client"`。

#### 2. 心跳响应

收到 `ping` 时必须回复 `pong`：

```json
// 收到
{"action": "ping"}
// 回复
{"action": "pong"}
```

### 自定义消息

可以定义任意 `action`，格式如下：

```json
{
  "action": "your_custom_action",
  "request_id": "uuid-or-number",
  "your_data": "..."
}
```

建议使用 `request_id` 来关联请求与响应。

## 构建插件

### CMakeLists.txt 模板

```cmake
cmake_minimum_required(VERSION 3.16)
project(tea-plugin-my-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(tea-plugin-my-server src/main.cpp)
target_link_libraries(tea-plugin-my-server PRIVATE
    tea_common
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
```

### C++ 代码模板

```cpp
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

void sendMessage(const json& msg) {
    std::cout << msg.dump() << "\n";
    std::cout.flush();
}

void handleMessage(const json& msg) {
    std::string action = msg.value("action", "");

    if (action == "ping") {
        sendMessage({{"action", "pong"}});
    } else if (action == "your_action") {
        // 处理自定义消息
        json result;
        result["action"] = "your_action_result";
        result["request_id"] = msg.value("request_id", "");
        result["data"] = "处理结果";
        sendMessage(result);
    }
}

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("My plugin started");

    // 发送就绪通知
    sendMessage({
        {"action", "ready"},
        {"plugin", "my-plugin"},
        {"role", "server"}  // 或 "client"
    });

    // 主循环：读取 stdin
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

    return 0;
}
```

## 前端插件开发

### 前端清单 (frontend/plugin.json)

```json
{
  "name": "my-plugin",
  "version": "1.0.0",
  "entry": "index.js",
  "title": "我的插件",
  "icon": "custom",
  "route": "/plugin/my-plugin",
  "description": "我的插件界面"
}
```

建议将 `entry` 指向可直接被浏览器加载的 ESM 文件（如 `index.js`），避免直接分发 `.vue` SFC 文件导致运行时无法解析。

### Vue 组件接口

前端组件接收以下 props：

```typescript
interface PluginProps {
  pluginName: string  // 插件名称
  apiBase: string     // API 基础路径（如 "/api"）
}
```

### 两种加载方式

#### 方式一：导出 Vue 组件（推荐）

```vue
<template>
  <div>我的插件界面</div>
</template>

<script setup lang="ts">
defineProps<{
  pluginName: string
  apiBase: string
}>()
</script>
```

#### 方式二：导出 mount 函数

```typescript
export function mount(container: HTMLElement, ctx: { pluginName: string; apiBase: string }) {
  container.innerHTML = '<div>我的插件</div>'

  // 返回卸载函数（可选）
  return () => {
    container.innerHTML = ''
  }
}
```

### 与后端通信

前端通过 LemonTea 的 HTTP API 与插件后端通信：

```typescript
// 发送请求
const resp = await fetch(`${apiBase}/plugins/my-plugin/custom-action`, {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ key: 'value' }),
})
const data = await resp.json()
```

## 安装与分发

### 打包

将插件打包为 **统一插件包**（`.tar.gz`，包含运行时与前端）：

```bash
cd plugins/my-plugin
tar czf my-plugin-1.0.0.tar.gz \
  plugin.json \
  server/tea-plugin-my-server \
  client/tea-plugin-my-client \
  frontend/
```

`lemon-only` 插件可不包含 `client/`；`honey-only` 插件可不包含 `server/`。

推荐直接使用项目脚本导出统一包：

```bash
./scripts/macOS_export_ssh_plugin.sh
./scripts/macOS_export_file_manager_plugin.sh
./scripts/macOS_export_monitor_plugin.sh
./scripts/macOS_export_cam_lan_stream_plugin.sh

./scripts/export_ssh_plugin_linux_x64_lemon_rpi5_honey.sh
./scripts/export_file_manager_plugin_linux_x64_lemon_rpi5_honey.sh
./scripts/export_monitor_plugin_linux_x64_lemon_rpi5_honey.sh
./scripts/export_cam_lan_stream_plugin_linux_x64.sh
```

分布式插件导出脚本支持平台选择（`linux-x64` / `rpi5` / `macos`）：

```bash
./scripts/export_ssh_plugin_linux_x64_lemon_rpi5_honey.sh \
  --lemon-platform linux-x64 \
  --honey-platform rpi5

./scripts/export_ssh_plugin_linux_x64_lemon_rpi5_honey.sh \
  --lemon-platform macos \
  --honey-platform macos
```

`lemon-only` 插件可选择 LemonTea 目标平台：

```bash
./scripts/export_cam_lan_stream_plugin_linux_x64.sh --lemon-platform linux-x64
./scripts/export_cam_lan_stream_plugin_linux_x64.sh --lemon-platform rpi5
./scripts/export_cam_lan_stream_plugin_linux_x64.sh --lemon-platform macos
```

导出结果示例：`ssh-unified-macos.tar.gz`

### 安装

通过 OrangeTea 的「统一插件管理台」上传后会先预览插件信息（名称/版本/依赖/能力），用户确认后再执行安装。

统一安装会根据 `plugin_type` 自动分发，无需手动选择分发目标：
- `distributed` -> LemonTea + 在线 HoneyTea + OrangeTea（若有前端）
- `lemon-only` -> LemonTea + OrangeTea（若有前端）
- `honey-only` -> 在线 HoneyTea + OrangeTea（若有前端）

也可通过 API：

```bash
curl -X POST http://localhost:9528/api/plugins/inspect \
  -F "plugin=@my-plugin-1.0.0.tar.gz"
```

确认后安装：

```bash
curl -X POST http://localhost:9528/api/plugins/install-unified \
  -F "plugin=@my-plugin-1.0.0.tar.gz"
```

## 安全建议

1. **路径校验**：文件操作插件必须校验路径，防止目录遍历攻击
2. **输入验证**：验证所有来自 stdin 的 JSON 输入
3. **权限控制**：使用最小权限运行插件进程
4. **资源限制**：避免无限制的内存/CPU 使用

## 调试技巧

1. 使用 `spdlog` 写入 stderr（不会影响 stdio 通信）
2. 单独运行插件可执行文件进行测试：
   ```bash
   echo '{"action":"ping"}' | ./tea-plugin-my-server
   ```
3. 检查 `logs/` 目录下的日志文件

## 参考

- [nlohmann/json 文档](https://github.com/nlohmann/json)
- [spdlog 文档](https://github.com/gabime/spdlog)
- [Element Plus 文档](https://element-plus.org/zh-CN/)
- [ECharts 文档](https://echarts.apache.org/zh/index.html)
