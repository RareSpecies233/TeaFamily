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

### POST /api/plugins/install-unified
统一安装插件（推荐）：按 `plugin_type` 自动分发到 LemonTea / HoneyTea / OrangeTea。
- `multipart/form-data`
- 字段：`plugin`（统一插件包，必填）
- 分发规则：
  - `distributed` -> LemonTea + HoneyTea（所有在线节点）+ OrangeTea（若包含前端）
  - `lemon-only` -> LemonTea + OrangeTea（若包含前端）
  - `honey-only` -> HoneyTea（所有在线节点）+ OrangeTea（若包含前端）
- 返回关键字段：
  - `distribution_targets`：本次目标列表
  - `local_required` / `remote_required` / `frontend_required`
  - `local_installed` / `frontend_installed` / `remote_results`
  - `warnings`（例如无在线 HoneyTea 时的提示）

### POST /api/plugins/inspect
统一安装前预览插件信息（用于前端确认弹窗）。
- `multipart/form-data`
- 字段：`plugin`（统一插件包，必填）
- 返回：插件名称、版本、描述、能力、依赖、在线 HoneyTea 数量等。
- 关键字段：
  - `plugin.distribution_targets`：基于 `plugin_type` 推导出的安装目标
  - `plugin.connected_honey_nodes` / `plugin.connected_honey_count`

## 插件消息转发
### POST /api/plugin-message
向某个节点的某个插件发送消息。

说明：`node_id` 可为空或等于 LemonTea 自身节点，此时消息会投递到 LemonTea 本地插件。

请求体示例：
```json
{
  "node_id": "honey-001",
  "plugin": "monitor",
  "data": {"action": "ping"}
}
```

### GET /api/plugin-events
查询插件事件流（用于 Web SSH 输出流、监控实时数据、文件操作回执）。

查询参数：
- `plugin`：插件名（可选）
- `after`：仅返回序号大于该值的事件（可选，默认 0）
- `limit`：本次最多返回数量（可选，默认 200）

返回示例：
```json
{
  "events": [
    {
      "seq": 1024,
      "timestamp": 1775000000000,
      "plugin": "ssh",
      "data": {
        "action": "output",
        "session_id": "ssh-1775...",
        "data": "..."
      }
    }
  ],
  "next_seq": 1030
}
```

### POST /api/plugin-rpc
向插件发送消息并等待匹配响应（按 `request_id` + `expected_actions` 匹配）。

说明：`node_id` 可选；为空时默认请求 LemonTea 本地插件。

请求示例：
```json
{
  "node_id": "honey-001",
  "plugin": "file-manager",
  "data": {
    "action": "list",
    "path": "/tmp"
  },
  "timeout_ms": 4000,
  "expected_actions": ["list_result", "error"]
}
```

响应示例：
```json
{
  "success": true,
  "matched": true,
  "request_id": "file-manager-...",
  "response": {
    "action": "list_result",
    "entries": []
  },
  "response_seq": 2051
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
- 说明：若包内 `plugin.json` 已声明 `name`，服务端会优先使用 manifest 名称作为安装目录。

### DELETE /api/frontend-plugins/{name}
删除前端插件。

## 错误约定
- HTTP `200`：请求被受理，结果在 `{"success":true/false}`。
- HTTP `4xx/5xx`：参数错误或服务端异常，响应包含 `error` 字段。
