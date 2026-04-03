# TeaFamily 端口需求汇总

## 1. LemonTea（核心服务）

默认配置文件：`LemonTea/config.json`

- `tcp_port`: `9527`
  - LemonTea TCP 服务器（接受 HoneyTea 连接、插件 RPC）
- `http_port`: `9528`
  - LemonTea HTTP API（OrangeTea 前端 / CLI / 插件管理接口）
- `udp_port`: `9531`
  - LemonTea 本地 UDP 接收（默认 `DEFAULT_UDP_BASE_PORT + 1`）
- `greentea_port`: `9529`
  - GreenTea 更新端口（LemonTea self-update 向 GreenTea 请求）  
  无需开放，仅本地回环通信使用

## 2. HoneyTea（边缘节点）

默认配置文件：`HoneyTea/config.json`

- `server_port`: `9527`
  - 连接到 LemonTea 的 TCP 目标端口（出站）
- `udp_port`: `9530`
  - HoneyTea 本地 UDP 服务（`DEFAULT_UDP_BASE_PORT`）
- `greentea_port`: `9529`
  - 同 GreenTea 端口用于更新/监控（本机通信）  
  无需开放，仅本地回环通信使用

## 3. OrangeTea（前端）

OrangeTea 本身不直接绑定后端 TCP/UDP 端口：

- 运行在浏览器内，访问 LemonTea 的 HTTP API（默认 `http://127.0.0.1:9528`）
- 开发模式 `npm run dev` 的 Vite 默认端口 `5173`（仅开发）

## 4. GreenTea（守护 + 更新）

配置文件：`GreenTea/config.json`

- 监听 `listen_port`：默认 `9529`（`DEFAULT_GREENTEA_PORT`）
- 仅用于本机/内网的更新推送，非必须对外暴露

## 5. 插件端口补充
### cam-lan-stream
- 默认 `19731`，可以由插件环境变量调整（`TEA_CAM_STREAM_PORT`）

## 6. 防火墙与最小开放策略

- 外部访问:
  - `9528`（LemonTea HTTP API）
  - `9527`（LemonTea TCP, 供 HoneyTea 连接）
- 内部节点:
  - `9530`/`9531`（HoneyTea/LemonTea UDP，视需要）
  - `9529`（GreenTea，本地更新）
- OrangeTea 生产端口视静态托管和 Nginx 代理而定

## 7. 注意点

- 端口可在对应 `config.json` 里调整
- LemonTea/HoneyTea/GreenTea 推荐部署为 `--config config/*.json`
- 生产建议仅内网可达，避免公网未授权访问
