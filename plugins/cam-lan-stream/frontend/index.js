import {
  createApp,
  computed,
  onMounted,
  onUnmounted,
  ref,
  watch,
} from 'https://unpkg.com/vue@3/dist/vue.esm-browser.js'

const STYLE_ID = 'tea-cam-lan-stream-style'

function ensureStyle() {
  if (document.getElementById(STYLE_ID)) {
    return
  }
  const style = document.createElement('style')
  style.id = STYLE_ID
  style.textContent = `
    .cam-stream-root { color: #1f2f41; font-family: "Noto Sans SC", "PingFang SC", sans-serif; }
    .cam-stream-head { display: flex; flex-wrap: wrap; gap: 10px; justify-content: space-between; align-items: flex-start; margin-bottom: 12px; }
    .cam-stream-head h2 { margin: 0; font-size: 22px; }
    .cam-stream-sub { margin: 4px 0 0; color: #58708a; }
    .cam-stream-btn { border: 0; border-radius: 10px; padding: 8px 12px; cursor: pointer; font-weight: 600; color: #fff; background: #2d7dd2; }
    .cam-stream-btn.secondary { background: #5e738e; }
    .cam-stream-grid { display: grid; grid-template-columns: 320px minmax(0, 1fr); gap: 12px; }
    .cam-stream-card { border: 1px solid #dbe8f6; border-radius: 14px; padding: 12px; background: linear-gradient(135deg, #ffffff, #f7fbff); box-shadow: 0 8px 22px rgba(26, 70, 112, 0.08); }
    .cam-stream-card h3 { margin: 0 0 10px; font-size: 15px; color: #2f435a; }
    .cam-stream-list { max-height: 420px; overflow: auto; display: flex; flex-direction: column; gap: 8px; }
    .cam-stream-item { border: 1px solid #d0e0f2; border-radius: 10px; padding: 8px 10px; cursor: pointer; background: #fff; }
    .cam-stream-item.active { border-color: #2d7dd2; background: #edf5ff; }
    .cam-stream-item strong { display: block; color: #1f3450; }
    .cam-stream-item span { color: #60788f; font-size: 12px; }
    .cam-stream-kv { display: grid; gap: 8px; margin-bottom: 10px; }
    .cam-stream-kv div { display: flex; gap: 8px; font-size: 13px; color: #2f4660; }
    .cam-stream-kv b { color: #6b8299; min-width: 88px; }
    .cam-stream-preview { border: 1px solid #d0e1f3; border-radius: 12px; background: #0f1f31; min-height: 300px; display: flex; align-items: center; justify-content: center; overflow: hidden; }
    .cam-stream-preview img { width: 100%; display: block; object-fit: contain; }
    .cam-stream-empty { color: #a2b4c8; padding: 20px; text-align: center; }
    .cam-stream-mobile-link { word-break: break-all; color: #1f66ad; }
    .cam-stream-err { border: 1px solid #f2c8c8; background: #fff1f1; color: #8c3030; border-radius: 10px; padding: 8px 10px; margin-bottom: 10px; }
    @media (max-width: 980px) {
      .cam-stream-grid { grid-template-columns: 1fr; }
    }
  `
  document.head.appendChild(style)
}

function normalizeApiBase(apiBase) {
  if (!apiBase) {
    return '/api'
  }
  return String(apiBase).replace(/\/+$/, '') || '/api'
}

function inferOriginFromApiBase(apiBase) {
  try {
    const u = new URL(apiBase)
    return `${u.protocol}//${u.hostname}`
  } catch {
    return `${window.location.protocol}//${window.location.hostname}`
  }
}

function inferServiceOrigin(apiBase, statusServer) {
  if (statusServer?.public_origin) {
    return statusServer.public_origin
  }
  const origin = inferOriginFromApiBase(apiBase)
  const port = statusServer?.port || 19731
  return `${origin}:${port}`
}

async function callPluginRpc(apiBase, pluginName, data, expectedActions = []) {
  const resp = await fetch(`${apiBase}/plugin-rpc`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      plugin: pluginName,
      data,
      timeout_ms: 3500,
      expected_actions: expectedActions,
    }),
  })

  const payload = await resp.json().catch(() => ({}))
  if (!resp.ok) {
    throw new Error(payload.error || `${resp.status} ${resp.statusText}`)
  }
  return payload
}

export function mount(container, ctx = {}) {
  ensureStyle()

  const pluginName = ctx.pluginName || 'cam-lan-stream'
  const apiBase = normalizeApiBase(ctx.apiBase)

  const app = createApp({
    setup() {
      const loading = ref(false)
      const error = ref('')
      const serverInfo = ref(null)
      const devices = ref([])
      const selectedDevice = ref('')
      const selectedCamera = ref('')
      const frameTick = ref(Date.now())

      let pollTimer = null
      let frameTimer = null

      const serviceOrigin = computed(() => inferServiceOrigin(apiBase, serverInfo.value))
      const mobilePageUrl = computed(() => {
        if (serverInfo.value?.mobile_page_url) {
          return serverInfo.value.mobile_page_url
        }
        return `${serviceOrigin.value}/`
      })

      const selectedDeviceObj = computed(() =>
        devices.value.find((item) => item.device_id === selectedDevice.value) || null
      )

      const selectedCameraObj = computed(() => {
        const cams = selectedDeviceObj.value?.cameras || []
        return cams.find((item) => item.camera_id === selectedCamera.value) || null
      })

      const frameUrl = computed(() => {
        if (!selectedDevice.value || !selectedCamera.value) {
          return ''
        }
        return `${serviceOrigin.value}/api/frame/${encodeURIComponent(selectedDevice.value)}/${encodeURIComponent(selectedCamera.value)}?t=${frameTick.value}`
      })

      function ensureSelection() {
        if (!devices.value.length) {
          selectedDevice.value = ''
          selectedCamera.value = ''
          return
        }

        if (!devices.value.some((item) => item.device_id === selectedDevice.value)) {
          selectedDevice.value = devices.value[0].device_id
        }

        const cameras = selectedDeviceObj.value?.cameras || []
        if (!cameras.some((item) => item.camera_id === selectedCamera.value)) {
          selectedCamera.value = cameras[0]?.camera_id || ''
        }
      }

      async function fetchStatus() {
        const rpc = await callPluginRpc(apiBase, pluginName, { action: 'status' }, ['status_result', 'error'])
        const response = rpc.response || {}
        serverInfo.value = response.server || serverInfo.value
      }

      async function fetchDevices() {
        const resp = await fetch(`${serviceOrigin.value}/api/devices`)
        const payload = await resp.json().catch(() => ({}))
        if (!resp.ok) {
          throw new Error(payload.error || `${resp.status} ${resp.statusText}`)
        }
        devices.value = Array.isArray(payload.devices) ? payload.devices : []
        ensureSelection()
      }

      async function refreshAll() {
        loading.value = true
        error.value = ''
        try {
          await fetchStatus()
          await fetchDevices()
        } catch (e) {
          error.value = `刷新失败：${e.message || e}`
        } finally {
          loading.value = false
        }
      }

      function setDevice(deviceId) {
        selectedDevice.value = deviceId
        const cams = selectedDeviceObj.value?.cameras || []
        selectedCamera.value = cams[0]?.camera_id || ''
      }

      function setCamera(cameraId) {
        selectedCamera.value = cameraId
      }

      function startTimers() {
        if (!pollTimer) {
          pollTimer = window.setInterval(() => {
            refreshAll().catch(() => {})
          }, 3000)
        }
        if (!frameTimer) {
          frameTimer = window.setInterval(() => {
            if (selectedDevice.value && selectedCamera.value) {
              frameTick.value = Date.now()
            }
          }, 230)
        }
      }

      function stopTimers() {
        if (pollTimer) {
          clearInterval(pollTimer)
          pollTimer = null
        }
        if (frameTimer) {
          clearInterval(frameTimer)
          frameTimer = null
        }
      }

      watch(
        () => selectedDevice.value,
        () => {
          ensureSelection()
        }
      )

      onMounted(async () => {
        await refreshAll()
        startTimers()
      })

      onUnmounted(() => {
        stopTimers()
      })

      return {
        loading,
        error,
        serverInfo,
        devices,
        serviceOrigin,
        mobilePageUrl,
        selectedDevice,
        selectedCamera,
        selectedDeviceObj,
        selectedCameraObj,
        frameUrl,
        refreshAll,
        setDevice,
        setCamera,
      }
    },

    template: `
      <section class="cam-stream-root">
        <header class="cam-stream-head">
          <div>
            <h2>局域网摄像头串流</h2>
            <p class="cam-stream-sub">移动端访问采集页后，选择摄像头即可实时上报画面到 LemonTea。</p>
          </div>
          <div>
            <button class="cam-stream-btn" :disabled="loading" @click="refreshAll">刷新状态</button>
          </div>
        </header>

        <div v-if="error" class="cam-stream-err">{{ error }}</div>

        <div class="cam-stream-card" style="margin-bottom: 12px;">
          <div class="cam-stream-kv">
            <div><b>插件服务地址</b><span>{{ serviceOrigin }}</span></div>
            <div><b>移动端采集页</b><a class="cam-stream-mobile-link" :href="mobilePageUrl" target="_blank">{{ mobilePageUrl }}</a></div>
            <div><b>监听端口</b><span>{{ serverInfo?.port || '-' }}</span></div>
          </div>
        </div>

        <div class="cam-stream-grid">
          <aside class="cam-stream-card">
            <h3>在线设备</h3>
            <div class="cam-stream-list">
              <button
                v-for="dev in devices"
                :key="dev.device_id"
                type="button"
                class="cam-stream-item"
                :class="{ active: dev.device_id === selectedDevice }"
                @click="setDevice(dev.device_id)"
              >
                <strong>{{ dev.device_name || dev.device_id }}</strong>
                <span>{{ dev.device_id }} · {{ dev.remote_ip || '-' }}</span>
              </button>
              <div v-if="devices.length === 0" class="cam-stream-empty">暂无设备上线。请先用手机访问上方采集页。</div>
            </div>
          </aside>

          <section class="cam-stream-card">
            <h3>摄像头与实时画面</h3>
            <div class="cam-stream-list" style="max-height: 150px; margin-bottom: 10px;">
              <button
                v-for="cam in (selectedDeviceObj?.cameras || [])"
                :key="cam.camera_id"
                type="button"
                class="cam-stream-item"
                :class="{ active: cam.camera_id === selectedCamera }"
                @click="setCamera(cam.camera_id)"
              >
                <strong>{{ cam.camera_label || cam.camera_id }}</strong>
                <span>{{ cam.camera_id }}</span>
              </button>
              <div v-if="selectedDevice && (selectedDeviceObj?.cameras || []).length === 0" class="cam-stream-empty">该设备尚未上报摄像头列表。</div>
            </div>

            <div class="cam-stream-preview">
              <img v-if="frameUrl" :src="frameUrl" alt="实时串流画面" />
              <div v-else class="cam-stream-empty">请选择设备和摄像头后查看画面。</div>
            </div>
          </section>
        </div>
      </section>
    `,
  })

  app.mount(container)
  return () => app.unmount()
}
