# 摄像头硬件参数与采集结果

## 设备概览
- 目标文件夹: `/home/gonc/TempTest`
- 拍照文件: `photo.jpg`
- 录制文件: `video.h264`, `video.mp4`
- 使用工具: `rpicam-still`, `rpicam-vid`, `ffmpeg`

## 检测到的摄像头设备
- Camera pipeline: `rp1-cfe`（平台 `platform:1f00128000.csi`）
- 关联设备节点:
  - `/dev/video0` - 主摄像头输入
  - `/dev/video1..7` - 其他 CSI/ISP 相关节点
  - `/dev/media0` - 摄像头媒体设备
- 驱动信息:
  - 驱动名称: `rp1-cfe`
  - 驱动版本: `6.12.47`
  - 硬件修订: `0x00114666` (1132134)
  - Capabilities: Video Capture, Metadata Capture, Metadata Output, I/O MC, Streaming, Extended Pix Format

## 摄像头传感器信息
- 传感器型号: `ov5647`
- I2C 地址: `0x36`
- 摄像头管线处理: `rpi/pisp`
- 使用的调谐文件: `/usr/share/libcamera/ipa/rpi/pisp/ov5647.json`

## 支持的主要像素格式（/dev/video0）
- YUYV 4:2:2
- UYVY 4:2:2
- RGB565、RGB24、BGR24、RGB32
- Bayer 8-bit/10-bit/12-bit/14-bit/16-bit 格式
- PiSP 8b 压缩格式
- 灰度格式

## 实际采集参数
### 拍照
- 使用命令: `/usr/bin/rpicam-still -n -o /home/gonc/TempTest/photo.jpg -t 2000`
- 拍照结果: `photo.jpg`
- 自动模式选择:
  - 1296x972 YUV420 输出
  - 1296x972 RAW 传感器格式 SGBRG10_1X10

### 录像
- 使用命令: `/usr/bin/rpicam-vid -n -t 10000 -o /home/gonc/TempTest/video.h264`
- 录制结果: `video.h264`
- 录制分辨率: 640x480
- 视频编码器: H.264
- 录制时长: 10 秒

## 视频文件转换
- 为方便播放，已将 `video.h264` 转换为 `video.mp4`
- 转换命令: `ffmpeg -y -i /home/gonc/TempTest/video.h264 -c copy /home/gonc/TempTest/video.mp4`

## 当前结果文件
- `photo.jpg` — 拍摄的一张图片
- `video.h264` — 原始 H.264 流视频
- `video.mp4` — 可直接播放的 MP4 视频

## 说明
- 已确认摄像头正常连接并成功拍照、录制视频。
- 如果需要更高分辨率、更多曝光控制或自动对焦参数，可继续使用 `rpicam-still` / `rpicam-vid` 的更多选项进行设置。
