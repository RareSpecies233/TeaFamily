# macOS 构建脚本说明（scripts/macOS_build_release.sh）

## 概述

`scripts/macOS_build_release.sh` 是一个用于 macOS 的打包/发布辅助脚本，自动完成 CMake 构建、收集主程序与配置，以及构建并拷贝 OrangeTea 前端静态文件到 `dist` 目录，最终生成可部署的 `dist` 包。

此文档说明脚本的行为、产物位置、常见修改点与故障排查提示。

## 脚本目的

- 在 `build-release` 中运行 CMake/编译（Release 模式）。
- 将主可执行文件、配置与 OrangeTea 前端构建结果收集到 `dist`（默认 `PROJECT_ROOT/dist`）。
- 便于一次性把所有运行时产物打包到一个目录，便于复制到服务器/客户端部署。
- 每次打包会清空并重建 `dist/frontend_plugins` 与 `dist/plugins`，保证默认无任何预装插件。

## 使用方法

在项目根目录运行：

```bash
./scripts/macOS_build_release.sh        # 正常构建并自动收集产物到 dist
./scripts/macOS_build_release.sh clean  # 清理再构建（删除 build-release 与 dist）
sh ./scripts/macOS_build_release.sh clean  # 也支持用 sh 调用（脚本会自动切到 bash）
```

脚本会自动检测 `npm`，若存在则进入 `OrangeTea` 并执行 `npm install`（若缺少 node_modules）和 `npm run build`，并把前端构建输出复制到 `dist/frontend`。

> 新增：脚本不会再执行 `cmake --install`，避免第三方库把 `.h`、`.cmake` 等开发文件写入 `dist`。`dist` 只保留运行时相关内容（可执行文件、插件、配置、前端静态资源）。

## 关键步骤（逐步说明）

1. 变量与路径计算
   - `SCRIPT_DIR`：脚本所在目录
   - `PROJECT_ROOT`：脚本的上级目录（项目根）
   - `BUILD_DIR`：默认 `PROJECT_ROOT/build-release`
   - `INSTALL_DIR`：默认 `PROJECT_ROOT/dist`

2. 可选清理
   - 传入 `clean` 参数会删除 `build-release` 与 `dist` 目录。

3. CMake 配置
   - 使用 `-DCMAKE_BUILD_TYPE=Release`
   - 在 macOS 上会设置 `-DCMAKE_OSX_ARCHITECTURES=$(uname -m)`，以便为当前架构生成二进制。

4. 编译
   - 使用本机逻辑 CPU 数（`sysctl -n hw.logicalcpu`）作为并行编译线程数，并执行 `cmake --build`。

5. 收集产物到 `dist`
   - 创建 `dist/bin`、`dist/plugins`、`dist/config`。
   - 复制主程序：`GreenTea`、`HoneyTea`、`LemonTea` 的可执行文件到 `dist/bin`。
  - **不复制任何插件运行时、清单与插件前端文件**，`dist/plugins` 保持空目录。
   - 复制各组件的 `config.json` 到 `dist/config/`（文件名按组件命名）。
  - 自动改写 `dist/config/GreenTea.json` 中被监控进程路径，确保默认指向 `dist/bin/LemonTea`、`dist/bin/HoneyTea` 和 `dist/config/*.json`，避免发布目录下路径不匹配。

  如果常规路径无法找到产物，脚本会执行回退查找（使用 `find`），在 `build-release` 下查找匹配的可执行文件并复制到相应位置。这样可覆盖不同 CMake 输出布局导致的路径差异。

6. 构建前端
   - 如果可用 `npm`，会进入 `OrangeTea`：若 `node_modules` 缺失则先 `npm install`，再 `npm run build`，并把 `OrangeTea/dist/*` 拷贝到 `dist/frontend/`。

## 产物结构（例）

```
dist/
  bin/
    LemonTea
    HoneyTea
    GreenTea
  frontend_plugins/
    # 默认为空（统一插件安装时写入）
  plugins/
    # 默认为空（插件需通过 OrangeTea 上传安装）
  config/
    GreenTea.json
    HoneyTea.json
    LemonTea.json
  frontend/   # OrangeTea 的生产构建文件
```

## 部署与运行示例

将 `dist` 复制到目标主机，例如 `/opt/teafamily`，然后在目标主机上运行：

```bash
cd /opt/teafamily
mkdir -p logs
./bin/LemonTea > logs/lemontea.log 2>&1 &
./bin/GreenTea > logs/greentea.log 2>&1 &
./bin/HoneyTea > logs/honeytea.log 2>&1 &
```

建议在生产环境通过 `systemd`（Linux）或 `launchd`（macOS）管理进程，便于开机自启与日志集中管理。

## 在 Raspberry Pi / ARM 平台上的注意事项

- `scripts/macOS_build_release.sh` 默认为 macOS 环境设计（使用 `sysctl`、`CMAKE_OSX_ARCHITECTURES` 等）。要在树莓派（ARM）上运行：
  - 建议在目标设备（树莓派）上直接执行 CMake 构建；或使用交叉编译生成 ARM 二进制并将其放入 `dist`。
  - 确保 HoneyTea 与插件运行时都为相同架构编译。

## 插件导出脚本（macOS）

为了独立分发内置插件，项目提供以下导出脚本：

```bash
./scripts/macOS_export_ssh_plugin.sh
./scripts/macOS_export_file_manager_plugin.sh
./scripts/macOS_export_monitor_plugin.sh
./scripts/macOS_export_cam_lan_stream_plugin.sh
```

每个脚本会生成一个统一包：
- 统一插件包：`<plugin>-unified-macos.tar.gz`（包含运行时与前端，供 OrangeTea 统一安装入口使用）

说明：`cam-lan-stream` 为 `lemon-only` 插件，统一安装时只会安装到 LemonTea（以及前端页面），不会要求 HoneyTea 运行时。

## 常见问题与排查

- 构建失败：首先检查 `cmake`、编译器（Xcode/clang）和依赖（nlohmann/json、spdlog、cpp-httplib）是否可用。可在 `build-release` 目录手动运行 `cmake`/`make` 查看更详细错误。
- 前端构建失败：进入 `OrangeTea` 目录单独运行 `npm install` / `npm run build`，修复依赖或类型错误（TypeScript）后重试脚本。
- 默认没有任何插件：这是预期行为。请先执行导出脚本生成 unified 包，再通过 OrangeTea「统一插件管理台」上传安装。

## 可定制点

- 将 `INSTALL_DIR` 改为其他目录以符合你的部署结构。
- 若希望在 Linux 上使用，移除或修改 macOS 专属的 `CMAKE_OSX_ARCHITECTURES` 与 `sysctl` 调用（可改用 `nproc`）。
