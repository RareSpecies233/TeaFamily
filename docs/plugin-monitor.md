# 监控插件使用文档

## 插件类型
- `distributed`（双端插件）
- 统一包会自动分发到 LemonTea / HoneyTea（在线节点），并安装 OrangeTea 前端页面

## 导出（macOS）
```bash
./scripts/macOS_export_monitor_plugin.sh
```

默认输出：
- `dist/plugin-exports/monitor/monitor-unified-macos.tar.gz`

## 导出（Linux x64 LemonTea + Raspberry Pi 5 HoneyTea）
```bash
./scripts/export_monitor_plugin_linux_x64_lemon_rpi5_honey.sh
```

默认输出：
- `dist/plugin-exports/monitor/monitor-unified-lemon-linux-x64-honey-rpi5.tar.gz`

可选平台导出：
```bash
./scripts/export_monitor_plugin_linux_x64_lemon_rpi5_honey.sh \
  --lemon-platform linux-x64 \
  --honey-platform rpi5

./scripts/export_monitor_plugin_linux_x64_lemon_rpi5_honey.sh \
  --lemon-platform macos \
  --honey-platform macos
```

包名会随平台变化，例如：
- `monitor-unified-lemon-macos-honey-macos.tar.gz`

## 安装步骤
1. 在 OrangeTea 的“统一插件管理台”上传 `monitor-unified-macos.tar.gz`。
2. 预览信息确认后执行安装，系统会按 `distributed` 自动分发到 LemonTea 与在线 HoneyTea 节点。
3. 安装完成后，可在同一页统一启停 LemonTea/HoneyTea 插件进程。

## 运行与控制 API
- LemonTea 侧：
  - `POST /api/plugins/monitor/start`
  - `POST /api/plugins/monitor/stop`
- HoneyTea 侧：
  - `POST /api/clients/{nodeId}/plugins/monitor/start`
  - `POST /api/clients/{nodeId}/plugins/monitor/stop`

## 插件间通信
- 可通过标准消息格式把监控事件投递给其他 LemonTea 插件：
```json
{"action":"plugin_message","target_plugin":"another-plugin","data":{"kind":"metrics","value":42}}
```
- LemonTea 已支持同机插件 A -> B 的消息转发。
