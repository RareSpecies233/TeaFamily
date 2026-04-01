# SSH 插件使用文档

## 插件类型
- `distributed`（双端插件）
- 统一包会自动分发到 LemonTea / HoneyTea，并自动安装 OrangeTea 前端页面

## 导出（macOS）
```bash
./scripts/macOS_export_ssh_plugin.sh
```

默认输出：
- `dist/plugin-exports/ssh/ssh-unified-macos.tar.gz`

## 安装步骤
1. 在 OrangeTea 的“统一插件管理台”上传 `ssh-unified-macos.tar.gz`。
2. 选择 HoneyTea 分发范围（全部在线 / 指定节点 / 不分发）。
3. 安装完成后，可在同一页统一启动 LemonTea 与 HoneyTea 侧 `ssh` 插件。

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
