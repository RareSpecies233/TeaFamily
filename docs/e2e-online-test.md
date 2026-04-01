# TeaFamily 端到端在线测试记录

测试日期：2026-04-01

## 测试目标

1. 验证默认构建产物无预装插件。
2. 验证统一插件包的“预览 -> 确认安装 -> 三端分发”流程。
3. 验证 HoneyTea 插件状态可在 API 中正确反映。
4. 验证前端插件入口文件可正常访问（用于 OrangeTea 动态加载）。

## 测试环境

- 构建命令：`./scripts/macOS_build_release.sh clean`
- 插件导出命令：
  - `./scripts/macOS_export_ssh_plugin.sh`
  - `./scripts/macOS_export_file_manager_plugin.sh`
  - `./scripts/macOS_export_monitor_plugin.sh`
- 隔离端口（避免本机已有实例冲突）：
  - LemonTea TCP/HTTP/UDP：`19627/19628/19631`
  - HoneyTea UDP：`19630`

## 在线测试步骤与结果

### 1) 基线检查（默认无插件）

请求：
- `GET /api/status`
- `GET /api/plugins`
- `GET /api/frontend-plugins`
- `GET /api/clients`

结果：
- `plugins_count = 0`
- 本地插件列表 `[]`
- 前端插件列表 `[]`
- HoneyTea 客户端在线且插件列表为空

### 2) 三个插件统一包预览与安装

插件包：
- `dist/plugin-exports/ssh/ssh-unified-macos.tar.gz`
- `dist/plugin-exports/file-manager/file-manager-unified-macos.tar.gz`
- `dist/plugin-exports/monitor/monitor-unified-macos.tar.gz`

每个插件执行：
- `POST /api/plugins/inspect`
- `POST /api/plugins/install-unified`

结果：
- 三个插件 `inspect` 均返回 `success=true`，包含名称、版本、依赖、能力、在线 HoneyTea 数量等信息。
- 三个插件 `install-unified` 均返回 `success=true`。
- `remote_results` 均包含 `honey-001` 且 `success=true`。
- `frontend_installed=true`。

### 3) 安装后资产检查

请求：
- `GET /api/plugins`
- `GET /api/clients/honey-001/plugins`
- `GET /api/frontend-plugins`

结果：
- LemonTea 本地插件：`ssh`、`file-manager`、`monitor`（状态 `stopped`）。
- HoneyTea 远端插件：`ssh`、`file-manager`、`monitor`（状态 `stopped`）。
- 前端插件列表包含 3 个插件。

### 4) 前端插件入口文件访问检查

请求：
- `GET /api/frontend-plugins/ssh/index.js`
- `GET /api/frontend-plugins/file-manager/index.js`
- `GET /api/frontend-plugins/monitor/index.js`

结果：
- 三个入口文件 HTTP 状态均为 `200`，且返回字节数大于 0。

### 5) HoneyTea 状态同步检查（运行/停止）

操作：
- 启动：
  - `POST /api/plugins/monitor/start`
  - `POST /api/clients/honey-001/plugins/monitor/start`
- 查询：`GET /api/clients/honey-001/plugins`
- 停止：
  - `POST /api/plugins/monitor/stop`
  - `POST /api/clients/honey-001/plugins/monitor/stop`
- 再查询：`GET /api/clients/honey-001/plugins`

结果：
- 启动后 HoneyTea 中 `monitor` 状态为 `running`。
- 停止后 HoneyTea 中 `monitor` 状态恢复为 `stopped`。

## 结论

- 默认构建已满足“无预装插件”。
- 统一插件安装链路（预览 + 三端分发）可用。
- HoneyTea 插件状态显示链路可用。
- 三个现有插件的统一包安装与前端入口加载可用。
