# 文件管理插件使用文档

## 插件类型
- `distributed`（双端插件）
- 统一包会自动分发到 LemonTea / HoneyTea（在线节点），并安装 OrangeTea 前端页面

## 导出（macOS）
```bash
./scripts/macOS_export_file_manager_plugin.sh
```

默认输出：
- `dist/plugin-exports/file-manager/file-manager-unified-macos.tar.gz`

## 导出（Linux x64 LemonTea + Raspberry Pi 5 HoneyTea）
```bash
./scripts/export_file_manager_plugin_linux_x64_lemon_rpi5_honey.sh
```

默认输出：
- `dist/plugin-exports/file-manager/file-manager-unified-lemon-linux-x64-honey-rpi5.tar.gz`

可选平台导出：
```bash
./scripts/export_file_manager_plugin_linux_x64_lemon_rpi5_honey.sh \
  --lemon-platform linux-x64 \
  --honey-platform rpi5

./scripts/export_file_manager_plugin_linux_x64_lemon_rpi5_honey.sh \
  --lemon-platform macos \
  --honey-platform macos
```

包名会随平台变化，例如：
- `file-manager-unified-lemon-macos-honey-macos.tar.gz`

## 安装步骤
1. 在 OrangeTea 的“统一插件管理台”上传 `file-manager-unified-macos.tar.gz`。
2. 预览信息确认后执行安装，系统会按 `distributed` 自动分发到 LemonTea 与在线 HoneyTea 节点。
3. 安装完成后，在同一页统一启动或停止服务端与远端插件进程。

## 运行与控制 API
- LemonTea 侧：
  - `POST /api/plugins/file-manager/start`
  - `POST /api/plugins/file-manager/stop`
- HoneyTea 侧：
  - `POST /api/clients/{nodeId}/plugins/file-manager/start`
  - `POST /api/clients/{nodeId}/plugins/file-manager/stop`

## 安全建议
- 客户端文件操作必须限制根目录（白名单），避免目录穿越。
- 下载与删除建议附带审计日志（操作人、路径、时间）。
