# TeaFamily - 分布式进程管理系统

## 概述

TeaFamily 是一套分布式进程管理系统，包含以下组件：

| 组件 | 语言 | 角色 |
|------|------|------|
| **GreenTea** | C++ | 守护进程 - 监控并自动重启其他组件 |
| **HoneyTea** | C++ | 客户端 - 管理本地子进程（插件） |
| **LemonTea** | C++ | 服务端 - 集中管理、HTTP API |
| **OrangeTea** | Vue3 | 前端界面 - 图形化管理所有组件 |

## 架构

```
┌─────────────┐     HTTP     ┌─────────────┐     TCP     ┌─────────────┐
│  OrangeTea  │◄────────────►│  LemonTea   │◄───────────►│  HoneyTea   │
│  (Vue3 UI)  │              │  (Server)   │             │  (Client)   │
└─────────────┘              └──────┬──────┘             └──────┬──────┘
                                    │                           │
                              ┌─────┴─────┐               ┌─────┴─────┐
                              │ GreenTea  │               │ GreenTea  │
                              │ (Daemon)  │               │ (Daemon)  │
                              └───────────┘               └───────────┘
                                    │                           │
                              ┌─────┴─────┐               ┌─────┴─────┐
                              │  Plugins  │               │  Plugins  │
                              └───────────┘               └───────────┘
```

## 构建

### macOS
```bash
./scripts/macOS_build_release.sh
./scripts/macOS_build_release.sh clean # 清理构建
```

### 跨平台目标构建（Linux x64 / Raspberry Pi 5）
```bash
./scripts/build_linux_x64_and_rpi5.sh
./scripts/build_linux_x64_and_rpi5.sh clean
```

默认会输出：
- `dist/cross-targets/linux-x64`（GreenTea + LemonTea）
- `dist/cross-targets/rpi5`（GreenTea + HoneyTea）

可选环境变量：
- `LINUX_X64_CC` / `LINUX_X64_CXX` / `LINUX_X64_SYSROOT`
- `RPI5_CC` / `RPI5_CXX` / `RPI5_SYSROOT`
- `BUILD_ROOT` / `DIST_ROOT`
- `TEA_CROSS_DEPS_ROOT`（交叉依赖缓存目录，默认 `build-cross/cross-deps`）
- `TEA_BROTLI_VERSION`（自动下载构建 Brotli 时使用的版本，默认 `1.1.0`）

说明：
- 脚本会优先从交叉编译器自动探测 `sysroot`，也支持通过 `*_SYSROOT` 显式指定。
- 目标 `sysroot` 必须包含 OpenSSL（不会通过关闭功能绕过）。
- 若目标 `sysroot` 缺少 Brotli，脚本会自动为对应目标交叉编译并注入 toolchain 搜索路径。

更多脚本说明见 [macOS 构建脚本说明](docs/macOS_build_release.md)

### 手动构建
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j
```

## 配置

每个组件都有独立的 `config.json` 配置文件，详见各组件目录。

## 插件

内置插件：
- **SSH插件** - 远程终端访问
- **文件管理插件** - 远程文件浏览与管理
- **监控插件** - 系统资源监控与图表展示
- **局域网摄像头串流插件** - 移动端摄像头采集与 OrangeTea 实时预览（仅 LemonTea 安装）

详见 [插件开发文档](docs/plugin-development.md)

插件使用文档：
- [SSH 插件](docs/plugin-ssh.md)
- [文件管理插件](docs/plugin-file-manager.md)
- [监控插件](docs/plugin-monitor.md)
- [局域网摄像头串流插件](docs/plugin-cam-lan-stream.md)

LemonTea HTTP API：
- [API 文档](docs/lemontea-api.md)

macOS 插件导出脚本：
```bash
./scripts/macOS_export_ssh_plugin.sh
./scripts/macOS_export_file_manager_plugin.sh
./scripts/macOS_export_monitor_plugin.sh
./scripts/macOS_export_cam_lan_stream_plugin.sh
```

跨目标插件导出脚本（LemonTea: Linux x64 / HoneyTea: Raspberry Pi 5）：
```bash
./scripts/export_ssh_plugin_linux_x64_lemon_rpi5_honey.sh
./scripts/export_file_manager_plugin_linux_x64_lemon_rpi5_honey.sh
./scripts/export_monitor_plugin_linux_x64_lemon_rpi5_honey.sh
```

LemonTea-only 插件导出脚本（Linux x64）：
```bash
./scripts/export_cam_lan_stream_plugin_linux_x64.sh
```

脚本会导出统一安装包（例如 `ssh-unified-macos.tar.gz`），用于 OrangeTea 的统一插件安装入口。

统一安装入口会根据 `plugin_type` 自动分发：
- `distributed` -> LemonTea + HoneyTea（在线节点）+ OrangeTea 前端（若有）
- `lemon-only` -> LemonTea + OrangeTea 前端（若有）
- `honey-only` -> HoneyTea（在线节点）+ OrangeTea 前端（若有）

默认 release 构建产物不会预装任何插件（`dist/plugins` 与 `dist/frontend_plugins` 初始为空），需通过 OrangeTea 上传统一插件包后安装。

## OrangeTea 前端

```bash
cd OrangeTea
npm install
npm run dev      # 开发模式
npm run build    # 生产构建
```

### OrangeTea Vue3 调试工具（Vue DevTools）

OrangeTea 已启用 Vue3 调试工具（`vite-plugin-vue-devtools`），开发模式下可在浏览器中更方便地调试组件状态。

建议步骤：

```bash
cd OrangeTea
npm install
npm run dev
```

然后在浏览器打开 OrangeTea 页面，配合浏览器扩展 `Vue.js devtools`（Chrome/Edge/Firefox）进行组件树、Pinia 状态和响应式数据调试。

## 部署（构建后二进制的运行方式）

下面说明如何将构建脚本 `scripts/macOS_build_release.sh` 生成的 `dist` 目录部署到服务器或客户端，并以多种方式启动服务。

### 准备
- 将 `dist` 目录复制或上传到目标主机（示例路径 `/opt/teafamily`）。
- 修改 `dist/config/*.json` 中的配置（如 `server_host`、`node_id`、端口等）以匹配你的环境。
- 默认端口：
      - TCP: `9527`
      - HTTP: `9528`
      - GreenTea 更新：`9529`
      - UDP 基础端口：`9530`

### 直接运行（调试 / 快速验证）
在目标主机上进入 `dist` 目录并直接运行二进制（示例把日志重定向到 `logs/`）：

```bash
cd /opt/teafamily
mkdir -p logs
# 在服务器上运行 LemonTea（HTTP 服务）
./bin/LemonTea > logs/lemontea.log 2>&1 &
# 可选：运行 GreenTea 守护进程（监控/更新）
./bin/GreenTea > logs/greentea.log 2>&1 &
# 在每台客户端（例如 Raspberry Pi）上运行 HoneyTea
./bin/HoneyTea > logs/honeytea.log 2>&1 &

# 查看日志
tail -f logs/lemontea.log
```

### 配置文件位置与运行（重要）

构建脚本会将组件配置文件复制到 `dist/config/` 下，文件名为组件目录名，例如：

- `dist/config/LemonTea.json`
- `dist/config/HoneyTea.json`
- `dist/config/GreenTea.json`

默认情况下，各组件二进制会尝试打开进程启动时工作目录下的 `config.json`（即默认路径为 `config.json`）。因此运行时请显式指定配置文件路径或使用相对路径指向 `dist/config` 中的文件：

推荐在 `dist` 根目录运行并传入 `--config` 参数：

```bash
cd /opt/teafamily
./bin/LemonTea --config config/LemonTea.json > logs/lemontea.log 2>&1 &
./bin/GreenTea --config config/GreenTea.json > logs/greentea.log 2>&1 &
./bin/HoneyTea --config config/HoneyTea.json > logs/honeytea.log 2>&1 &
```

如果你必须从 `dist/bin` 目录直接运行，可使用相对路径：

```bash
cd /opt/teafamily/bin
./LemonTea --config ../config/LemonTea.json > ../logs/lemontea.log 2>&1 &
```

也可以使用符号链接或复制单个 `config.json` 到当前工作目录，但要注意不要覆盖不同组件的配置文件：

```bash
ln -s /opt/teafamily/config/LemonTea.json /opt/teafamily/config.json
# 或者（谨慎）
cp /opt/teafamily/config/LemonTea.json /opt/teafamily/config.json
```

当出现 "Cannot open config file: config.json" 或类似错误时，通常意味着当前工作目录下没有 `config.json`，请用上述任一方法确保二进制能找到正确的配置文件。

### 使用 systemd（Linux）
创建服务单元（示例 `/etc/systemd/system/lemontea.service`）：

```ini
[Unit]
Description=LemonTea Service
After=network.target

[Service]
Type=simple
WorkingDirectory=/opt/teafamily
ExecStart=/bin/sh -c '/opt/teafamily/bin/LemonTea >> /var/log/teafamily/lemontea.log 2>&1'
Restart=on-failure
User=teafamily

[Install]
WantedBy=multi-user.target
```

启用并查看日志：

```bash
sudo mkdir -p /var/log/teafamily
sudo systemctl daemon-reload
sudo systemctl enable --now lemontea.service
sudo journalctl -u lemontea.service -f
```

### macOS launchd 示例
在 macOS 上可以使用 `launchd`，示例 `~/Library/LaunchAgents/com.teafamily.lemontea.plist`：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
      <dict>
            <key>Label</key><string>com.teafamily.lemontea</string>
            <key>ProgramArguments</key>
            <array>
                  <string>/opt/teafamily/bin/LemonTea</string>
            </array>
            <key>WorkingDirectory</key><string>/opt/teafamily</string>
            <key>RunAtLoad</key><true/>
            <key>StandardOutPath</key><string>/var/log/teafamily/lemontea.log</string>
            <key>StandardErrorPath</key><string>/var/log/teafamily/lemontea.err</string>
      </dict>
</plist>
```

### OrangeTea 前端部署（静态文件）
`dist/frontend` 包含生产构建的静态文件，可使用任意静态文件服务器或 Nginx 提供服务。示例使用 `serve`：

```bash
npm install -g serve
serve -s /opt/teafamily/frontend -l 8080
# 访问: http://<your-host>:8080
```

### 常见问题（FAQ）

- 只启动 GreenTea 会自动拉起 LemonTea 和 HoneyTea 吗？
      - 会，但前提是 `GreenTea` 配置中的 `processes[].auto_start=true`，并且 `binary` 与 `args` 路径正确。
      - 当前构建脚本会自动改写 `dist/config/GreenTea.json`，默认指向 `dist/bin/LemonTea`、`dist/bin/HoneyTea` 和对应 `dist/config/*.json`，因此在 `dist` 目录下通常可直接由 GreenTea 托管拉起。

- 树莓派部署（Raspberry Pi）
      - 可以把 `HoneyTea` 部署在树莓派上，但需要为目标架构（arm/arm64/armv7）编译相应的二进制。推荐直接在树莓派上运行构建脚本，或使用交叉编译并将对应平台的二进制复制到树莓派。
      - 插件系统仍然有效：只要插件可执行文件是为目标机器编译并部署到该机器，LemonTea/HoneyTea 会按原样管理它们。

- 子进程 / 插件可以使用其它语言吗？
      - 可以。`HoneyTea`/`LemonTea` 管理的是任意可执行文件，插件或子进程可以用 Python、Go、Rust、Shell 等语言实现。
      - 如果插件与父进程通过 stdio 使用 JSON-line 协议通信，请确保插件实现了相应的 JSON-line 输入/输出（例如 Python 使用 `sys.stdin.readline()` 与 `print(json.dumps(...), flush=True)`）。
      - 对于脚本语言，请确保目标机器安装相应运行时（如 `python3`），并且脚本具有可执行权限和正确的 shebang（如 `#!/usr/bin/env python3`）。
