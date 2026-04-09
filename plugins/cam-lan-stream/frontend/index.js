import {
  createApp,
  computed,
  onMounted,
  onUnmounted,
  reactive,
  ref,
  watch,
} from 'https://unpkg.com/vue@3/dist/vue.esm-browser.js'

const STYLE_ID = 'tea-cam-mtx-style'

function ensureStyle() {
  if (document.getElementById(STYLE_ID)) {
    return
  }

  const style = document.createElement('style')
  style.id = STYLE_ID
  style.textContent = `
    .tea-cam-root {
      color: #2e2415;
      font-family: Manrope, 'Noto Sans SC', 'PingFang SC', sans-serif;
    }
    .tea-cam-shell {
      display: grid;
      gap: 14px;
    }
    .tea-cam-hero,
    .tea-cam-card {
      border: 1px solid #dfd1ba;
      border-radius: 18px;
      background:
        radial-gradient(circle at top right, rgba(255, 242, 203, 0.9), transparent 38%),
        linear-gradient(135deg, rgba(255, 249, 240, 0.96), rgba(244, 253, 255, 0.98));
      box-shadow: 0 16px 40px rgba(96, 77, 33, 0.08);
    }
    .tea-cam-hero {
      display: grid;
      grid-template-columns: minmax(0, 1.2fr) minmax(280px, 0.8fr);
      gap: 18px;
      padding: 20px;
    }
    .tea-cam-title {
      margin: 0;
      font-size: 26px;
      line-height: 1.12;
      color: #30210e;
    }
    .tea-cam-subtitle {
      margin: 10px 0 0;
      max-width: 820px;
      color: #6a5738;
      line-height: 1.65;
      font-size: 14px;
    }
    .tea-cam-pill-row {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      margin-top: 14px;
    }
    .tea-cam-pill {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 7px 12px;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.9);
      border: 1px solid rgba(214, 195, 162, 0.9);
      color: #5a4726;
      font-size: 12px;
      font-weight: 700;
    }
    .tea-cam-hero-side {
      display: grid;
      gap: 10px;
      align-content: start;
    }
    .tea-cam-status-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .tea-cam-status-cell {
      padding: 12px;
      border-radius: 14px;
      background: rgba(255, 255, 255, 0.88);
      border: 1px solid rgba(222, 207, 177, 0.92);
    }
    .tea-cam-status-label {
      display: block;
      font-size: 11px;
      color: #7a6848;
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    .tea-cam-status-value {
      display: block;
      margin-top: 8px;
      font-size: 15px;
      color: #312511;
      font-weight: 700;
      word-break: break-word;
    }
    .tea-cam-banner {
      padding: 11px 14px;
      border-radius: 14px;
      background: rgba(255, 248, 235, 0.95);
      border: 1px solid rgba(222, 205, 171, 0.9);
      color: #6a542e;
      font-size: 13px;
    }
    .tea-cam-banner.error {
      background: rgba(255, 237, 237, 0.96);
      border-color: rgba(228, 177, 177, 0.95);
      color: #8b3737;
    }
    .tea-cam-grid {
      display: grid;
      grid-template-columns: minmax(300px, 360px) minmax(0, 1fr);
      gap: 14px;
    }
    .tea-cam-card {
      padding: 16px;
    }
    .tea-cam-card h3 {
      margin: 0 0 12px;
      font-size: 16px;
      color: #382b14;
    }
    .tea-cam-stack {
      display: grid;
      gap: 10px;
    }
    .tea-cam-field {
      display: grid;
      gap: 6px;
    }
    .tea-cam-field label {
      font-size: 12px;
      color: #775f37;
      font-weight: 700;
    }
    .tea-cam-field input,
    .tea-cam-field select,
    .tea-cam-field textarea {
      width: 100%;
      border: 1px solid #d4c3a4;
      border-radius: 12px;
      background: rgba(255, 255, 255, 0.92);
      color: #322613;
      padding: 10px 12px;
      font: inherit;
      box-sizing: border-box;
    }
    .tea-cam-field textarea {
      min-height: 118px;
      resize: vertical;
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
      font-size: 12px;
      line-height: 1.5;
    }
    .tea-cam-row {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .tea-cam-row.triple {
      grid-template-columns: repeat(3, minmax(0, 1fr));
    }
    .tea-cam-button-row {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
    }
    .tea-cam-btn {
      border: 0;
      border-radius: 12px;
      padding: 10px 16px;
      cursor: pointer;
      background: linear-gradient(135deg, #2d6fca, #2655a8);
      color: #fff;
      font-weight: 700;
      min-width: 124px;
      text-decoration: none;
      text-align: center;
    }
    .tea-cam-btn.secondary {
      background: linear-gradient(135deg, #f6e2b8, #dfbb73);
      color: #51360a;
    }
    .tea-cam-btn.danger {
      background: linear-gradient(135deg, #ca5b53, #a83a35);
    }
    .tea-cam-btn.ghost {
      background: rgba(255, 255, 255, 0.92);
      border: 1px solid #d9c7a6;
      color: #614724;
    }
    .tea-cam-btn:disabled {
      opacity: 0.55;
      cursor: not-allowed;
    }
    .tea-cam-preview-shell {
      display: grid;
      gap: 12px;
    }
    .tea-cam-preview {
      position: relative;
      overflow: hidden;
      border-radius: 18px;
      background:
        radial-gradient(circle at 15% 15%, rgba(255, 198, 83, 0.2), transparent 30%),
        linear-gradient(160deg, #141d2d, #0d1220 60%, #0e1728);
      min-height: 420px;
      border: 1px solid rgba(213, 194, 160, 0.7);
    }
    .tea-cam-video {
      width: 100%;
      height: 100%;
      object-fit: contain;
      background: transparent;
      min-height: 420px;
    }
    .tea-cam-empty {
      position: absolute;
      inset: 0;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 24px;
      color: #c7d5e5;
      font-size: 15px;
      text-align: center;
    }
    .tea-cam-overlay {
      position: absolute;
      left: 16px;
      right: 16px;
      bottom: 16px;
      display: flex;
      justify-content: space-between;
      gap: 10px;
      flex-wrap: wrap;
      pointer-events: none;
    }
    .tea-cam-overlay-chip {
      pointer-events: auto;
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 8px 11px;
      border-radius: 999px;
      background: rgba(12, 18, 30, 0.78);
      border: 1px solid rgba(237, 217, 184, 0.2);
      color: #edf3ff;
      font-size: 12px;
    }
    .tea-cam-dot {
      width: 9px;
      height: 9px;
      border-radius: 999px;
      background: #ea7d65;
      box-shadow: 0 0 0 6px rgba(234, 125, 101, 0.18);
    }
    .tea-cam-dot.live {
      background: #57dd8c;
      box-shadow: 0 0 0 6px rgba(87, 221, 140, 0.2);
    }
    .tea-cam-kv-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 10px;
    }
    .tea-cam-kv-item {
      padding: 12px;
      border-radius: 14px;
      background: rgba(255, 255, 255, 0.9);
      border: 1px solid rgba(218, 199, 166, 0.9);
    }
    .tea-cam-kv-item strong {
      display: block;
      font-size: 11px;
      color: #7d6841;
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    .tea-cam-kv-item span,
    .tea-cam-kv-item a {
      display: block;
      margin-top: 7px;
      color: #274770;
      word-break: break-all;
      text-decoration: none;
      font-weight: 600;
    }
    .tea-cam-api-list {
      margin: 0;
      padding-left: 18px;
      color: #5d4a2a;
      line-height: 1.7;
    }
    .tea-cam-api-list code {
      color: #244d7a;
      font-weight: 700;
    }
    .tea-cam-code-grid {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 10px;
    }
    .tea-cam-code-card {
      overflow: hidden;
      border-radius: 14px;
      border: 1px solid #dcccaf;
      background: rgba(252, 251, 247, 0.94);
    }
    .tea-cam-code-card h4 {
      margin: 0;
      padding: 10px 12px;
      background: rgba(249, 236, 203, 0.92);
      color: #573e16;
      font-size: 13px;
    }
    .tea-cam-code-card pre {
      margin: 0;
      padding: 12px;
      overflow: auto;
      font-size: 12px;
      line-height: 1.55;
      color: #283346;
      background: rgba(255, 255, 255, 0.94);
    }
    @media (max-width: 1180px) {
      .tea-cam-hero,
      .tea-cam-grid {
        grid-template-columns: 1fr;
      }
      .tea-cam-code-grid {
        grid-template-columns: 1fr;
      }
    }
    @media (max-width: 760px) {
      .tea-cam-status-grid,
      .tea-cam-row,
      .tea-cam-row.triple,
      .tea-cam-kv-grid {
        grid-template-columns: 1fr;
      }
      .tea-cam-preview,
      .tea-cam-video {
        min-height: 260px;
      }
    }
  `
  document.head.appendChild(style)
}

function normalizeApiBase(apiBase) {
  return String(apiBase || '/api').replace(/\/+$/, '') || '/api'
}

async function fetchJson(url, options) {
  const response = await fetch(url, options)
  const payload = await response.json().catch(() => ({}))
  if (!response.ok) {
    throw new Error(payload.error || `${response.status} ${response.statusText}`)
  }
  return payload
}

async function listClients(apiBase) {
  const payload = await fetchJson(`${apiBase}/clients`)
  return Array.isArray(payload.clients) ? payload.clients : []
}

async function listRemotePlugins(apiBase, nodeId) {
  if (!nodeId) {
    return []
  }
  const payload = await fetchJson(`${apiBase}/clients/${encodeURIComponent(nodeId)}/plugins`)
  return Array.isArray(payload.plugins) ? payload.plugins : []
}

async function listLocalPlugins(apiBase) {
  const payload = await fetchJson(`${apiBase}/plugins`)
  return Array.isArray(payload.plugins) ? payload.plugins : []
}

async function postJson(url, body) {
  return fetchJson(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
}

async function postPlain(url) {
  return fetchJson(url, { method: 'POST' })
}

function toNumber(value, fallback) {
  const num = Number(value)
  return Number.isFinite(num) ? num : fallback
}

class SimpleWhepReader {
  constructor({ url, onTrack, onError }) {
    this.url = url
    this.onTrack = onTrack
    this.onError = onError
    this.pc = null
    this.sessionUrl = ''
  }

  async start() {
    this.close()
    const connection = new RTCPeerConnection()
    this.pc = connection
    connection.addTransceiver('video', { direction: 'recvonly' })
    connection.ontrack = (event) => {
      this.onTrack?.(event)
    }
    connection.onconnectionstatechange = () => {
      if (connection.connectionState === 'failed' || connection.connectionState === 'disconnected') {
        this.onError?.(new Error(`WebRTC connection state: ${connection.connectionState}`))
      }
    }

    const offer = await connection.createOffer()
    await connection.setLocalDescription(offer)
    await this.waitIceComplete(connection)

    const response = await fetch(this.url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/sdp' },
      body: connection.localDescription?.sdp || offer.sdp,
    })

    if (!response.ok) {
      const text = await response.text().catch(() => '')
      throw new Error(text || `${response.status} ${response.statusText}`)
    }

    const location = response.headers.get('location')
    this.sessionUrl = location ? new URL(location, this.url).toString() : this.url
    const answerSdp = await response.text()
    await connection.setRemoteDescription({ type: 'answer', sdp: answerSdp })
  }

  waitIceComplete(connection) {
    if (connection.iceGatheringState === 'complete') {
      return Promise.resolve()
    }

    return new Promise((resolve) => {
      const handler = () => {
        if (connection.iceGatheringState === 'complete') {
          connection.removeEventListener('icegatheringstatechange', handler)
          resolve()
        }
      }
      connection.addEventListener('icegatheringstatechange', handler)
    })
  }

  close() {
    if (this.pc) {
      this.pc.close()
      this.pc = null
    }
    if (this.sessionUrl) {
      fetch(this.sessionUrl, { method: 'DELETE' }).catch(() => {})
      this.sessionUrl = ''
    }
  }
}

export function mount(container, ctx = {}) {
  ensureStyle()

  const pluginName = ctx.pluginName || 'cam-lan-stream'
  const apiBase = normalizeApiBase(ctx.apiBase)

  const app = createApp({
    setup() {
      const loading = ref(false)
      const saving = ref(false)
      const actionBusy = ref(false)
      const error = ref('')
      const info = ref('')
      const clients = ref([])
      const localState = ref('-')
      const remoteState = ref('-')
      const selectedNode = ref('')
      const status = ref(null)
      const isRecording = ref(false)
      const recordError = ref('')
      const recorder = ref(null)
      const recordedChunks = ref([])
      const videoEl = ref(null)
      const cameraForm = reactive({
        public_host: '',
        camera_id: 0,
        width: 1296,
        height: 972,
        fps: 30,
        bitrate: 5000000,
        idr_period: 30,
        brightness: 0,
        contrast: 1,
        saturation: 1,
        sharpness: 1,
        exposure: 'normal',
        awb: 'auto',
        denoise: 'off',
        shutter: 0,
        gain: 0,
        ev: 0,
        hflip: false,
        vflip: false,
        stream_path: 'rpi-camera',
        rtsp_port: 8554,
        webrtc_port: 8889,
        webrtc_udp_port: 8189,
        yolo_port: 20000,
      })

      let previewReader = null
      let previewStream = null
      let statusTimer = null
      let refreshTimer = null

      const service = computed(() => status.value?.service || {})
      const endpoints = computed(() => status.value?.endpoints || {})
      const system = computed(() => status.value?.system || {})
      const onlineClients = computed(() => clients.value.filter((item) => item.connected))
      const previewUrl = computed(() => {
        if (endpoints.value.webrtc_whep_url) {
          return endpoints.value.webrtc_whep_url
        }
        if (!cameraForm.public_host || !cameraForm.stream_path) {
          return ''
        }
        return `http://${cameraForm.public_host}:${cameraForm.webrtc_port}/${cameraForm.stream_path}/whep`
      })
      const previewPageUrl = computed(() => {
        if (endpoints.value.webrtc_player_url) {
          return endpoints.value.webrtc_player_url
        }
        if (!cameraForm.public_host || !cameraForm.stream_path) {
          return ''
        }
        return `http://${cameraForm.public_host}:${cameraForm.webrtc_port}/${cameraForm.stream_path}`
      })
      const apiDocsText = computed(() => [
        `YOLO 裸流: ${endpoints.value.yolo_tcp_url || `tcp://<HoneyTea-IP>:${cameraForm.yolo_port}`}`,
        `RTSP 原始路径: ${endpoints.value.rtsp_url || `rtsp://<HoneyTea-IP>:${cameraForm.rtsp_port}/${cameraForm.stream_path}`}`,
        `WebRTC 页面: ${previewPageUrl.value || `http://<HoneyTea-IP>:${cameraForm.webrtc_port}/${cameraForm.stream_path}`}`,
        `WHEP 地址: ${previewUrl.value || `http://<HoneyTea-IP>:${cameraForm.webrtc_port}/${cameraForm.stream_path}/whep`}`,
      ].join('\n'))
      const jsExample = computed(() => [
        `const video = document.querySelector('#preview')`,
        `const whepUrl = '${previewUrl.value || 'http://<HoneyTea-IP>:8889/rpi-camera/whep'}'`,
        '',
        'const pc = new RTCPeerConnection()',
        "pc.addTransceiver('video', { direction: 'recvonly' })",
        'pc.ontrack = (event) => {',
        '  video.srcObject = event.streams[0]',
        '}',
        'const offer = await pc.createOffer()',
        'await pc.setLocalDescription(offer)',
        'await new Promise((resolve) => {',
        "  if (pc.iceGatheringState === 'complete') return resolve()",
        "  pc.addEventListener('icegatheringstatechange', () => pc.iceGatheringState === 'complete' && resolve(), { once: true })",
        '})',
        "const resp = await fetch(whepUrl, { method: 'POST', headers: { 'Content-Type': 'application/sdp' }, body: pc.localDescription.sdp })",
        "await pc.setRemoteDescription({ type: 'answer', sdp: await resp.text() })",
      ].join('\n'))
      const pythonExample = computed(() => [
        'import socket',
        '',
        `host = '${cameraForm.public_host || '<HoneyTea-IP>'}'`,
        `port = ${cameraForm.yolo_port}`,
        '',
        'sock = socket.create_connection((host, port), timeout=5)',
        "with open('camera.h264', 'wb') as output:",
        '    while True:',
        '        chunk = sock.recv(65536)',
        '        if not chunk:',
        '            break',
        '        output.write(chunk)',
      ].join('\n'))
      const ffplayExample = computed(() => [
        `ffplay -fflags nobuffer -flags low_delay -f h264 '${endpoints.value.yolo_tcp_url || `tcp://<HoneyTea-IP>:${cameraForm.yolo_port}`}'`,
        '',
        `ffplay -rtsp_transport tcp '${endpoints.value.rtsp_url || `rtsp://<HoneyTea-IP>:${cameraForm.rtsp_port}/${cameraForm.stream_path}`}'`,
      ].join('\n'))

      function setMessage(message, isError = false) {
        if (isError) {
          error.value = message
          info.value = ''
        } else {
          info.value = message
          error.value = ''
        }
      }

      function syncForm(payload) {
        const config = payload?.camera_config || {}
        Object.keys(cameraForm).forEach((key) => {
          if (Object.prototype.hasOwnProperty.call(config, key)) {
            cameraForm[key] = config[key]
          }
        })
        if (!cameraForm.public_host && payload?.endpoints?.public_host) {
          cameraForm.public_host = payload.endpoints.public_host
        }
      }

      async function callBridge(action, extra = {}, expectedActions = []) {
        const requestId = `${action}-${Date.now()}-${Math.random().toString(16).slice(2, 8)}`
        const response = await postJson(`${apiBase}/plugin-rpc`, {
          plugin: pluginName,
          data: {
            action,
            request_id: requestId,
            target_node: selectedNode.value,
            ...extra,
          },
          timeout_ms: 6500,
          expected_actions: expectedActions,
        })

        if (!response.success) {
          throw new Error(response.error || '请求失败')
        }
        if (!response.matched) {
          throw new Error('插件响应超时')
        }
        return response.response || {}
      }

      async function refreshClientStates() {
        const [clientList, locals] = await Promise.all([
          listClients(apiBase),
          listLocalPlugins(apiBase),
        ])

        clients.value = clientList
        if (!selectedNode.value && onlineClients.value.length) {
          selectedNode.value = onlineClients.value[0].node_id
        }
        if (selectedNode.value && !onlineClients.value.some((item) => item.node_id === selectedNode.value)) {
          selectedNode.value = onlineClients.value[0]?.node_id || ''
        }

        localState.value = locals.find((item) => item.name === pluginName)?.state || '未安装'
        if (selectedNode.value) {
          const remotes = await listRemotePlugins(apiBase, selectedNode.value)
          remoteState.value = remotes.find((item) => item.name === pluginName)?.state || '未安装'
        } else {
          remoteState.value = '无在线节点'
        }
      }

      async function refreshStatus(showLoading = false) {
        if (showLoading) {
          loading.value = true
        }
        try {
          await refreshClientStates()
          if (!selectedNode.value) {
            status.value = null
            stopPreview()
            return
          }
          const payload = await callBridge('status', {}, ['status_result', 'stream_error', 'error'])
          status.value = payload
          syncForm(payload)
          if (payload.action === 'stream_error') {
            setMessage(payload.message || '视频服务异常', true)
          }
        } catch (err) {
          setMessage(`刷新状态失败：${err.message || err}`, true)
        } finally {
          if (showLoading) {
            loading.value = false
          }
        }
      }

      async function restartPreviewIfNeeded() {
        if (service.value.active) {
          await connectPreview()
        } else {
          stopPreview()
        }
      }

      async function applyConfig() {
        if (!selectedNode.value) {
          setMessage('没有在线的 HoneyTea 节点，无法下发摄像头参数', true)
          return
        }
        saving.value = true
        try {
          const payload = await callBridge(
            'set_config',
            {
              camera_config: {
                public_host: String(cameraForm.public_host || '').trim(),
                camera_id: toNumber(cameraForm.camera_id, 0),
                width: toNumber(cameraForm.width, 1296),
                height: toNumber(cameraForm.height, 972),
                fps: toNumber(cameraForm.fps, 30),
                bitrate: toNumber(cameraForm.bitrate, 5000000),
                idr_period: toNumber(cameraForm.idr_period, 30),
                brightness: toNumber(cameraForm.brightness, 0),
                contrast: toNumber(cameraForm.contrast, 1),
                saturation: toNumber(cameraForm.saturation, 1),
                sharpness: toNumber(cameraForm.sharpness, 1),
                exposure: cameraForm.exposure,
                awb: cameraForm.awb,
                denoise: cameraForm.denoise,
                shutter: toNumber(cameraForm.shutter, 0),
                gain: toNumber(cameraForm.gain, 0),
                ev: toNumber(cameraForm.ev, 0),
                hflip: Boolean(cameraForm.hflip),
                vflip: Boolean(cameraForm.vflip),
                stream_path: String(cameraForm.stream_path || 'rpi-camera').trim() || 'rpi-camera',
                rtsp_port: toNumber(cameraForm.rtsp_port, 8554),
                webrtc_port: toNumber(cameraForm.webrtc_port, 8889),
                webrtc_udp_port: toNumber(cameraForm.webrtc_udp_port, 8189),
                yolo_port: toNumber(cameraForm.yolo_port, 20000),
              },
            },
            ['config_result', 'stream_error', 'error']
          )
          status.value = { ...(status.value || {}), ...payload }
          syncForm(status.value)
          setMessage('摄像头参数已下发到 HoneyTea，并按需重启本地视频服务')
          await refreshStatus()
          await restartPreviewIfNeeded()
        } catch (err) {
          setMessage(`保存配置失败：${err.message || err}`, true)
        } finally {
          saving.value = false
        }
      }

      async function triggerAction(action, successText) {
        if (!selectedNode.value) {
          setMessage('没有在线的 HoneyTea 节点', true)
          return
        }
        actionBusy.value = true
        try {
          const payload = await callBridge(action, {}, ['status_result', 'stream_error', 'error'])
          status.value = payload
          syncForm(payload)
          setMessage(successText)
          await refreshStatus()
          await restartPreviewIfNeeded()
        } catch (err) {
          setMessage(`${successText}失败：${err.message || err}`, true)
        } finally {
          actionBusy.value = false
        }
      }

      async function runSimpleAction(fn, successText) {
        try {
          await fn()
          if (successText) {
            setMessage(successText)
          }
        } catch (err) {
          setMessage(`${successText || '操作'}失败：${err.message || err}`, true)
        }
      }

      async function connectPreview() {
        recordError.value = ''
        stopPreview()
        if (!previewUrl.value || !service.value.active) {
          return
        }
        try {
          previewReader = new SimpleWhepReader({
            url: previewUrl.value,
            onTrack: (event) => {
              previewStream = event.streams[0]
              if (videoEl.value) {
                videoEl.value.srcObject = previewStream
              }
            },
            onError: (err) => {
              setMessage(`WebRTC 预览异常：${err.message || err}`, true)
            },
          })
          await previewReader.start()
        } catch (err) {
          stopPreview()
          setMessage(`连接 WebRTC 预览失败：${err.message || err}`, true)
        }
      }

      function stopPreview() {
        if (previewReader) {
          previewReader.close()
          previewReader = null
        }
        if (videoEl.value) {
          videoEl.value.pause?.()
          videoEl.value.srcObject = null
        }
        previewStream = null
        stopRecording()
      }

      function downloadBlob(blob, fileName) {
        const url = URL.createObjectURL(blob)
        const anchor = document.createElement('a')
        anchor.href = url
        anchor.download = fileName
        anchor.click()
        URL.revokeObjectURL(url)
      }

      function takeScreenshot() {
        if (!videoEl.value || !previewStream) {
          setMessage('当前没有可截图的视频流', true)
          return
        }
        const canvas = document.createElement('canvas')
        canvas.width = videoEl.value.videoWidth || 1280
        canvas.height = videoEl.value.videoHeight || 720
        const context = canvas.getContext('2d')
        context.drawImage(videoEl.value, 0, 0, canvas.width, canvas.height)
        canvas.toBlob((blob) => {
          if (!blob) {
            setMessage('截图失败，浏览器未返回图像数据', true)
            return
          }
          downloadBlob(blob, `cam-lan-stream-${Date.now()}.png`)
        }, 'image/png')
      }

      function startRecording() {
        if (!previewStream) {
          recordError.value = '当前没有可录制的视频流'
          return
        }
        try {
          recordedChunks.value = []
          const mimeType = [
            'video/webm;codecs=vp9,opus',
            'video/webm;codecs=vp8,opus',
            'video/webm',
          ].find((candidate) => !window.MediaRecorder?.isTypeSupported || MediaRecorder.isTypeSupported(candidate)) || ''

          recorder.value = mimeType ? new MediaRecorder(previewStream, { mimeType }) : new MediaRecorder(previewStream)
          recorder.value.ondataavailable = (event) => {
            if (event.data && event.data.size > 0) {
              recordedChunks.value.push(event.data)
            }
          }
          recorder.value.onstop = () => {
            const blob = new Blob(recordedChunks.value, { type: recorder.value?.mimeType || 'video/webm' })
            if (blob.size > 0) {
              downloadBlob(blob, `cam-lan-stream-${Date.now()}.webm`)
            }
            recordedChunks.value = []
            recorder.value = null
            isRecording.value = false
          }
          recorder.value.start(1000)
          isRecording.value = true
          recordError.value = ''
        } catch (err) {
          recordError.value = `开始录制失败：${err.message || err}`
        }
      }

      function stopRecording() {
        if (recorder.value && recorder.value.state !== 'inactive') {
          recorder.value.stop()
        }
        recorder.value = null
        isRecording.value = false
      }

      watch(selectedNode, async () => {
        await refreshStatus(true)
      })

      watch(
        () => [previewUrl.value, service.value.active],
        async () => {
          await restartPreviewIfNeeded()
        }
      )

      onMounted(async () => {
        await refreshStatus(true)
        statusTimer = window.setInterval(() => {
          refreshStatus().catch(() => {})
        }, 5000)
        refreshTimer = window.setInterval(() => {
          if (service.value.active && videoEl.value && !videoEl.value.srcObject) {
            connectPreview().catch(() => {})
          }
        }, 7000)
      })

      onUnmounted(() => {
        if (statusTimer) {
          window.clearInterval(statusTimer)
        }
        if (refreshTimer) {
          window.clearInterval(refreshTimer)
        }
        stopPreview()
      })

      return {
        loading,
        saving,
        actionBusy,
        error,
        info,
        clients,
        localState,
        remoteState,
        selectedNode,
        status,
        service,
        endpoints,
        system,
        cameraForm,
        videoEl,
        isRecording,
        recordError,
        previewPageUrl,
        previewUrl,
        apiDocsText,
        jsExample,
        pythonExample,
        ffplayExample,
        refreshStatus,
        applyConfig,
        startStream: () => triggerAction('start_stream', '视频服务已启动'),
        stopStream: () => triggerAction('stop_stream', '视频服务已停止'),
        restartStream: () => triggerAction('restart_stream', '视频服务已重启'),
        takeScreenshot,
        startRecording,
        stopRecording,
        startLocalPlugin: () => runSimpleAction(async () => {
          await postPlain(`${apiBase}/plugins/${pluginName}/start`)
          await refreshStatus(true)
        }, 'LemonTea 插件已启动'),
        stopLocalPlugin: () => runSimpleAction(async () => {
          await postPlain(`${apiBase}/plugins/${pluginName}/stop`)
          await refreshStatus(true)
        }, 'LemonTea 插件已停止'),
        startRemotePlugin: () => runSimpleAction(async () => {
          if (!selectedNode.value) throw new Error('没有在线节点')
          await postPlain(`${apiBase}/clients/${encodeURIComponent(selectedNode.value)}/plugins/${pluginName}/start`)
          await refreshStatus(true)
        }, 'HoneyTea 插件已启动'),
        stopRemotePlugin: () => runSimpleAction(async () => {
          if (!selectedNode.value) throw new Error('没有在线节点')
          await postPlain(`${apiBase}/clients/${encodeURIComponent(selectedNode.value)}/plugins/${pluginName}/stop`)
          await refreshStatus(true)
        }, 'HoneyTea 插件已停止'),
      }
    },
    template: `
      <section class="tea-cam-root">
        <div class="tea-cam-shell">
          <section class="tea-cam-hero">
            <div>
              <h2 class="tea-cam-title">树莓派摄像头双路串流控制台</h2>
              <p class="tea-cam-subtitle">
                HoneyTea 端通过 MediaMTX 直接驱动树莓派摄像头，将同一份 H.264 视频同时输出给 OrangeTea 的 WebRTC 低延迟预览和未来 YOLO 插件使用的裸 H.264 over TCP 接口。
                LemonTea 负责参数下发与状态回收，因此页面上的所有控制都走现有 TeaFamily 插件总线，不需要额外开放控制端口。
              </p>
              <div class="tea-cam-pill-row">
                <span class="tea-cam-pill">MediaMTX WebRTC</span>
                <span class="tea-cam-pill">Raw H.264 over TCP</span>
                <span class="tea-cam-pill">截图与录制在前端完成</span>
                <span class="tea-cam-pill">树莓派 5 / ov5647 已留接口</span>
              </div>
            </div>

            <div class="tea-cam-hero-side">
              <div class="tea-cam-status-grid">
                <div class="tea-cam-status-cell">
                  <span class="tea-cam-status-label">LemonTea 端状态</span>
                  <span class="tea-cam-status-value">{{ localState }}</span>
                </div>
                <div class="tea-cam-status-cell">
                  <span class="tea-cam-status-label">HoneyTea 端状态</span>
                  <span class="tea-cam-status-value">{{ remoteState }}</span>
                </div>
                <div class="tea-cam-status-cell">
                  <span class="tea-cam-status-label">服务状态</span>
                  <span class="tea-cam-status-value">{{ status?.state || '未知' }}</span>
                </div>
                <div class="tea-cam-status-cell">
                  <span class="tea-cam-status-label">YOLO 客户端</span>
                  <span class="tea-cam-status-value">{{ service.yolo_clients ?? 0 }}</span>
                </div>
              </div>

              <div class="tea-cam-banner" v-if="info">{{ info }}</div>
              <div class="tea-cam-banner error" v-if="error">{{ error }}</div>
            </div>
          </section>

          <section class="tea-cam-grid">
            <aside class="tea-cam-card tea-cam-stack">
              <h3>HoneyTea 节点与插件控制</h3>

              <div class="tea-cam-field">
                <label>在线 HoneyTea 节点</label>
                <select v-model="selectedNode">
                  <option value="">请选择在线节点</option>
                  <option v-for="item in clients.filter((entry) => entry.connected)" :key="item.node_id" :value="item.node_id">
                    {{ item.node_id }} ({{ item.address }}:{{ item.port }})
                  </option>
                </select>
              </div>

              <div class="tea-cam-button-row">
                <button class="tea-cam-btn ghost" @click="refreshStatus(true)" :disabled="loading">刷新状态</button>
                <button class="tea-cam-btn secondary" @click="startLocalPlugin">启动 LemonTea 插件</button>
                <button class="tea-cam-btn ghost" @click="stopLocalPlugin">停止 LemonTea 插件</button>
                <button class="tea-cam-btn secondary" @click="startRemotePlugin" :disabled="!selectedNode">启动 HoneyTea 插件</button>
                <button class="tea-cam-btn ghost" @click="stopRemotePlugin" :disabled="!selectedNode">停止 HoneyTea 插件</button>
              </div>

              <div class="tea-cam-button-row">
                <button class="tea-cam-btn" @click="startStream" :disabled="actionBusy || !selectedNode">启动视频服务</button>
                <button class="tea-cam-btn secondary" @click="restartStream" :disabled="actionBusy || !selectedNode">重启视频服务</button>
                <button class="tea-cam-btn danger" @click="stopStream" :disabled="actionBusy || !selectedNode">停止视频服务</button>
              </div>

              <h3>摄像头参数</h3>

              <div class="tea-cam-row">
                <div class="tea-cam-field">
                  <label>public_host</label>
                  <input v-model="cameraForm.public_host" placeholder="例如 192.168.1.88" />
                </div>
                <div class="tea-cam-field">
                  <label>camera_id</label>
                  <input v-model="cameraForm.camera_id" type="number" min="0" step="1" />
                </div>
              </div>

              <div class="tea-cam-row triple">
                <div class="tea-cam-field">
                  <label>宽度</label>
                  <input v-model="cameraForm.width" type="number" min="320" step="2" />
                </div>
                <div class="tea-cam-field">
                  <label>高度</label>
                  <input v-model="cameraForm.height" type="number" min="240" step="2" />
                </div>
                <div class="tea-cam-field">
                  <label>FPS</label>
                  <input v-model="cameraForm.fps" type="number" min="1" max="120" step="1" />
                </div>
              </div>

              <div class="tea-cam-row triple">
                <div class="tea-cam-field">
                  <label>bitrate</label>
                  <input v-model="cameraForm.bitrate" type="number" min="1000000" step="500000" />
                </div>
                <div class="tea-cam-field">
                  <label>IDR period</label>
                  <input v-model="cameraForm.idr_period" type="number" min="1" step="1" />
                </div>
                <div class="tea-cam-field">
                  <label>YOLO 端口</label>
                  <input v-model="cameraForm.yolo_port" type="number" min="1" max="65535" step="1" />
                </div>
              </div>

              <div class="tea-cam-row triple">
                <div class="tea-cam-field">
                  <label>亮度</label>
                  <input v-model="cameraForm.brightness" type="number" min="-1" max="1" step="0.1" />
                </div>
                <div class="tea-cam-field">
                  <label>对比度</label>
                  <input v-model="cameraForm.contrast" type="number" min="0" max="16" step="0.1" />
                </div>
                <div class="tea-cam-field">
                  <label>饱和度</label>
                  <input v-model="cameraForm.saturation" type="number" min="0" max="16" step="0.1" />
                </div>
              </div>

              <div class="tea-cam-row triple">
                <div class="tea-cam-field">
                  <label>锐度</label>
                  <input v-model="cameraForm.sharpness" type="number" min="0" max="16" step="0.1" />
                </div>
                <div class="tea-cam-field">
                  <label>曝光</label>
                  <select v-model="cameraForm.exposure">
                    <option value="normal">normal</option>
                    <option value="short">short</option>
                    <option value="long">long</option>
                    <option value="custom">custom</option>
                  </select>
                </div>
                <div class="tea-cam-field">
                  <label>AWB</label>
                  <select v-model="cameraForm.awb">
                    <option value="auto">auto</option>
                    <option value="incandescent">incandescent</option>
                    <option value="tungsten">tungsten</option>
                    <option value="fluorescent">fluorescent</option>
                    <option value="indoor">indoor</option>
                    <option value="daylight">daylight</option>
                    <option value="cloudy">cloudy</option>
                    <option value="custom">custom</option>
                  </select>
                </div>
              </div>

              <div class="tea-cam-row triple">
                <div class="tea-cam-field">
                  <label>降噪</label>
                  <select v-model="cameraForm.denoise">
                    <option value="off">off</option>
                    <option value="cdn_off">cdn_off</option>
                    <option value="cdn_fast">cdn_fast</option>
                    <option value="cdn_hq">cdn_hq</option>
                  </select>
                </div>
                <div class="tea-cam-field">
                  <label>shutter(us)</label>
                  <input v-model="cameraForm.shutter" type="number" min="0" step="100" />
                </div>
                <div class="tea-cam-field">
                  <label>gain</label>
                  <input v-model="cameraForm.gain" type="number" min="0" step="0.1" />
                </div>
              </div>

              <div class="tea-cam-row triple">
                <div class="tea-cam-field">
                  <label>EV</label>
                  <input v-model="cameraForm.ev" type="number" min="-10" max="10" step="0.1" />
                </div>
                <div class="tea-cam-field">
                  <label>HFlip</label>
                  <select v-model="cameraForm.hflip">
                    <option :value="false">false</option>
                    <option :value="true">true</option>
                  </select>
                </div>
                <div class="tea-cam-field">
                  <label>VFlip</label>
                  <select v-model="cameraForm.vflip">
                    <option :value="false">false</option>
                    <option :value="true">true</option>
                  </select>
                </div>
              </div>

              <div class="tea-cam-row triple">
                <div class="tea-cam-field">
                  <label>stream_path</label>
                  <input v-model="cameraForm.stream_path" />
                </div>
                <div class="tea-cam-field">
                  <label>RTSP 端口</label>
                  <input v-model="cameraForm.rtsp_port" type="number" min="1" max="65535" step="1" />
                </div>
                <div class="tea-cam-field">
                  <label>WebRTC 端口</label>
                  <input v-model="cameraForm.webrtc_port" type="number" min="1" max="65535" step="1" />
                </div>
              </div>

              <div class="tea-cam-row">
                <div class="tea-cam-field">
                  <label>WebRTC UDP 端口</label>
                  <input v-model="cameraForm.webrtc_udp_port" type="number" min="1" max="65535" step="1" />
                </div>
                <div class="tea-cam-field">
                  <label>当前端点摘要</label>
                  <textarea readonly :value="apiDocsText"></textarea>
                </div>
              </div>

              <div class="tea-cam-button-row">
                <button class="tea-cam-btn" @click="applyConfig" :disabled="saving || !selectedNode">保存并热重载</button>
              </div>
            </aside>

            <section class="tea-cam-preview-shell">
              <div class="tea-cam-card">
                <h3>WebRTC 实时预览</h3>
                <div class="tea-cam-preview">
                  <video ref="videoEl" class="tea-cam-video" muted autoplay playsinline controls></video>
                  <div v-if="!service.active" class="tea-cam-empty">
                    当前视频服务未运行。请先在左侧启动 HoneyTea 插件并启动视频服务。
                  </div>
                  <div class="tea-cam-overlay">
                    <span class="tea-cam-overlay-chip">
                      <span class="tea-cam-dot" :class="{ live: service.active }"></span>
                      {{ service.active ? 'WebRTC 在线' : '等待启动' }}
                    </span>
                    <span class="tea-cam-overlay-chip">
                      {{ cameraForm.width }} × {{ cameraForm.height }} @ {{ cameraForm.fps }} fps
                    </span>
                  </div>
                </div>

                <div class="tea-cam-button-row" style="margin-top:12px">
                  <button class="tea-cam-btn secondary" @click="takeScreenshot" :disabled="!service.active">保存截图</button>
                  <button class="tea-cam-btn" @click="startRecording" :disabled="!service.active || isRecording">开始录制</button>
                  <button class="tea-cam-btn danger" @click="stopRecording" :disabled="!isRecording">停止录制</button>
                  <a v-if="previewPageUrl" class="tea-cam-btn ghost" :href="previewPageUrl" target="_blank" rel="noopener noreferrer">打开 MediaMTX 页面</a>
                </div>
                <div class="tea-cam-banner error" v-if="recordError" style="margin-top:10px">{{ recordError }}</div>
              </div>

              <div class="tea-cam-card">
                <h3>输出端点与运行信息</h3>
                <div class="tea-cam-kv-grid">
                  <div class="tea-cam-kv-item">
                    <strong>WebRTC WHEP</strong>
                    <a v-if="previewUrl" :href="previewUrl" target="_blank" rel="noopener noreferrer">{{ previewUrl }}</a>
                    <span v-else>等待 HoneyTea 上报</span>
                  </div>
                  <div class="tea-cam-kv-item">
                    <strong>MediaMTX 页面</strong>
                    <a v-if="previewPageUrl" :href="previewPageUrl" target="_blank" rel="noopener noreferrer">{{ previewPageUrl }}</a>
                    <span v-else>等待 HoneyTea 上报</span>
                  </div>
                  <div class="tea-cam-kv-item">
                    <strong>RTSP</strong>
                    <span>{{ endpoints.rtsp_url || '-' }}</span>
                  </div>
                  <div class="tea-cam-kv-item">
                    <strong>YOLO Raw H.264</strong>
                    <span>{{ endpoints.yolo_tcp_url || '-' }}</span>
                  </div>
                  <div class="tea-cam-kv-item">
                    <strong>HoneyTea Host</strong>
                    <span>{{ endpoints.public_host || cameraForm.public_host || '-' }}</span>
                  </div>
                  <div class="tea-cam-kv-item">
                    <strong>系统信息</strong>
                    <span>{{ system.hostname || '-' }} / {{ system.arch || '-' }} / {{ system.os || '-' }}</span>
                  </div>
                </div>
              </div>

              <div class="tea-cam-card tea-cam-stack">
                <h3>YOLO / 第三方接入说明</h3>
                <ul class="tea-cam-api-list">
                  <li><code>tcp://HoneyTea-IP:YOLO_PORT</code> 输出的是 Annex-B 裸 H.264 字节流，后续 YOLO 插件可以直接消费。</li>
                  <li><code>rtsp://HoneyTea-IP:RTSP_PORT/STREAM_PATH</code> 是 MediaMTX 的标准 RTSP 路径，可用于排障或调试播放器。</li>
                  <li><code>http://HoneyTea-IP:WEBRTC_PORT/STREAM_PATH/whep</code> 是 OrangeTea 当前使用的低延迟 WebRTC 接口。</li>
                  <li>参数修改后，HoneyTea 会重写 MediaMTX 配置并重启本地视频服务，前端会自动重新拉流。</li>
                </ul>

                <div class="tea-cam-code-grid">
                  <article class="tea-cam-code-card">
                    <h4>JavaScript / WHEP</h4>
                    <pre>{{ jsExample }}</pre>
                  </article>
                  <article class="tea-cam-code-card">
                    <h4>Python / YOLO 裸流</h4>
                    <pre>{{ pythonExample }}</pre>
                  </article>
                  <article class="tea-cam-code-card">
                    <h4>ffplay 调试</h4>
                    <pre>{{ ffplayExample }}</pre>
                  </article>
                </div>
              </div>
            </section>
          </section>
        </div>
      </section>
    `,
  })

  app.mount(container)

  return () => {
    app.unmount()
  }
}