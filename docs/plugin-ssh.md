# SSH 插件使用文档

## 插件类型
- `distributed`（双端插件）
- 需要在 LemonTea 与 HoneyTea 同时安装运行时插件包
- 可选安装 OrangeTea 前端插件包用于动态页面

## 导出（macOS）
```bash
./scripts/macOS_export_ssh_plugin.sh
```

默认输出：
- `dist/plugin-exports/ssh/ssh-runtime-macos.tar.gz`
- `dist/plugin-exports/ssh/ssh-frontend-macos.tar.gz`

## 安装步骤
1. 在 OrangeTea 的“插件管理 -> LemonTea 插件”上传 `ssh-runtime-macos.tar.gz`。
2. 在 OrangeTea 的“插件管理 -> HoneyTea 插件”中，给目标客户端上传同一个 `ssh-runtime-macos.tar.gz`。
3. 在 OrangeTea 的“插件管理 -> OrangeTea 前端插件”上传 `ssh-frontend-macos.tar.gz`。
4. 启动 LemonTea 与 HoneyTea 侧的 `ssh` 插件。

## 运行与控制
- LemonTea 侧：
  - `POST /api/plugins/ssh/start`
  - `POST /api/plugins/ssh/stop`
- HoneyTea 侧：
  - `POST /api/clients/{nodeId}/plugins/ssh/start`
  - `POST /api/clients/{nodeId}/plugins/ssh/stop`

## 备注
- 当前版本为进程级控制与消息转发能力，适合作为 SSH 会话控制链路的基础。
- 若要扩展成完整终端交互，建议在 LemonTea 侧为 SSH 增加专用会话 API（创建会话、输入输出流、关闭会话）。
