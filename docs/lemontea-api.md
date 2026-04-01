# LemonTea HTTP API 文档

Base URL 示例：`http://127.0.0.1:9528`

## 状态
### GET /api/status
返回 LemonTea 运行状态。

## 客户端（HoneyTea）
### GET /api/clients
获取已连接 HoneyTea 列表与缓存插件信息。

### GET /api/clients/{nodeId}/plugins
刷新并返回指定 HoneyTea 的插件列表。

### POST /api/clients/{nodeId}/plugins/{name}/start
启动指定 HoneyTea 插件。

### POST /api/clients/{nodeId}/plugins/{name}/stop
停止指定 HoneyTea 插件。

### DELETE /api/clients/{nodeId}/plugins/{name}
卸载指定 HoneyTea 插件。

### POST /api/clients/{nodeId}/plugins/install
上传并安装插件到指定 HoneyTea。
- `multipart/form-data`
- 字段：`plugin`（文件），`name`（可选）

## LemonTea 本地插件
### GET /api/plugins
获取 LemonTea 本地插件列表。

### POST /api/plugins/{name}/start
启动本地插件。

### POST /api/plugins/{name}/stop
停止本地插件。

### DELETE /api/plugins/{name}
卸载本地插件。

### POST /api/plugins/install
安装本地插件（tar.gz）。
- `multipart/form-data`
- 字段：`plugin`（文件），`name`（可选）

## 插件消息转发
### POST /api/plugin-message
向某个节点的某个插件发送消息。

请求体示例：
```json
{
  "node_id": "honey-001",
  "plugin": "monitor",
  "data": {"action": "ping"}
}
```

## 程序更新
### POST /api/update/lemon-tea
上传 LemonTea 新二进制，触发 GreenTea 自更新流程。
- `multipart/form-data`
- 字段：`binary`

### POST /api/update/honey-tea/{nodeId}
上传 HoneyTea 新二进制（由 LemonTea 转发到目标节点）。
- `multipart/form-data`
- 字段：`binary`

## OrangeTea 前端插件
### GET /api/frontend-plugins
列出已安装前端插件。

### GET /api/frontend-plugins/{name}/{file}
读取前端插件静态文件。

### POST /api/frontend-plugins/install
安装前端插件包。
- `multipart/form-data`
- 字段：`plugin`（文件），`name`（可选）

### DELETE /api/frontend-plugins/{name}
删除前端插件。

## 错误约定
- HTTP `200`：请求被受理，结果在 `{"success":true/false}`。
- HTTP `4xx/5xx`：参数错误或服务端异常，响应包含 `error` 字段。
