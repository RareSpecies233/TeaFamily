#ifdef TEA_HTTPLIB_HEADER_FILE
#include TEA_HTTPLIB_HEADER_FILE
#else
#include <httplib.h>
#endif
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace {

struct CameraState {
    std::string camera_id;
    std::string camera_label;
    std::vector<unsigned char> jpeg_frame;
    uint64_t last_frame_ms = 0;
};

struct DeviceState {
    std::string device_id;
    std::string device_name;
    std::string user_agent;
    std::string remote_ip;
    uint64_t last_seen_ms = 0;
    std::unordered_map<std::string, CameraState> cameras;
};

std::mutex g_out_mutex;
std::mutex g_data_mutex;
std::unordered_map<std::string, DeviceState> g_devices;

httplib::Server g_http_server;
std::thread g_http_thread;
std::atomic<bool> g_running{true};

std::string g_bind_host = "0.0.0.0";
std::string g_public_host = "127.0.0.1";
int g_listen_port = 19731;

uint64_t nowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        begin++;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }
    return value.substr(begin, end - begin);
}

std::string normalizeId(const std::string& value, const std::string& fallback) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '-' || c == '_' || c == '.') {
            out.push_back(c);
        }
    }
    if (out.empty()) {
        return fallback;
    }
    return out;
}

bool parsePort(const char* value, int& out_port) {
    if (!value || !*value) {
        return false;
    }
    try {
        const int parsed = std::stoi(value);
        if (parsed < 1 || parsed > 65535) {
            return false;
        }
        out_port = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

void sendMessage(const json& payload) {
    std::lock_guard<std::mutex> lock(g_out_mutex);
    std::cout << payload.dump() << "\n";
    std::cout.flush();
}

std::string inferRemoteIp(const httplib::Request& req) {
    auto forwarded = req.get_header_value("X-Forwarded-For");
    if (!forwarded.empty()) {
        auto comma = forwarded.find(',');
        return trim(forwarded.substr(0, comma));
    }
    return req.remote_addr;
}

bool decodeBase64(const std::string& in, std::vector<unsigned char>& out) {
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::array<int, 256> table{};
    table.fill(-1);
    for (size_t i = 0; i < chars.size(); ++i) {
        table[static_cast<unsigned char>(chars[i])] = static_cast<int>(i);
    }

    int val = 0;
    int valb = -8;
    for (unsigned char c : in) {
        if (std::isspace(c)) {
            continue;
        }
        if (c == '=') {
            break;
        }
        const int decoded = table[c];
        if (decoded < 0) {
            return false;
        }
        val = (val << 6) + decoded;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return true;
}

bool decodeImageDataUrl(const std::string& data_url, std::vector<unsigned char>& jpeg, std::string& error) {
    static const std::string prefix = "data:image/jpeg;base64,";
    if (data_url.rfind(prefix, 0) != 0) {
        error = "image_data_url must start with data:image/jpeg;base64,";
        return false;
    }

    const std::string encoded = data_url.substr(prefix.size());
    if (encoded.empty()) {
        error = "empty image payload";
        return false;
    }

    if (!decodeBase64(encoded, jpeg) || jpeg.empty()) {
        error = "invalid base64 image payload";
        return false;
    }

    return true;
}

json snapshotDevices() {
    std::lock_guard<std::mutex> lock(g_data_mutex);

    json devices = json::array();
    for (const auto& it : g_devices) {
        const auto& dev = it.second;
        json cameras = json::array();
        for (const auto& cam_it : dev.cameras) {
            const auto& cam = cam_it.second;
            cameras.push_back({
                {"camera_id", cam.camera_id},
                {"camera_label", cam.camera_label},
                {"last_frame_ms", cam.last_frame_ms}
            });
        }

        std::sort(cameras.begin(), cameras.end(), [](const json& a, const json& b) {
            return a.value("camera_id", std::string()) < b.value("camera_id", std::string());
        });

        devices.push_back({
            {"device_id", dev.device_id},
            {"device_name", dev.device_name},
            {"remote_ip", dev.remote_ip},
            {"last_seen_ms", dev.last_seen_ms},
            {"cameras", cameras}
        });
    }

    std::sort(devices.begin(), devices.end(), [](const json& a, const json& b) {
        return a.value("device_id", std::string()) < b.value("device_id", std::string());
    });

    return devices;
}

json buildStatusResponse(const std::string& request_id) {
    json payload = {
        {"action", "status_result"},
        {"plugin", "cam-lan-stream"},
        {"server", {
            {"bind_host", g_bind_host},
            {"port", g_listen_port},
            {"public_origin", "http://" + g_public_host + ":" + std::to_string(g_listen_port)},
            {"mobile_page_url", "http://" + g_public_host + ":" + std::to_string(g_listen_port) + "/"}
        }},
        {"devices", snapshotDevices()}
    };

    if (!request_id.empty()) {
        payload["request_id"] = request_id;
    }

    return payload;
}

std::string buildMobilePageHtml() {
    return R"HTML(<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>TeaFamily 局域网摄像头上报</title>
  <style>
    :root { --bg: #f7fbff; --card: #ffffff; --line: #d8e5f6; --text: #203247; --muted: #5f7288; --accent: #2e7dd7; }
    * { box-sizing: border-box; }
    body { margin: 0; padding: 14px; background: linear-gradient(160deg, #eef6ff, #f8fffb); color: var(--text); font-family: "Noto Sans SC", "PingFang SC", sans-serif; }
    .card { max-width: 760px; margin: 0 auto; background: var(--card); border: 1px solid var(--line); border-radius: 14px; padding: 14px; box-shadow: 0 10px 24px rgba(23, 63, 104, .08); }
    h1 { margin: 0 0 10px; font-size: 20px; }
    p { margin: 6px 0; color: var(--muted); }
    .row { display: flex; flex-wrap: wrap; gap: 10px; margin: 10px 0; }
    .col { flex: 1 1 220px; min-width: 220px; }
    label { display: block; font-size: 12px; color: var(--muted); margin-bottom: 4px; }
    input, select, button { width: 100%; border: 1px solid #cdddf1; border-radius: 10px; padding: 9px 10px; font-size: 14px; background: #fff; }
    button { cursor: pointer; background: var(--accent); color: #fff; border: none; font-weight: 600; }
    button.secondary { background: #7c8ea6; }
    video { width: 100%; background: #0f1b29; border-radius: 10px; min-height: 220px; }
    .status { padding: 8px 10px; border-radius: 8px; background: #edf4ff; color: #1f4776; font-size: 13px; }
    .warn { background: #fff2dd; color: #7b4c07; }
  </style>
</head>
<body>
  <div class="card">
    <h1>TeaFamily 局域网摄像头上报</h1>
    <p>选择摄像头后，页面会持续将 JPEG 帧上传到 LemonTea 插件服务。OrangeTea 可实时查看设备与画面。</p>
    <p class="status warn">提示：移动浏览器通常要求 HTTPS 或局域网可信环境才能调用摄像头权限。</p>

    <div class="row">
      <div class="col">
        <label>设备名称</label>
        <input id="deviceName" placeholder="例如：Alice-iPhone" />
      </div>
      <div class="col">
        <label>摄像头</label>
        <select id="cameraSelect"></select>
      </div>
    </div>

    <div class="row">
      <div class="col"><button id="refreshBtn" class="secondary">刷新摄像头列表</button></div>
      <div class="col"><button id="startBtn">开始串流</button></div>
      <div class="col"><button id="stopBtn" class="secondary">停止串流</button></div>
    </div>

    <video id="preview" playsinline autoplay muted></video>
    <canvas id="canvas" width="640" height="360" style="display:none"></canvas>

    <div class="row">
      <div class="col"><div id="state" class="status">等待启动</div></div>
      <div class="col"><div id="fps" class="status">帧率: -</div></div>
    </div>
  </div>

  <script>
    const deviceIdKey = 'tea_cam_stream_device_id'
    let deviceId = localStorage.getItem(deviceIdKey)
    if (!deviceId) {
      deviceId = 'mobile-' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2, 8)
      localStorage.setItem(deviceIdKey, deviceId)
    }

    const elDeviceName = document.getElementById('deviceName')
    const elCamera = document.getElementById('cameraSelect')
    const elState = document.getElementById('state')
    const elFps = document.getElementById('fps')
    const video = document.getElementById('preview')
    const canvas = document.getElementById('canvas')
    const ctx = canvas.getContext('2d')

    let stream = null
    let pushTimer = null
    let registerTimer = null
    let frameCount = 0
    let fpsTimer = null

    function setState(text, warn = false) {
      elState.textContent = text
      elState.classList.toggle('warn', !!warn)
    }

    function currentCameraLabel() {
      const opt = elCamera.options[elCamera.selectedIndex]
      return opt ? (opt.textContent || '') : ''
    }

    async function jsonFetch(path, body) {
      const resp = await fetch(path, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      })
      if (!resp.ok) {
        throw new Error('HTTP ' + resp.status)
      }
      return resp.json().catch(() => ({}))
    }

    async function refreshCameras() {
      try {
        const devices = await navigator.mediaDevices.enumerateDevices()
        const cameras = devices.filter((d) => d.kind === 'videoinput')
        elCamera.innerHTML = cameras
          .map((d, idx) => `<option value="${d.deviceId}">${d.label || ('摄像头 ' + (idx + 1))}</option>`)
          .join('')

        if (!elCamera.value && cameras.length > 0) {
          elCamera.value = cameras[0].deviceId
        }
      } catch (e) {
        setState('读取摄像头列表失败：' + (e.message || e), true)
      }
    }

    async function registerDevice() {
      const devices = await navigator.mediaDevices.enumerateDevices()
      const cameras = devices
        .filter((d) => d.kind === 'videoinput')
        .map((d, idx) => ({ id: d.deviceId, label: d.label || ('摄像头 ' + (idx + 1)) }))

      await jsonFetch('/api/register', {
        device_id: deviceId,
        device_name: (elDeviceName.value || '').trim() || navigator.userAgent,
        cameras,
      })
    }

    async function pushFrameLoop() {
      if (!stream) {
        return
      }

      try {
        const targetWidth = 640
        const targetHeight = 360
        canvas.width = targetWidth
        canvas.height = targetHeight
        ctx.drawImage(video, 0, 0, targetWidth, targetHeight)
        const dataUrl = canvas.toDataURL('image/jpeg', 0.65)

        await jsonFetch('/api/publish-frame', {
          device_id: deviceId,
          device_name: (elDeviceName.value || '').trim() || navigator.userAgent,
          camera_id: elCamera.value,
          camera_label: currentCameraLabel(),
          image_data_url: dataUrl,
        })

        frameCount += 1
      } catch (e) {
        setState('上传帧失败：' + (e.message || e), true)
      } finally {
        pushTimer = setTimeout(pushFrameLoop, 220)
      }
    }

    async function startStreaming() {
      if (!navigator.mediaDevices?.getUserMedia) {
        setState('当前浏览器不支持摄像头接口', true)
        return
      }

      if (stream) {
        stopStreaming()
      }

      try {
        const cameraId = elCamera.value
        stream = await navigator.mediaDevices.getUserMedia({
          video: cameraId ? { deviceId: { exact: cameraId } } : true,
          audio: false,
        })
        video.srcObject = stream
        await video.play()

        await registerDevice()
        pushFrameLoop()

        if (registerTimer) clearInterval(registerTimer)
        registerTimer = setInterval(() => {
          registerDevice().catch(() => {})
        }, 3000)

        if (fpsTimer) clearInterval(fpsTimer)
        frameCount = 0
        fpsTimer = setInterval(() => {
          elFps.textContent = '帧率: ' + frameCount * 2 + ' FPS（近似）'
          frameCount = 0
        }, 2000)

        setState('串流中：' + (currentCameraLabel() || elCamera.value || '默认摄像头'))
      } catch (e) {
        setState('启动失败：' + (e.message || e), true)
      }
    }

    function stopStreaming() {
      if (pushTimer) {
        clearTimeout(pushTimer)
        pushTimer = null
      }
      if (registerTimer) {
        clearInterval(registerTimer)
        registerTimer = null
      }
      if (fpsTimer) {
        clearInterval(fpsTimer)
        fpsTimer = null
      }
      if (stream) {
        stream.getTracks().forEach((track) => track.stop())
        stream = null
      }
      video.srcObject = null
      elFps.textContent = '帧率: -'
      setState('已停止')
    }

    document.getElementById('refreshBtn').addEventListener('click', () => {
      refreshCameras().then(() => registerDevice().catch(() => {}))
    })
    document.getElementById('startBtn').addEventListener('click', startStreaming)
    document.getElementById('stopBtn').addEventListener('click', stopStreaming)

    window.addEventListener('beforeunload', () => {
      stopStreaming()
    })

    ;(async () => {
      elDeviceName.value = localStorage.getItem('tea_cam_stream_device_name') || ''
      elDeviceName.addEventListener('change', () => {
        localStorage.setItem('tea_cam_stream_device_name', elDeviceName.value || '')
      })
      await refreshCameras()
      await registerDevice().catch(() => {})
      setState('摄像头就绪，点击“开始串流”')
    })()
  </script>
</body>
</html>)HTML";
}

void configureHttpRoutes() {
    g_http_server.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    g_http_server.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(buildMobilePageHtml(), "text/html; charset=utf-8");
    });

    g_http_server.Get("/api/devices", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{{"devices", snapshotDevices()}}.dump(), "application/json");
    });

    g_http_server.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            const std::string raw_device_id = body.value("device_id", std::string());
            const std::string device_id = normalizeId(raw_device_id, "unknown-device");
            const std::string device_name = body.value("device_name", std::string("mobile-device"));

            std::lock_guard<std::mutex> lock(g_data_mutex);
            auto& device = g_devices[device_id];
            device.device_id = device_id;
            device.device_name = device_name;
            device.user_agent = req.get_header_value("User-Agent");
            device.remote_ip = inferRemoteIp(req);
            device.last_seen_ms = nowMs();

            if (body.contains("cameras") && body["cameras"].is_array()) {
                for (const auto& item : body["cameras"]) {
                    if (!item.is_object()) {
                        continue;
                    }
                    const std::string camera_id = normalizeId(item.value("id", std::string()), std::string());
                    if (camera_id.empty()) {
                        continue;
                    }
                    auto& camera = device.cameras[camera_id];
                    camera.camera_id = camera_id;
                    camera.camera_label = item.value("label", camera_id);
                }
            }

            res.set_content(json{{"success", true}, {"device_id", device_id}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"success", false}, {"error", e.what()}}.dump(), "application/json");
        }
    });

    g_http_server.Post("/api/publish-frame", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            const std::string device_id = normalizeId(body.value("device_id", std::string()), "unknown-device");
            const std::string camera_id = normalizeId(body.value("camera_id", std::string()), "default-camera");
            const std::string camera_label = body.value("camera_label", camera_id);
            const std::string device_name = body.value("device_name", std::string("mobile-device"));
            const std::string image_data_url = body.value("image_data_url", std::string());

            std::vector<unsigned char> jpeg;
            std::string decode_error;
            if (!decodeImageDataUrl(image_data_url, jpeg, decode_error)) {
                res.status = 400;
                res.set_content(json{{"success", false}, {"error", decode_error}}.dump(), "application/json");
                return;
            }

            {
                std::lock_guard<std::mutex> lock(g_data_mutex);
                auto& device = g_devices[device_id];
                device.device_id = device_id;
                device.device_name = device_name;
                device.user_agent = req.get_header_value("User-Agent");
                device.remote_ip = inferRemoteIp(req);
                device.last_seen_ms = nowMs();

                auto& camera = device.cameras[camera_id];
                camera.camera_id = camera_id;
                camera.camera_label = camera_label;
                camera.last_frame_ms = device.last_seen_ms;
                camera.jpeg_frame = std::move(jpeg);
            }

            res.set_content(json{{"success", true}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"success", false}, {"error", e.what()}}.dump(), "application/json");
        }
    });

    g_http_server.Get(R"(/api/frame/([^/]+)/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        const std::string device_id = normalizeId(req.matches[1].str(), std::string());
        const std::string camera_id = normalizeId(req.matches[2].str(), std::string());

        std::vector<unsigned char> frame;
        {
            std::lock_guard<std::mutex> lock(g_data_mutex);
            const auto dev_it = g_devices.find(device_id);
            if (dev_it == g_devices.end()) {
                res.status = 404;
                res.set_content(R"({"error":"device not found"})", "application/json");
                return;
            }

            const auto cam_it = dev_it->second.cameras.find(camera_id);
            if (cam_it == dev_it->second.cameras.end() || cam_it->second.jpeg_frame.empty()) {
                res.status = 404;
                res.set_content(R"({"error":"camera frame not found"})", "application/json");
                return;
            }

            frame = cam_it->second.jpeg_frame;
        }

        res.set_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        res.set_content(reinterpret_cast<const char*>(frame.data()), static_cast<size_t>(frame.size()), "image/jpeg");
    });

    g_http_server.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{{"ok", true}, {"plugin", "cam-lan-stream"}}.dump(), "application/json");
    });
}

void stopHttpServer() {
    g_running.store(false);
    g_http_server.stop();
    if (g_http_thread.joinable()) {
        g_http_thread.join();
    }
}

void handleIncomingMessage(const json& msg) {
    const std::string cmd = msg.value("cmd", std::string());
    const std::string action = msg.value("action", std::string());
    const std::string request_id = msg.value("request_id", std::string());

    if (cmd == "stop") {
        sendMessage({{"action", "stopped"}, {"plugin", "cam-lan-stream"}});
        stopHttpServer();
        return;
    }

    if (cmd == "status" || action == "status" || action == "list_devices") {
        sendMessage(buildStatusResponse(request_id));
        return;
    }

    if (action == "ping") {
        sendMessage({{"action", "pong"}, {"request_id", request_id}});
        return;
    }
}

}  // namespace

int main() {
    spdlog::set_level(spdlog::level::info);

    int env_port = 0;
    if (parsePort(std::getenv("TEA_CAM_STREAM_PORT"), env_port)) {
        g_listen_port = env_port;
    }

    const char* bind_host = std::getenv("TEA_CAM_STREAM_BIND_HOST");
    if (bind_host && *bind_host) {
        g_bind_host = bind_host;
    }

    const char* public_host = std::getenv("TEA_CAM_STREAM_PUBLIC_HOST");
    if (public_host && *public_host) {
        g_public_host = public_host;
    }

    configureHttpRoutes();

    g_http_thread = std::thread([]() {
        spdlog::info("cam-lan-stream http server listening on {}:{}", g_bind_host, g_listen_port);
        if (!g_http_server.listen(g_bind_host.c_str(), g_listen_port)) {
            if (g_running.load()) {
                sendMessage({
                    {"action", "error"},
                    {"plugin", "cam-lan-stream"},
                    {"message", "failed to listen http server on " + g_bind_host + ":" + std::to_string(g_listen_port)}
                });
            }
        }
    });

    sendMessage({
        {"action", "ready"},
        {"plugin", "cam-lan-stream"},
        {"role", "server"},
        {"server", {
            {"bind_host", g_bind_host},
            {"port", g_listen_port},
            {"public_origin", "http://" + g_public_host + ":" + std::to_string(g_listen_port)},
            {"mobile_page_url", "http://" + g_public_host + ":" + std::to_string(g_listen_port) + "/"}
        }}
    });

    std::string line;
    while (g_running.load() && std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        try {
            auto msg = json::parse(line);
            handleIncomingMessage(msg);
        } catch (const std::exception& e) {
            sendMessage({
                {"action", "error"},
                {"plugin", "cam-lan-stream"},
                {"message", std::string("invalid json message: ") + e.what()}
            });
        }
    }

    stopHttpServer();
    return 0;
}
