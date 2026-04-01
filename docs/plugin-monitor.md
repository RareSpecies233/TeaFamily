# 监控插件使用文档

## 插件类型
- `distributed`（双端插件）
- 需要 LemonTea 与 HoneyTea 安装运行时插件包
- 可选安装 OrangeTea 前端插件包

## 导出（macOS）
```bash
./scripts/macOS_export_monitor_plugin.sh
```

默认输出：
- `dist/plugin-exports/monitor/monitor-runtime-macos.tar.gz`
- `dist/plugin-exports/monitor/monitor-frontend-macos.tar.gz`

## 安装步骤
1. 在 OrangeTea 的“LemonTea 插件”上传 `monitor-runtime-macos.tar.gz`。
2. 在 OrangeTea 的“HoneyTea 插件”给目标客户端上传同一个运行时包。
3. 在 OrangeTea 的“OrangeTea 前端插件”上传 `monitor-frontend-macos.tar.gz`。

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
