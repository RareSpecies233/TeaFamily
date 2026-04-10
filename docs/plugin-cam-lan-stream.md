# 树莓派摄像头串流插件使用文档

## 插件定位
- 插件名：`cam-lan-stream`
- 插件类型：`distributed`
- LemonTea：作为控制桥接与 API 入口
- HoneyTea：作为树莓派摄像头实际运行端
- OrangeTea：提供参数调节、WebRTC 预览、截图、录制和接入示例

这个插件面向如下部署方式：
- LemonTea / OrangeTea 运行在电脑上
- HoneyTea 运行在树莓派 5 上
- HoneyTea 负责直接访问树莓派摄像头，并通过 MediaMTX 输出：
  - 一路 WebRTC / WHEP，给 OrangeTea 前端低延迟预览
  - 一路裸 H.264 over TCP，给后续 YOLO 插件或其他程序直接消费

## 运行时依赖
HoneyTea 所在设备需要满足：
- 安装 `mediamtx`
- 安装 `ffmpeg`
- 系统具备树莓派相机栈（如 `/dev/video0`、`/dev/media0`）
- MediaMTX 版本需支持 `rpiCamera` source

可选环境变量：
- `TEA_CAM_STREAM_MEDIAMTX_BIN`
  - 指定 MediaMTX 可执行文件路径
- `TEA_CAM_STREAM_FFMPEG_BIN`
  - 指定 ffmpeg 可执行文件路径

说明：当前 HoneyTea 运行时逻辑按树莓派 / Linux 相机栈实现，若把 HoneyTea 运行在 macOS 上，插件可以被安装和启动，但相机流服务会返回“当前运行时需要 Linux / Raspberry Pi camera stack”的错误状态。

## 摄像头参数
OrangeTea 页面可直接调整以下参数，并通过 LemonTea 转发给 HoneyTea：
- `camera_id`
- `width` / `height`
- `fps`
- `bitrate`
- `idr_period`
- `brightness`
- `contrast`
- `saturation`
- `sharpness`
- `exposure`
- `awb`
- `denoise`
- `shutter`
- `gain`
- `ev`
- `hflip` / `vflip`
- `stream_path`
- `rtsp_port`
- `webrtc_port`
- `webrtc_udp_port`
- `yolo_port`
- `public_host`

参数保存后会写入 HoneyTea 插件目录下的 `runtime/camera-config.json`，后续重启插件会自动恢复。

## 日志与排障
摄像头插件的 client/server 现在都会把日志同时输出到标准输出和运行目录下的 `runtime` 日志文件，便于离线排查。

- HoneyTea 侧（client）：`runtime/cam-lan-stream-client.log`
- LemonTea 侧（server）：`runtime/cam-lan-stream-server.log`

日志内容包含：
- 插件启动信息（运行目录、日志文件路径）
- `start_stream` / `stop_stream` / `set_config` 等关键动作
- MediaMTX 与 ffmpeg 子进程启动、退出状态
- 自动重启次数与失败原因
- 桥接转发错误、JSON 解析异常

说明：
- 日志文件位于各自进程的运行目录（即程序当前工作目录下的 `runtime`）。
- HoneyTea 侧日志中会带 `[mediamtx]`、`[relay]` 前缀，分别对应子进程输出，适合快速定位媒体链路故障。

## 导出脚本

### macOS LemonTea + macOS HoneyTea
```bash
./scripts/macOS_export_cam_lan_stream_plugin.sh
```

默认输出：
- `dist/plugin-exports/cam-lan-stream/cam-lan-stream-unified-macos.tar.gz`

### macOS LemonTea + 树莓派5 HoneyTea
```bash
./scripts/export_cam_lan_stream_plugin_linux_x64.sh \
  --lemon-platform macos \
  --honey-platform rpi5
```

默认输出：
- `dist/plugin-exports/cam-lan-stream/cam-lan-stream-unified-lemon-macos-honey-rpi5.tar.gz`

### Linux x64 LemonTea + 树莓派5 HoneyTea
```bash
./scripts/export_cam_lan_stream_plugin_linux_x64.sh \
  --lemon-platform linux-x64 \
  --honey-platform rpi5
```

默认输出：
- `dist/plugin-exports/cam-lan-stream/cam-lan-stream-unified-lemon-linux-x64-honey-rpi5.tar.gz`

## 安装步骤
1. 先让 HoneyTea 在线连接到 LemonTea。
2. 在 OrangeTea 的“统一插件管理台”上传 unified 包。
3. 在安装确认框中确认插件信息，执行统一安装。
4. 系统会把：
   - server 运行时安装到 LemonTea
   - client 运行时安装到 HoneyTea
   - frontend 页面安装到 OrangeTea 动态插件目录
5. 在插件详情页里：
   - 启动 LemonTea 侧插件
   - 启动 HoneyTea 侧插件
   - 启动视频服务

## OrangeTea 页面功能
OrangeTea 的摄像头插件页内置了：
- HoneyTea 节点选择
- LemonTea / HoneyTea 插件状态查看
- 摄像头参数编辑与保存
- WebRTC 实时预览
- 保存截图（前端 canvas）
- 录制视频（前端 MediaRecorder）
- YOLO / 第三方程序接入示例代码

## 输出接口

### 1. WebRTC 页面
- `http://<HoneyTea-IP>:<webrtc_port>/<stream_path>`

这是 MediaMTX 自带的 WebRTC 页面，适合排查流是否已就绪。

### 2. WebRTC WHEP
- `http://<HoneyTea-IP>:<webrtc_port>/<stream_path>/whep`

OrangeTea 页面默认通过这个地址自己建立 `RTCPeerConnection`，因此可以直接拿到 `MediaStream` 做截图和录制。

### 3. RTSP 调试地址
- `rtsp://<HoneyTea-IP>:<rtsp_port>/<stream_path>`

适合用 `ffplay`、`ffmpeg`、VLC 等工具直接检查 MediaMTX 输出。

### 4. YOLO 裸 H.264 over TCP
- `tcp://<HoneyTea-IP>:<yolo_port>`

输出内容为 Annex-B 格式的裸 H.264 字节流。插件内部使用 `ffmpeg` 从本地 RTSP 路径抽取 H.264 elementary stream，再分发给所有 TCP 连接客户端。后续你的 YOLO 插件只要连上这个端口，就可以直接消费这路流。

## 接入示例

### JavaScript / WebRTC WHEP
```javascript
const video = document.querySelector('#preview')
const whepUrl = 'http://<HoneyTea-IP>:8889/rpi-camera/whep'

const pc = new RTCPeerConnection()
pc.addTransceiver('video', { direction: 'recvonly' })
pc.ontrack = (event) => {
  video.srcObject = event.streams[0]
}

const offer = await pc.createOffer()
await pc.setLocalDescription(offer)
await new Promise((resolve) => {
  if (pc.iceGatheringState === 'complete') return resolve()
  pc.addEventListener('icegatheringstatechange', () => pc.iceGatheringState === 'complete' && resolve(), { once: true })
})

const resp = await fetch(whepUrl, {
  method: 'POST',
  headers: { 'Content-Type': 'application/sdp' },
  body: pc.localDescription.sdp,
})
await pc.setRemoteDescription({ type: 'answer', sdp: await resp.text() })
```

### Python / YOLO 裸流
```python
import socket

host = '<HoneyTea-IP>'
port = 20000

sock = socket.create_connection((host, port), timeout=5)
with open('camera.h264', 'wb') as output:
    while True:
        chunk = sock.recv(65536)
        if not chunk:
            break
        output.write(chunk)
```

### ffplay 调试
```bash
ffplay -fflags nobuffer -flags low_delay -f h264 tcp://<HoneyTea-IP>:20000
ffplay -rtsp_transport tcp rtsp://<HoneyTea-IP>:8554/rpi-camera
```

## 端口需求
默认端口如下，可在插件页修改：
- `8554`：RTSP
- `8889`：WebRTC / WHEP HTTP
- `8189`：WebRTC UDP
- `20000`：YOLO 裸 H.264 TCP

如果 LemonTea、HoneyTea、OrangeTea 不在同一台机器，除了 LemonTea 的 `9527/9528` 之外，还需要确保浏览器所在网络能够访问 HoneyTea 上述端口。

## 与本次树莓派摄像头参数的对应关系
你提供的摄像头信息中已经确认：
- 设备工作正常
- 传感器为 `ov5647`
- 树莓派相机栈可正常输出 H.264
- 调谐文件位于 `/usr/share/libcamera/ipa/rpi/pisp/ov5647.json`

插件会在 HoneyTea 侧优先尝试自动检测该调谐文件；如果你后续切换摄像头或改动系统镜像，也可以通过 `tuning_file` 字段手动覆盖。

## 注意事项
- 当前 HoneyTea 运行时依赖树莓派相机栈，因此最合适的目标平台是 `rpi5`。
- `public_host` 必须是 OrangeTea 浏览器能够访问到的 HoneyTea 地址，否则 WHEP / RTSP / YOLO 地址会展示正确格式但浏览器或客户端无法连通。
- 如果你后续要写 YOLO 插件，优先直接消费 `tcp://<HoneyTea-IP>:<yolo_port>` 这路流，不必再从 OrangeTea 或 WebRTC 页面里二次抓取。