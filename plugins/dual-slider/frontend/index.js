export async function mount(container, ctx) {
  const pluginName = ctx?.pluginName || 'dual-slider'
  const apiBase = (ctx?.apiBase || '/api').replace(/\/+$/, '')

  container.innerHTML = `
    <section class="ds-root">
      <header class="ds-header">
        <div>
          <h2>双滑块控制台</h2>
          <p>LemonTea 单端低延迟模式：前端快通道 + 插件 UDP 接口（供原生程序接入）。</p>
        </div>
        <div class="ds-actions">
          <button id="ds-refresh" class="ghost">刷新</button>
          <button id="ds-mobile-mode" class="accent">移动端模式</button>
          <button id="ds-reset" class="warn">全部归零</button>
        </div>
      </header>

      <section class="ds-card ds-top-card">
        <div class="ds-status-grid">
          <label>
            LemonTea 插件状态
            <span id="ds-local-state" class="ds-badge">-</span>
          </label>
          <label>
            传输模式
            <span id="ds-transport" class="ds-badge">HTTP 快通道 + UDP（可选）</span>
          </label>
          <label>
            当前值
            <span id="ds-values" class="ds-badge">A: 0% / B: 0%</span>
          </label>
        </div>
        <div class="ds-btn-row">
          <button id="ds-start-local">启动 LemonTea 插件</button>
          <button id="ds-stop-local" class="warn">停止 LemonTea 插件</button>
        </div>
      </section>

      <section class="ds-tab-row" role="tablist" aria-label="双滑块页面切换">
        <button id="ds-tab-control" class="tab active" role="tab" aria-selected="true">控制界面</button>
        <button id="ds-tab-display" class="tab" role="tab" aria-selected="false">展示界面</button>
      </section>

      <section id="ds-panel-control" class="ds-panel active" role="tabpanel">
        <div class="ds-main-grid">
          <article class="ds-card ds-slider-card">
            <h3>滑块控制</h3>
            <div class="slider-group">
              <label for="ds-slider-a">滑块 A <strong id="ds-a-value">0%</strong></label>
              <input id="ds-slider-a" type="range" min="-100" max="100" step="10" value="0" />
              <div class="quick-actions">
                <button data-target="a" data-mode="inc">A +10%</button>
                <button data-target="a" data-mode="dec" class="ghost">A -10%</button>
                <button data-target="a" data-mode="reset" class="warn">A 归零</button>
              </div>
            </div>

            <div class="slider-group">
              <label for="ds-slider-b">滑块 B <strong id="ds-b-value">0%</strong></label>
              <input id="ds-slider-b" type="range" min="-100" max="100" step="10" value="0" />
              <div class="quick-actions">
                <button data-target="b" data-mode="inc">B +10%</button>
                <button data-target="b" data-mode="dec" class="ghost">B -10%</button>
                <button data-target="b" data-mode="reset" class="warn">B 归零</button>
              </div>
            </div>
          </article>

          <article class="ds-card ds-key-card">
            <h3>桌面键盘映射（3 + 3）</h3>
            <p>点击输入框后按下任意按键进行绑定。默认：A(Q/A/Z), B(W/S/X)</p>
            <div class="key-grid">
              <label>滑块 A +10%
                <input id="key-a-inc" class="key-input" readonly />
              </label>
              <label>滑块 A -10%
                <input id="key-a-dec" class="key-input" readonly />
              </label>
              <label>滑块 A 归零
                <input id="key-a-reset" class="key-input" readonly />
              </label>
              <label>滑块 B +10%
                <input id="key-b-inc" class="key-input" readonly />
              </label>
              <label>滑块 B -10%
                <input id="key-b-dec" class="key-input" readonly />
              </label>
              <label>滑块 B 归零
                <input id="key-b-reset" class="key-input" readonly />
              </label>
            </div>
            <div class="quick-actions">
              <button id="ds-key-default" class="ghost">恢复默认键位</button>
            </div>
          </article>
        </div>
      </section>

      <section id="ds-panel-display" class="ds-panel" role="tabpanel">
        <article class="ds-card ds-display-card">
          <h3>只读展示界面</h3>
          <p>该页面仅展示当前数值。控制页与展示页会通过浏览器通道和后端同步实现低延迟刷新。</p>
          <div class="display-values">
            <div class="display-item">
              <span>滑块 A</span>
              <strong id="ds-display-a">0%</strong>
            </div>
            <div class="display-item">
              <span>滑块 B</span>
              <strong id="ds-display-b">0%</strong>
            </div>
          </div>
          <pre id="ds-api-hint" class="ds-api-hint"></pre>
        </article>
      </section>

      <section id="ds-mobile-layer" class="ds-mobile-layer" aria-hidden="true">
        <header class="mobile-head">
          <h3>移动端全屏控制</h3>
          <button id="ds-mobile-close" class="ghost">退出</button>
        </header>
        <div class="mobile-joy-row">
          <article class="mobile-joy-col">
            <h4>滑块 A</h4>
            <div id="ds-joy-pad-a" class="joy-pad">
              <div id="ds-joy-stick-a" class="joy-stick"></div>
            </div>
            <div id="ds-joy-value-a" class="joy-value">0%</div>
          </article>
          <article class="mobile-joy-col">
            <h4>滑块 B</h4>
            <div id="ds-joy-pad-b" class="joy-pad">
              <div id="ds-joy-stick-b" class="joy-stick"></div>
            </div>
            <div id="ds-joy-value-b" class="joy-value">0%</div>
          </article>
        </div>
      </section>
    </section>
  `

  const styleEl = document.createElement('style')
  styleEl.textContent = `
    .ds-root { font-family: Manrope, 'Noto Sans SC', sans-serif; color: #1f2a2f; }
    .ds-header { display:flex; justify-content:space-between; gap:14px; align-items:flex-start; margin-bottom:12px; }
    .ds-header h2 { margin:0; font-size:22px; }
    .ds-header p { margin:6px 0 0; color:#4a6570; }
    .ds-actions { display:flex; gap:8px; }
    .ds-card { border:1px solid #c7dbe3; border-radius:14px; padding:12px; background:linear-gradient(145deg,#f2fbff,#f7fff5); box-shadow:0 10px 20px rgba(26,94,113,.08); }
    .ds-top-card { margin-bottom:12px; }
    .ds-status-grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(200px, 1fr)); gap:10px; margin-bottom:10px; }
    .ds-status-grid label { display:flex; flex-direction:column; gap:6px; font-size:12px; color:#506a75; }
    .ds-badge { display:inline-flex; align-items:center; min-height:34px; padding:0 10px; border-radius:8px; background:#e7f2f7; color:#2f5768; font-size:13px; }
    .ds-btn-row { display:flex; flex-wrap:wrap; gap:8px; }
    .ds-root button { border:0; border-radius:9px; padding:9px 12px; cursor:pointer; background:#2e8f75; color:#fff; font-weight:600; min-width:124px; text-align:center; }
    .ds-root button.warn { background:#b84f4f; }
    .ds-root button.ghost { background:#e9f0f8; color:#2b4f72; }
    .ds-root button.accent { background:#2f5eb8; }
    .ds-tab-row { display:flex; gap:8px; margin:0 0 12px; }
    .ds-tab-row .tab { min-width:120px; background:#dce9ea; color:#2c4f53; }
    .ds-tab-row .tab.active { background:#2f5eb8; color:#fff; }
    .ds-panel { display:none; }
    .ds-panel.active { display:block; }
    .ds-main-grid { display:grid; grid-template-columns:minmax(420px, 1.6fr) minmax(320px, 1fr); gap:12px; }
    .slider-group + .slider-group { margin-top:16px; }
    .slider-group label { display:flex; align-items:center; justify-content:space-between; font-size:14px; margin-bottom:8px; color:#304f53; }
    .slider-group strong { font-size:20px; color:#2a6f7c; }
    input[type='range'] { width:100%; accent-color:#2f8f75; }
    .quick-actions { display:flex; flex-wrap:wrap; gap:8px; margin-top:10px; }
    .key-grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(160px, 1fr)); gap:10px; margin-top:12px; }
    .key-grid label { display:flex; flex-direction:column; gap:6px; font-size:12px; color:#4d6668; }
    .key-input { padding:8px 10px; border-radius:8px; border:1px solid #b6d2d8; background:#fff; font-weight:700; text-transform:uppercase; cursor:pointer; }
    .ds-display-card p { margin-top:6px; color:#577174; }
    .display-values { display:grid; grid-template-columns:repeat(auto-fit, minmax(180px, 1fr)); gap:12px; margin-top:10px; }
    .display-item { border-radius:12px; background:#11262b; color:#dff8f9; padding:16px; display:flex; flex-direction:column; gap:6px; }
    .display-item span { font-size:12px; opacity:.8; }
    .display-item strong { font-size:34px; line-height:1; }
    .ds-api-hint { margin-top:14px; border-radius:10px; background:#101917; color:#b9f2d2; padding:10px; overflow:auto; }

    .ds-mobile-layer {
      position: fixed;
      inset: 0;
      z-index: 9999;
      display: none;
      flex-direction: column;
      background:
        radial-gradient(circle at 15% 18%, rgba(150, 245, 255, 0.2), transparent 40%),
        radial-gradient(circle at 88% 84%, rgba(180, 255, 188, 0.2), transparent 36%),
        #0f1a21;
      color: #d9f1ff;
      padding: max(12px, env(safe-area-inset-top)) 14px max(14px, env(safe-area-inset-bottom));
      box-sizing: border-box;
    }
    .ds-mobile-layer.active { display: flex; }
    .mobile-head { display:flex; align-items:center; justify-content:space-between; gap:10px; margin-bottom:10px; }
    .mobile-head h3 { margin:0; font-size:18px; }
    .mobile-joy-row {
      flex: 1;
      min-height: 0;
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 14px;
    }
    .mobile-joy-col {
      border-radius: 14px;
      border: 1px solid rgba(177, 232, 255, 0.24);
      background: rgba(22, 44, 58, 0.66);
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 10px;
      padding: 10px;
    }
    .mobile-joy-col h4 { margin: 0; font-size: 16px; color: #b9e7ff; }
    .joy-pad {
      width: min(44vw, 280px);
      height: min(72vh, 500px);
      border-radius: 24px;
      border: 1px solid rgba(173, 219, 255, 0.35);
      background:
        linear-gradient(180deg, rgba(142, 219, 255, 0.16), rgba(111, 231, 169, 0.12)),
        #13242e;
      position: relative;
      overflow: hidden;
      touch-action: none;
      user-select: none;
    }
    .joy-pad::before,
    .joy-pad::after {
      content: '';
      position: absolute;
      left: 12%;
      right: 12%;
      height: 1px;
      background: rgba(176, 216, 238, 0.2);
    }
    .joy-pad::before { top: 28%; }
    .joy-pad::after { bottom: 28%; }
    .joy-stick {
      position: absolute;
      left: 50%;
      top: 50%;
      width: 68px;
      height: 68px;
      border-radius: 999px;
      transform: translate(-50%, -50%);
      background: radial-gradient(circle at 32% 28%, #e2f8ff, #7dd4ff 65%, #46a7e0);
      box-shadow: 0 0 0 6px rgba(126, 220, 255, 0.22), 0 12px 28px rgba(8, 22, 31, 0.48);
      pointer-events: none;
    }
    .joy-value {
      min-width: 96px;
      text-align: center;
      font-size: 30px;
      line-height: 1;
      font-weight: 800;
      color: #d8f6ff;
    }

    @media (max-width: 1050px) {
      .ds-main-grid { grid-template-columns:1fr; }
    }

    @media (max-width: 880px) {
      .ds-actions { flex-wrap: wrap; }
      .mobile-joy-row { grid-template-columns: 1fr 1fr; }
      .joy-pad { width: min(43vw, 240px); height: min(70vh, 420px); }
      .joy-stick { width: 64px; height: 64px; }
    }
  `
  container.prepend(styleEl)

  const elLocalState = container.querySelector('#ds-local-state')
  const elTransport = container.querySelector('#ds-transport')
  const elValues = container.querySelector('#ds-values')
  const elA = container.querySelector('#ds-slider-a')
  const elB = container.querySelector('#ds-slider-b')
  const elAValue = container.querySelector('#ds-a-value')
  const elBValue = container.querySelector('#ds-b-value')
  const elDisplayA = container.querySelector('#ds-display-a')
  const elDisplayB = container.querySelector('#ds-display-b')
  const elApiHint = container.querySelector('#ds-api-hint')
  const elTabControl = container.querySelector('#ds-tab-control')
  const elTabDisplay = container.querySelector('#ds-tab-display')
  const elPanelControl = container.querySelector('#ds-panel-control')
  const elPanelDisplay = container.querySelector('#ds-panel-display')
  const elMobileLayer = container.querySelector('#ds-mobile-layer')
  const elJoyPadA = container.querySelector('#ds-joy-pad-a')
  const elJoyPadB = container.querySelector('#ds-joy-pad-b')
  const elJoyStickA = container.querySelector('#ds-joy-stick-a')
  const elJoyStickB = container.querySelector('#ds-joy-stick-b')
  const elJoyValueA = container.querySelector('#ds-joy-value-a')
  const elJoyValueB = container.querySelector('#ds-joy-value-b')

  const keyInputs = {
    aInc: container.querySelector('#key-a-inc'),
    aDec: container.querySelector('#key-a-dec'),
    aReset: container.querySelector('#key-a-reset'),
    bInc: container.querySelector('#key-b-inc'),
    bDec: container.querySelector('#key-b-dec'),
    bReset: container.querySelector('#key-b-reset'),
  }

  let stop = false
  let statusTimer = null
  let valueTimer = null
  let transportTimer = null
  let activeKeyInput = null
  let flushQueued = false
  let flushSource = 'slider'
  let lastApplyTs = 0

  let values = { slider_a: 0, slider_b: 0 }
  let transportInfo = { udp_enabled: false, udp_host: '127.0.0.1', udp_port: 19731 }

  const defaultBindings = {
    aInc: 'q',
    aDec: 'a',
    aReset: 'z',
    bInc: 'w',
    bDec: 's',
    bReset: 'x',
  }
  const keyBindings = { ...defaultBindings }

  const channel = ('BroadcastChannel' in window)
    ? new BroadcastChannel('tea-dual-slider-values')
    : null

  function formatPercent(v) {
    return `${Number(v || 0).toFixed(0)}%`
  }

  function clamp(v) {
    return Math.max(-100, Math.min(100, Number(v || 0)))
  }

  function setJoystickVisual(stickEl, value) {
    const top = 50 - (clamp(value) / 100) * 38
    stickEl.style.top = `${top}%`
  }

  function updateValueUI() {
    const a = clamp(values.slider_a)
    const b = clamp(values.slider_b)

    elA.value = String(a)
    elB.value = String(b)

    elAValue.textContent = formatPercent(a)
    elBValue.textContent = formatPercent(b)
    elDisplayA.textContent = formatPercent(a)
    elDisplayB.textContent = formatPercent(b)
    elJoyValueA.textContent = formatPercent(a)
    elJoyValueB.textContent = formatPercent(b)
    elValues.textContent = `A: ${formatPercent(a)} / B: ${formatPercent(b)}`

    setJoystickVisual(elJoyStickA, a)
    setJoystickVisual(elJoyStickB, b)
  }

  function syncApiHint() {
    const base = apiBase.replace(/\/+$/, '')
    const udpHost = transportInfo.udp_host || '127.0.0.1'
    const udpPort = transportInfo.udp_port || 19731

    elApiHint.textContent = [
      'HTTP 实时值 API（OrangeTea/其他插件推荐）',
      `POST ${base}/plugin-rpc`,
      '{',
      '  "plugin": "dual-slider",',
      '  "data": { "action": "get_values" },',
      '  "timeout_ms": 1500,',
      '  "expected_actions": ["values"]',
      '}',
      '',
      'UDP API（原生程序可选，浏览器不能直接 UDP）：',
      `udp://${udpHost}:${udpPort}`,
      '请求示例: {"action":"get_values"}',
      '设置示例: {"action":"set_values","slider_a":20,"slider_b":-30}',
      '',
      '响应示例:',
      '{"action":"values","slider_a":20,"slider_b":-30,"updated_at_ms":123456789}',
    ].join('\n')
  }

  function setBindingsToInputs() {
    Object.entries(keyInputs).forEach(([k, input]) => {
      input.value = (keyBindings[k] || '').toUpperCase()
    })
  }

  function activateTab(which) {
    const control = which === 'control'
    elTabControl.classList.toggle('active', control)
    elTabDisplay.classList.toggle('active', !control)
    elTabControl.setAttribute('aria-selected', String(control))
    elTabDisplay.setAttribute('aria-selected', String(!control))
    elPanelControl.classList.toggle('active', control)
    elPanelDisplay.classList.toggle('active', !control)
  }

  function applyValues(nextA, nextB, source, updatedAt) {
    const ts = Number(updatedAt || Date.now())
    if (ts < lastApplyTs) {
      return
    }

    values.slider_a = clamp(nextA)
    values.slider_b = clamp(nextB)
    lastApplyTs = ts
    updateValueUI()

    if (source === 'local' && channel) {
      channel.postMessage({
        slider_a: values.slider_a,
        slider_b: values.slider_b,
        updated_at_ms: ts,
      })
    }
  }

  async function fetchJson(url, options) {
    const resp = await fetch(url, options)
    const data = await resp.json().catch(() => ({}))
    if (!resp.ok) {
      throw new Error(data.error || `${resp.status} ${resp.statusText}`)
    }
    return data
  }

  async function post(url) {
    return fetchJson(url, { method: 'POST' })
  }

  async function pluginRpc(payload, expectedActions = ['values'], timeout = 1500) {
    const requestId = `${payload.action || 'req'}-${Date.now()}-${Math.random().toString(16).slice(2, 8)}`
    const resp = await fetchJson(`${apiBase}/plugin-rpc`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        node_id: '',
        plugin: pluginName,
        data: {
          ...payload,
          request_id: requestId,
        },
        timeout_ms: timeout,
        expected_actions: expectedActions,
      }),
    })

    if (!resp.success) {
      throw new Error(resp.error || '请求失败')
    }
    if (!resp.matched) {
      throw new Error('插件响应超时')
    }
    return resp.response || {}
  }

  async function pluginMessage(payload) {
    return fetchJson(`${apiBase}/plugin-message`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        node_id: '',
        plugin: pluginName,
        data: payload,
      }),
    })
  }

  function queueFastSet(source = 'fast-control') {
    flushSource = source
    if (flushQueued) {
      return
    }
    flushQueued = true

    const run = async () => {
      flushQueued = false
      try {
        await pluginMessage({
          action: 'set_values',
          slider_a: clamp(values.slider_a),
          slider_b: clamp(values.slider_b),
          source: flushSource,
        })
      } catch (e) {
        // ignore transient fast-path transport errors
      }
    }

    if (window.requestAnimationFrame) {
      window.requestAnimationFrame(() => run())
      return
    }
    setTimeout(run, 0)
  }

  async function syncReliable(source = 'reliable-sync') {
    const resp = await pluginRpc({
      action: 'set_values',
      slider_a: clamp(values.slider_a),
      slider_b: clamp(values.slider_b),
      source,
    }, ['values'], 1600)

    if (resp.action === 'values') {
      applyValues(resp.slider_a, resp.slider_b, 'remote', resp.updated_at_ms)
    }
  }

  async function refreshStatus() {
    const localResp = await fetchJson(`${apiBase}/plugins`)
    const local = (localResp.plugins || []).find((item) => item.name === pluginName)
    elLocalState.textContent = local ? local.state : '未安装'
  }

  async function refreshTransportInfo() {
    try {
      const resp = await pluginRpc({ action: 'get_transport', source: 'frontend' }, ['transport_info'], 1200)
      if (resp.action === 'transport_info') {
        transportInfo = {
          udp_enabled: !!resp.udp_enabled,
          udp_host: resp.udp_host || '127.0.0.1',
          udp_port: resp.udp_port || 19731,
        }
      }
    } catch (e) {
      transportInfo = {
        udp_enabled: false,
        udp_host: '127.0.0.1',
        udp_port: 19731,
      }
    }

    elTransport.textContent = transportInfo.udp_enabled
      ? `HTTP 快通道 + UDP(${transportInfo.udp_host}:${transportInfo.udp_port})`
      : 'HTTP 快通道（UDP 未启用）'

    syncApiHint()
  }

  async function refreshValues() {
    const resp = await pluginRpc({ action: 'get_values', source: 'frontend-poll' }, ['values'], 1200)
    if (resp.action === 'values') {
      applyValues(resp.slider_a, resp.slider_b, 'remote', resp.updated_at_ms)
    }
  }

  async function adjustValue(target, mode, source = 'frontend-key') {
    const resp = await pluginRpc({ action: 'adjust', target, mode, source }, ['values'])
    if (resp.action === 'values') {
      applyValues(resp.slider_a, resp.slider_b, 'remote', resp.updated_at_ms)
    }
  }

  function bindKeyInput(input, keyName) {
    input.addEventListener('click', () => {
      activeKeyInput = keyName
      input.value = '按下按键...'
    })

    input.addEventListener('blur', () => {
      if (activeKeyInput === keyName) {
        activeKeyInput = null
        input.value = (keyBindings[keyName] || '').toUpperCase()
      }
    })
  }

  async function onKeyActionByBinding(key) {
    if (!key) return false
    if (key === keyBindings.aInc) { await adjustValue('a', 'inc', 'keyboard'); return true }
    if (key === keyBindings.aDec) { await adjustValue('a', 'dec', 'keyboard'); return true }
    if (key === keyBindings.aReset) { await adjustValue('a', 'reset', 'keyboard'); return true }
    if (key === keyBindings.bInc) { await adjustValue('b', 'inc', 'keyboard'); return true }
    if (key === keyBindings.bDec) { await adjustValue('b', 'dec', 'keyboard'); return true }
    if (key === keyBindings.bReset) { await adjustValue('b', 'reset', 'keyboard'); return true }
    return false
  }

  const onKeyDown = async (event) => {
    const key = (event.key || '').toLowerCase()

    if (activeKeyInput) {
      event.preventDefault()
      if (key && key.length <= 16) {
        keyBindings[activeKeyInput] = key
        setBindingsToInputs()
      }
      activeKeyInput = null
      return
    }

    const targetTag = (event.target && event.target.tagName) ? event.target.tagName.toLowerCase() : ''
    if (targetTag === 'input' || targetTag === 'textarea' || event.isComposing) {
      return
    }

    try {
      const handled = await onKeyActionByBinding(key)
      if (handled) {
        event.preventDefault()
      }
    } catch (e) {
      // ignore transient keyboard control errors
    }
  }

  const onChannelMessage = (evt) => {
    const data = evt.data || {}
    if (data.slider_a === undefined || data.slider_b === undefined) {
      return
    }
    applyValues(data.slider_a, data.slider_b, 'remote', data.updated_at_ms || Date.now())
  }

  function valueFromJoystickPointer(padEl, clientY) {
    const rect = padEl.getBoundingClientRect()
    const centerY = rect.top + rect.height / 2
    const half = rect.height / 2
    const ratio = (centerY - clientY) / half
    return clamp(Math.round(ratio * 10) * 10)
  }

  function bindJoystick(padEl, target) {
    let dragging = false
    let pointerId = -1

    const updateFromPointer = (clientY) => {
      const value = valueFromJoystickPointer(padEl, clientY)
      if (target === 'a') {
        applyValues(value, values.slider_b, 'local', Date.now())
      } else {
        applyValues(values.slider_a, value, 'local', Date.now())
      }
      queueFastSet('mobile-joystick')
    }

    padEl.addEventListener('pointerdown', (event) => {
      dragging = true
      pointerId = event.pointerId
      padEl.setPointerCapture(pointerId)
      updateFromPointer(event.clientY)
    })

    padEl.addEventListener('pointermove', (event) => {
      if (!dragging || event.pointerId !== pointerId) {
        return
      }
      updateFromPointer(event.clientY)
    })

    const onPointerEnd = async (event) => {
      if (!dragging || event.pointerId !== pointerId) {
        return
      }
      dragging = false
      try {
        padEl.releasePointerCapture(pointerId)
      } catch (_) {}
      pointerId = -1
      try {
        await syncReliable('mobile-joystick-end')
      } catch (_) {}
    }

    padEl.addEventListener('pointerup', onPointerEnd)
    padEl.addEventListener('pointercancel', onPointerEnd)
  }

  async function enterMobileMode() {
    elMobileLayer.classList.add('active')
    elMobileLayer.setAttribute('aria-hidden', 'false')

    if (elMobileLayer.requestFullscreen) {
      try {
        await elMobileLayer.requestFullscreen({ navigationUI: 'hide' })
      } catch (_) {
        // fallback to fixed overlay
      }
    }
  }

  async function exitMobileMode() {
    elMobileLayer.classList.remove('active')
    elMobileLayer.setAttribute('aria-hidden', 'true')

    if (document.fullscreenElement && document.exitFullscreen) {
      try {
        await document.exitFullscreen()
      } catch (_) {
        // ignore
      }
    }
  }

  bindKeyInput(keyInputs.aInc, 'aInc')
  bindKeyInput(keyInputs.aDec, 'aDec')
  bindKeyInput(keyInputs.aReset, 'aReset')
  bindKeyInput(keyInputs.bInc, 'bInc')
  bindKeyInput(keyInputs.bDec, 'bDec')
  bindKeyInput(keyInputs.bReset, 'bReset')

  bindJoystick(elJoyPadA, 'a')
  bindJoystick(elJoyPadB, 'b')

  setBindingsToInputs()
  updateValueUI()
  syncApiHint()

  elTabControl.addEventListener('click', () => activateTab('control'))
  elTabDisplay.addEventListener('click', () => activateTab('display'))

  container.querySelector('#ds-key-default').addEventListener('click', () => {
    Object.assign(keyBindings, defaultBindings)
    setBindingsToInputs()
  })

  elA.addEventListener('input', () => {
    applyValues(elA.value, values.slider_b, 'local', Date.now())
    queueFastSet('touch-or-mouse')
  })

  elA.addEventListener('change', async () => {
    try {
      await syncReliable('touch-or-mouse-end')
    } catch (_) {}
  })

  elB.addEventListener('input', () => {
    applyValues(values.slider_a, elB.value, 'local', Date.now())
    queueFastSet('touch-or-mouse')
  })

  elB.addEventListener('change', async () => {
    try {
      await syncReliable('touch-or-mouse-end')
    } catch (_) {}
  })

  container.querySelectorAll('.quick-actions button[data-target]').forEach((btn) => {
    btn.addEventListener('click', async () => {
      const target = btn.getAttribute('data-target')
      const mode = btn.getAttribute('data-mode')
      if (!target || !mode) return
      await adjustValue(target, mode, 'button')
    })
  })

  container.querySelector('#ds-refresh').addEventListener('click', async () => {
    await refreshStatus()
    await refreshTransportInfo()
    await refreshValues()
  })

  container.querySelector('#ds-reset').addEventListener('click', async () => {
    const resp = await pluginRpc({ action: 'reset_all', source: 'frontend' }, ['values'])
    if (resp.action === 'values') {
      applyValues(resp.slider_a, resp.slider_b, 'remote', resp.updated_at_ms)
    }
  })

  container.querySelector('#ds-mobile-mode').addEventListener('click', async () => {
    await enterMobileMode()
  })

  container.querySelector('#ds-mobile-close').addEventListener('click', async () => {
    await exitMobileMode()
  })

  container.querySelector('#ds-start-local').addEventListener('click', async () => {
    await post(`${apiBase}/plugins/${pluginName}/start`)
    await refreshStatus()
  })

  container.querySelector('#ds-stop-local').addEventListener('click', async () => {
    await post(`${apiBase}/plugins/${pluginName}/stop`)
    await refreshStatus()
  })

  document.addEventListener('keydown', onKeyDown)
  if (channel) {
    channel.addEventListener('message', onChannelMessage)
  }

  try {
    await refreshStatus()
    await refreshTransportInfo()
    await refreshValues()
  } catch (_) {
    // initial load tolerates temporary failures
  }

  statusTimer = setInterval(async () => {
    if (stop) return
    try {
      await refreshStatus()
    } catch (_) {}
  }, 3000)

  transportTimer = setInterval(async () => {
    if (stop) return
    try {
      await refreshTransportInfo()
    } catch (_) {}
  }, 6000)

  valueTimer = setInterval(async () => {
    if (stop) return
    try {
      await refreshValues()
    } catch (_) {}
  }, 120)

  return () => {
    stop = true

    document.removeEventListener('keydown', onKeyDown)

    if (channel) {
      channel.removeEventListener('message', onChannelMessage)
      channel.close()
    }

    if (statusTimer) clearInterval(statusTimer)
    if (transportTimer) clearInterval(transportTimer)
    if (valueTimer) clearInterval(valueTimer)

    if (styleEl.parentNode) {
      styleEl.parentNode.removeChild(styleEl)
    }
  }
}
