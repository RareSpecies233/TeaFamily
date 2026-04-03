# 双滑块控制插件使用文档

## 插件类型
- `lemon-only`（仅 LemonTea + OrangeTea）
- 统一包会自动分发到 LemonTea，并安装 OrangeTea 前端页面

## 功能概览
- 双界面：
  - 控制界面：两个滑块，支持移动端触控同时滑动
  - 展示界面：只读显示两个滑块当前值
- 移动端模式：一键进入全屏控制页，提供左右布局的大尺寸双摇杆
- 桌面端可配置 `3 + 3` 键盘按键
  - 滑块 A：`+10% / -10% / 归零`
  - 滑块 B：`+10% / -10% / 归零`
- 低延迟通信：
  - OrangeTea 使用 HTTP 快通道（浏览器环境不能直连 UDP）
  - 插件额外提供 UDP 接口给原生程序/其他后端服务读取与写入
- 数值可通过 API 实时读取，供其他插件或外部程序使用

## 导出（macOS）
```bash
./scripts/macOS_export_dual_slider_plugin.sh
```

默认输出：
- `dist/plugin-exports/dual-slider/dual-slider-unified-macos.tar.gz`

## 导出（跨平台可选）
```bash
./scripts/export_dual_slider_plugin_linux_x64_lemon_rpi5_honey.sh \
  --lemon-platform linux-x64 \
  --honey-platform rpi5
```

也支持：
```bash
./scripts/export_dual_slider_plugin_linux_x64_lemon_rpi5_honey.sh \
  --lemon-platform macos \
  --honey-platform macos
```

## 安装步骤
1. 在 OrangeTea 的统一插件管理台上传 `dual-slider-unified-*.tar.gz`。
2. 预览插件信息后确认安装。
3. 安装完成后，仅需启动 LemonTea 的 `dual-slider`。
4. 打开插件页面后即可直接控制与查看数值。

## 实时值 API（HTTP）
通过 LemonTea 的 `plugin-rpc` 获取：

```bash
curl -X POST http://127.0.0.1:9528/api/plugin-rpc \
  -H 'Content-Type: application/json' \
  -d '{
    "plugin": "dual-slider",
    "data": { "action": "get_values" },
    "timeout_ms": 1500,
    "expected_actions": ["values"]
  }'
```

响应示例：
```json
{
  "success": true,
  "matched": true,
  "response": {
    "action": "values",
    "slider_a": 20,
    "slider_b": -30,
    "updated_at_ms": 1770012345678
  }
}
```

## UDP 接口（原生程序可选）

默认监听：
- `127.0.0.1:19731`

可通过环境变量调整：
- `TEA_DUAL_SLIDER_UDP_BIND`
- `TEA_DUAL_SLIDER_UDP_PORT`

说明：
- OrangeTea 浏览器无法直接使用 UDP，因此前端仍走 HTTP 快通道。
- UDP 主要用于本机原生程序或其他服务做更低延迟读写。

UDP 请求示例：
```json
{"action":"get_values"}
```

```json
{"action":"set_values","slider_a":30,"slider_b":-20}
```

查询传输信息（HTTP）：
```json
{"action":"get_transport"}
```

## 其他插件读取数值
- 方式 1：通过 LemonTea HTTP API（上面示例）读取。
- 方式 2：插件间消息模式：向 `dual-slider` 发送 `{"action":"get_values"}`，`dual-slider` 会返回 `slider_values` 数据包。

## 支持的动作
- `get_values`：读取当前值
- `set_values`：同时设置 `slider_a` 与 `slider_b`
- `adjust`：对单个滑块执行 `inc/dec/reset`
- `reset_all`：两个滑块同时归零
- `get_transport`：获取当前 UDP 监听配置
