# 局域网摄像头串流插件使用文档

## 插件类型
- `lemon-only`（仅 LemonTea 运行时）
- 统一包会自动安装到 LemonTea，并安装 OrangeTea 前端页面
- 不会向 HoneyTea 分发运行时

## 插件能力概述
- 在插件内置 HTTP 服务中提供移动端采集页
- 移动端选择摄像头后持续上报 JPEG 帧
- OrangeTea 动态页面展示：
  - 在线设备列表
  - 设备摄像头列表
  - 实时预览画面

## 导出（macOS）
```bash
./scripts/macOS_export_cam_lan_stream_plugin.sh
```

默认输出：
- `dist/plugin-exports/cam-lan-stream/cam-lan-stream-unified-macos.tar.gz`

## 导出（Linux x64 LemonTea）
```bash
./scripts/export_cam_lan_stream_plugin_linux_x64.sh
```

默认输出：
- `dist/plugin-exports/cam-lan-stream/cam-lan-stream-unified-lemon-linux-x64.tar.gz`

## 导出（可选目标平台）
可通过参数选择 LemonTea 目标平台：`linux-x64` / `rpi5` / `macos`

```bash
# LemonTea 运行在树莓派5
./scripts/export_cam_lan_stream_plugin_linux_x64.sh --lemon-platform rpi5

# LemonTea 运行在 macOS
./scripts/export_cam_lan_stream_plugin_linux_x64.sh --lemon-platform macos
```

## 安装步骤
1. 在 OrangeTea 的“统一插件管理台”上传统一包。
2. 预览信息确认后执行安装。
3. 系统会自动安装 LemonTea 运行时与 OrangeTea 前端页面。
4. 启动插件后，在插件页查看“移动端采集页”地址并用手机访问。

## 使用流程
1. 手机与 LemonTea 服务器处于同一局域网（或可直连）。
2. 手机打开插件提供的采集页地址（默认端口 `19731`）。
3. 允许摄像头权限，选择摄像头并点击“开始串流”。
4. 在 OrangeTea 插件页选择设备和摄像头，即可看到实时画面。

## 可配置环境变量
- `TEA_CAM_STREAM_PORT`：插件 HTTP 监听端口（默认 `19731`）
- `TEA_CAM_STREAM_BIND_HOST`：监听地址（默认 `0.0.0.0`）
- `TEA_CAM_STREAM_PUBLIC_HOST`：对外展示地址（默认 `127.0.0.1`）
- `TEA_CAM_STREAM_PUBLIC_ORIGIN`：完整对外地址，例如 `https://tea-lan.example.com:19731`，优先级最高
- `TEA_CAM_STREAM_SSL_CERT`：启用 HTTPS 时使用的证书文件路径
- `TEA_CAM_STREAM_SSL_KEY`：启用 HTTPS 时使用的私钥文件路径

## 移动端实时串流
移动浏览器调用摄像头时，必须处于安全上下文。推荐两种方式：
- 使用 HTTPS 直接访问插件页面，并提供受信任的证书文件。
- 通过本机/局域网反向代理提供 HTTPS，再把 `TEA_CAM_STREAM_PUBLIC_ORIGIN` 指向外部 HTTPS 地址。

如果暂时没有可用证书，插件会自动退回兼容模式，允许用户通过“拍照并上传”方式更新画面，但这不是实时串流。

### 一个可行的本地方案
1. 使用 `mkcert` 或企业证书为局域网地址生成受信任证书。
2. 将证书路径传给 `TEA_CAM_STREAM_SSL_CERT` 和 `TEA_CAM_STREAM_SSL_KEY`。
3. 用手机打开 `https://.../` 的移动端页面，允许摄像头权限后即可实时串流。

## 插件 HTTP 接口（插件内置服务）
- `GET /`：移动端采集页（HTTP 或 HTTPS，取决于是否配置证书）
- `GET /api/devices`：设备与摄像头快照
- `POST /api/register`：设备注册/更新
- `POST /api/publish-frame`：上传 JPEG 帧
- `GET /api/frame/{deviceId}/{cameraId}`：读取实时帧
- `GET /healthz`：健康检查

## 外置 API（推荐给第三方应用接入）
为避免耦合 OrangeTea 页面逻辑，插件提供了稳定的外置 API：

- `GET /api/public/status`：获取服务状态和设备快照
- `GET /api/public/devices`：获取设备列表和摄像头元数据
- `GET /api/public/devices/{deviceId}/cameras`：获取指定设备摄像头列表
- `GET /api/public/frame/{deviceId}/{cameraId}`：获取 JPEG 实时帧
- `GET /api/public/frame/{deviceId}/{cameraId}/base64`：获取 Base64 帧
- `GET /api/public/docs`：机器可读 API 文档

### JavaScript 示例
```javascript
const apiBase = 'http://127.0.0.1:19731/api/public'

const devicesResp = await fetch(apiBase + '/devices')
const devices = (await devicesResp.json()).devices || []

const deviceId = devices[0]?.device_id
const cameraId = devices[0]?.cameras?.[0]?.camera_id

const frameUrl = apiBase + '/frame/' + encodeURIComponent(deviceId) + '/' + encodeURIComponent(cameraId)
document.querySelector('#preview').src = frameUrl
```

### Python 示例
```python
import requests

api_base = 'http://127.0.0.1:19731/api/public'
devices = requests.get(api_base + '/devices', timeout=5).json().get('devices', [])

device_id = devices[0]['device_id']
camera_id = devices[0]['cameras'][0]['camera_id']

resp = requests.get(api_base + f'/frame/{device_id}/{camera_id}/base64', timeout=5)
payload = resp.json()
print('mime:', payload.get('mime'))
print('base64 length:', len(payload.get('frame_base64', '')))
```

### curl 示例
```bash
curl -s http://127.0.0.1:19731/api/public/status
curl -s http://127.0.0.1:19731/api/public/devices
curl -s http://127.0.0.1:19731/api/public/docs
```

说明：OrangeTea 的摄像头插件页面已经内置上述 API 接入说明和示例代码，便于你直接复制使用。

## 注意事项
- 移动浏览器调用摄像头通常要求 HTTPS 或可信局域网环境。
- 默认使用 Base64 JPEG 上报，适合局域网低延迟预览，不建议直接跨公网裸露服务。
