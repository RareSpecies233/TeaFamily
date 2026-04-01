# 文件管理插件使用文档

## 插件类型
- `distributed`（双端插件）
- 需要 LemonTea 与 HoneyTea 都安装运行时插件包
- 可选安装 OrangeTea 前端插件包用于动态页面

## 导出（macOS）
```bash
./scripts/macOS_export_file_manager_plugin.sh
```

默认输出：
- `dist/plugin-exports/file-manager/file-manager-runtime-macos.tar.gz`
- `dist/plugin-exports/file-manager/file-manager-frontend-macos.tar.gz`

## 安装步骤
1. 在 OrangeTea 的“LemonTea 插件”上传 `file-manager-runtime-macos.tar.gz`。
2. 在 OrangeTea 的“HoneyTea 插件”给目标客户端上传同一个运行时包。
3. 在 OrangeTea 的“OrangeTea 前端插件”上传 `file-manager-frontend-macos.tar.gz`。

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
