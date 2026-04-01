# 文件管理插件使用文档

## 插件类型
- `distributed`（双端插件）
- 统一包会自动分发到 LemonTea / HoneyTea，并自动安装 OrangeTea 前端页面

## 导出（macOS）
```bash
./scripts/macOS_export_file_manager_plugin.sh
```

默认输出：
- `dist/plugin-exports/file-manager/file-manager-unified-macos.tar.gz`

## 安装步骤
1. 在 OrangeTea 的“统一插件管理台”上传 `file-manager-unified-macos.tar.gz`。
2. 选择 HoneyTea 分发范围（全部在线 / 指定节点 / 不分发）。
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
