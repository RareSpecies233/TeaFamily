export async function mount(container, ctx) {
  const pluginName = ctx?.pluginName || 'dual-slider'
  const apiBase = (ctx?.apiBase || '/api').replace(/\/+$/, '')

  container.innerHTML = `
    <section class="ds-root">
      <header class="ds-header">
        <div>
          <h2>双滑块控制台</h2>
          <p>控制界面支持双滑块联动与桌面键盘映射，展示界面仅显示当前数值。</p>
        </div>
        <div class="ds-actions">
          <button id="ds-refresh" class="ghost">刷新</button>
          <button id="ds-reset" class="warn">全部归零</button>
        </div>
      </header>

      <section class="ds-card ds-top-card">
        <div class="ds-status-grid">
          <label>
            HoneyTea 节点
            <select id="ds-node"></select>
          </label>
          <label>
            LemonTea 插件状态
            <span id="ds-local-state" class="ds-badge">-</span>
          </label>
          <label>
            HoneyTea 插件状态
            <span id="ds-remote-state" class="ds-badge">-</span>
          </label>
          <label>
            当前值
            <span id="ds-values" class="ds-badge">A: 0% / B: 0%</span>
          </label>
        </div>
        <div class="ds-btn-row">
          <button id="ds-start-local">启动 LemonTea 插件</button>
          <button id="ds-stop-local" class="warn">停止 LemonTea 插件</button>
          <button id="ds-start-remote">启动 HoneyTea 插件</button>
          <button id="ds-stop-remote" class="warn">停止 HoneyTea 插件</button>
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
          <p>该页面仅展示当前数值，便于投屏或移动端查看。</p>
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
    </section>
  `

  const styleEl = document.createElement('style')
  styleEl.textContent = `
    .ds-root { font-family: Manrope, 'Noto Sans SC', sans-serif; color: #1d2b2c; }
    .ds-header { display:flex; justify-content:space-between; gap:14px; align-items:flex-start; margin-bottom:12px; }
    .ds-header h2 { margin:0; font-size:22px; }
    .ds-header p { margin:6px 0 0; color:#4d6668; }
    .ds-actions { display:flex; gap:8px; }
    .ds-card { border:1px solid #c7dfe2; border-radius:14px; padding:12px; background: linear-gradient(135deg, #f2fbff, #f7fff5); box-shadow: 0 10px 20px rgba(30,90,98,.08); }
    .ds-top-card { margin-bottom:12px; }
    .ds-status-grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(200px, 1fr)); gap:10px; margin-bottom:10px; }
    .ds-status-grid label { display:flex; flex-direction:column; gap:6px; font-size:12px; color:#4d6668; }
    .ds-status-grid select { padding:8px 10px; border-radius:8px; border:1px solid #b6d2d8; background:#fff; }
    .ds-badge { display:inline-flex; align-items:center; min-height:34px; padding:0 10px; border-radius:8px; background:#e7f3f5; color:#2f5e66; font-size:13px; }
    .ds-btn-row { display:flex; flex-wrap:wrap; gap:8px; }
    .ds-root button { border:0; border-radius:9px; padding:9px 12px; cursor:pointer; background:#2e8f75; color:#fff; font-weight:600; min-width:124px; text-align:center; }
    .ds-root button.warn { background:#b84f4f; }
    .ds-root button.ghost { background:#e8f0f6; color:#2b4f72; }
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
    .display-item { border-radius:12px; background:#13282a; color:#dff8f9; padding:16px; display:flex; flex-direction:column; gap:6px; }
    .display-item span { font-size:12px; opacity:.8; }
    .display-item strong { font-size:34px; line-height:1; }
    .ds-api-hint { margin-top:14px; border-radius:10px; background:#101917; color:#b9f2d2; padding:10px; overflow:auto; }
    @media (max-width: 1050px) { .ds-main-grid { grid-template-columns:1fr; } }
  `
  container.prepend(styleEl)

  const elNode = container.querySelector('#ds-node')
  const elLocalState = container.querySelector('#ds-local-state')
  const elRemoteState = container.querySelector('#ds-remote-state')
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
  let activeKeyInput = null

  let values = { slider_a: 0, slider_b: 0 }

  const defaultBindings = {
    aInc: 'q',
    aDec: 'a',
    aReset: 'z',
    bInc: 'w',
    bDec: 's',
    bReset: 'x',
  }

  const keyBindings = { ...defaultBindings }

  function formatPercent(v) {
    return `${Number(v || 0).toFixed(0)}%`
  }

  function clamp(v) {
    return Math.max(-100, Math.min(100, Number(v || 0)))
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
    elValues.textContent = `A: ${formatPercent(a)} / B: ${formatPercent(b)}`
  }

  function syncApiHint() {
    const node = elNode.value || '<node-id>'
    const base = apiBase.replace(/\/+$/, '')
    elApiHint.textContent = [
      '实时获取数值 API（给其他插件或应用调用）',
      `POST ${base}/plugin-rpc`,
      '',
      '{',
      `  "node_id": "${node}",`,
      '  "plugin": "dual-slider",',
      '  "data": { "action": "get_values" },',
      '  "timeout_ms": 3000,',
      '  "expected_actions": ["values"]',
      '}',
      '',
      '响应示例：',
      '{ "action":"values", "slider_a":10, "slider_b":-20, "updated_at_ms": 123456789 }',
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

  async function rpc(payload, expectedActions = ['values'], timeout = 3000) {
    if (!elNode.value) {
      throw new Error('请先选择 HoneyTea 节点')
    }

    const requestId = `${payload.action || 'req'}-${Date.now()}-${Math.random().toString(16).slice(2, 8)}`
    const resp = await fetchJson(`${apiBase}/plugin-rpc`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        node_id: elNode.value,
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

  async function refreshStatus() {
    const [clientsResp, localResp] = await Promise.all([
      fetchJson(`${apiBase}/clients`),
      fetchJson(`${apiBase}/plugins`),
    ])

    const clients = (clientsResp.clients || []).filter((item) => item.connected)
    const selected = elNode.value

    elNode.innerHTML = clients
      .map((item) => `<option value="${item.node_id}">${item.node_id} (${item.address}:${item.port})</option>`)
      .join('')

    if (selected && clients.some((item) => item.node_id === selected)) {
      elNode.value = selected
    }

    const local = (localResp.plugins || []).find((item) => item.name === pluginName)
    elLocalState.textContent = local ? local.state : '未安装'

    if (elNode.value) {
      const remoteResp = await fetchJson(`${apiBase}/clients/${elNode.value}/plugins`)
      const remote = (remoteResp.plugins || []).find((item) => item.name === pluginName)
      elRemoteState.textContent = remote ? remote.state : '未安装'
    } else {
      elRemoteState.textContent = '无在线节点'
    }

    syncApiHint()
  }

  async function refreshValues() {
    if (!elNode.value) {
      return
    }
    const resp = await rpc({ action: 'get_values', source: 'frontend' }, ['values'], 2500)
    if (resp.action === 'values') {
      values.slider_a = clamp(resp.slider_a)
      values.slider_b = clamp(resp.slider_b)
      updateValueUI()
    }
  }

  async function setValues(a, b, source = 'frontend-slider') {
    const resp = await rpc({ action: 'set_values', slider_a: clamp(a), slider_b: clamp(b), source }, ['values'])
    if (resp.action === 'values') {
      values.slider_a = clamp(resp.slider_a)
      values.slider_b = clamp(resp.slider_b)
      updateValueUI()
    }
  }

  async function adjustValue(target, mode, source = 'frontend-key') {
    const resp = await rpc({ action: 'adjust', target, mode, source }, ['values'])
    if (resp.action === 'values') {
      values.slider_a = clamp(resp.slider_a)
      values.slider_b = clamp(resp.slider_b)
      updateValueUI()
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
      // ignore key-triggered transient errors to avoid blocking input loop
    }
  }

  bindKeyInput(keyInputs.aInc, 'aInc')
  bindKeyInput(keyInputs.aDec, 'aDec')
  bindKeyInput(keyInputs.aReset, 'aReset')
  bindKeyInput(keyInputs.bInc, 'bInc')
  bindKeyInput(keyInputs.bDec, 'bDec')
  bindKeyInput(keyInputs.bReset, 'bReset')

  setBindingsToInputs()
  updateValueUI()
  syncApiHint()

  elTabControl.addEventListener('click', () => activateTab('control'))
  elTabDisplay.addEventListener('click', () => activateTab('display'))

  container.querySelector('#ds-key-default').addEventListener('click', () => {
    Object.assign(keyBindings, defaultBindings)
    setBindingsToInputs()
  })

  container.querySelector('#ds-slider-a').addEventListener('input', async (e) => {
    values.slider_a = clamp(e.target.value)
    updateValueUI()
    try {
      await setValues(values.slider_a, values.slider_b, 'touch-or-mouse')
    } catch (err) {
      // ignore transient transport errors
    }
  })

  container.querySelector('#ds-slider-b').addEventListener('input', async (e) => {
    values.slider_b = clamp(e.target.value)
    updateValueUI()
    try {
      await setValues(values.slider_a, values.slider_b, 'touch-or-mouse')
    } catch (err) {
      // ignore transient transport errors
    }
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
    await refreshValues()
  })

  container.querySelector('#ds-reset').addEventListener('click', async () => {
    await rpc({ action: 'reset_all', source: 'frontend' }, ['values'])
    await refreshValues()
  })

  container.querySelector('#ds-start-local').addEventListener('click', async () => {
    await post(`${apiBase}/plugins/${pluginName}/start`)
    await refreshStatus()
  })

  container.querySelector('#ds-stop-local').addEventListener('click', async () => {
    await post(`${apiBase}/plugins/${pluginName}/stop`)
    await refreshStatus()
  })

  container.querySelector('#ds-start-remote').addEventListener('click', async () => {
    if (!elNode.value) throw new Error('请先选择 HoneyTea 节点')
    await post(`${apiBase}/clients/${elNode.value}/plugins/${pluginName}/start`)
    await refreshStatus()
  })

  container.querySelector('#ds-stop-remote').addEventListener('click', async () => {
    if (!elNode.value) throw new Error('请先选择 HoneyTea 节点')
    await post(`${apiBase}/clients/${elNode.value}/plugins/${pluginName}/stop`)
    await refreshStatus()
  })

  elNode.addEventListener('change', async () => {
    syncApiHint()
    await refreshStatus()
    await refreshValues()
  })

  document.addEventListener('keydown', onKeyDown)

  try {
    await refreshStatus()
    await refreshValues()
  } catch (err) {
    // initial load tolerates temporary disconnection
  }

  statusTimer = setInterval(async () => {
    if (stop) return
    try {
      await refreshStatus()
    } catch (e) {
      // ignore transient status error
    }
  }, 3000)

  valueTimer = setInterval(async () => {
    if (stop) return
    try {
      await refreshValues()
    } catch (e) {
      // ignore transient value error
    }
  }, 500)

  return () => {
    stop = true
    document.removeEventListener('keydown', onKeyDown)
    if (statusTimer) clearInterval(statusTimer)
    if (valueTimer) clearInterval(valueTimer)
    if (styleEl.parentNode) styleEl.parentNode.removeChild(styleEl)
  }
}
