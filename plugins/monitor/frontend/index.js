export async function mount(container, ctx) {
  const pluginName = ctx?.pluginName || 'monitor'
  const apiBase = (ctx?.apiBase || '/api').replace(/\/+$/, '')

  container.innerHTML = `
    <section class="monitor-panel">
      <header class="monitor-header">
        <div>
          <h2>系统监控面板</h2>
          <p>实时采集 HoneyTea 设备系统指标：CPU、内存、磁盘、温度与系统配置。</p>
        </div>
        <div class="header-actions">
          <button id="monitor-refresh" class="ghost">刷新</button>
          <button id="monitor-toggle" class="accent">停止采集</button>
        </div>
      </header>

      <section class="card status-card">
        <div class="status-grid">
          <label>
            HoneyTea 节点
            <select id="monitor-node"></select>
          </label>
          <label>
            LemonTea 插件状态
            <span id="m-local" class="badge">-</span>
          </label>
          <label>
            HoneyTea 插件状态
            <span id="m-remote" class="badge">-</span>
          </label>
          <label>
            CPU
            <span id="m-cpu" class="badge">-</span>
          </label>
          <label>
            内存
            <span id="m-mem" class="badge">-</span>
          </label>
          <label>
            温度
            <span id="m-temp" class="badge">-</span>
          </label>
        </div>
        <div class="btn-row">
          <button id="monitor-start-local">启动 LemonTea 插件</button>
          <button id="monitor-stop-local" class="warn">停止 LemonTea 插件</button>
          <button id="monitor-start-remote">启动 HoneyTea 插件</button>
          <button id="monitor-stop-remote" class="warn">停止 HoneyTea 插件</button>
        </div>
      </section>

      <section class="monitor-main">
        <article class="card chart-card">
          <h3>CPU / 内存趋势</h3>
          <svg id="m-chart" viewBox="0 0 620 220" preserveAspectRatio="none"></svg>
          <div class="legend">
            <span><i class="cpu"></i>CPU%</span>
            <span><i class="mem"></i>Mem%</span>
          </div>
        </article>

        <article class="card info-card">
          <h3>系统配置</h3>
          <dl id="m-system"></dl>
          <h3>磁盘</h3>
          <div id="m-disks"></div>
          <h3>日志</h3>
          <pre id="monitor-log"></pre>
        </article>
      </section>
    </section>
  `

  const styleEl = document.createElement('style')
  styleEl.textContent = `
    .monitor-panel { font-family: Manrope, 'Noto Sans SC', sans-serif; color:#1f2b24; }
    .monitor-header { display:flex; justify-content:space-between; gap:14px; align-items:flex-start; margin-bottom:12px; }
    .monitor-header h2 { margin:0; font-size:22px; }
    .monitor-header p { margin:6px 0 0; color:#5e7768; }
    .header-actions { display:flex; gap:8px; }
    .card { border:1px solid #cfe1d8; border-radius:14px; padding:12px; background:linear-gradient(135deg,#f3fff9,#f7fbff); box-shadow:0 10px 20px rgba(40,90,70,.08); }
    .status-card { margin-bottom:12px; }
    .status-grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(210px, 1fr)); gap:10px; margin-bottom:10px; }
    label { display:flex; flex-direction:column; gap:6px; font-size:12px; color:#567564; }
    select { padding:8px 10px; border-radius:8px; border:1px solid #b7d0c2; background:#fff; }
    .badge { display:inline-flex; align-items:center; min-height:34px; padding:0 10px; border-radius:8px; background:#e9f4ee; color:#2f6750; font-size:13px; }
    .btn-row { display:flex; flex-wrap:wrap; gap:8px; }
    button { border:0; border-radius:9px; padding:9px 12px; cursor:pointer; background:#2f8f6d; color:#fff; font-weight:600; min-width:132px; text-align:center; }
    button.warn { background:#b84f4f; }
    button.ghost { background:#eaf4ee; color:#2c6b55; min-width:92px; }
    button.accent { background:#2f5eb8; }
    .monitor-main { display:grid; grid-template-columns:minmax(520px, 2fr) minmax(320px, 1fr); gap:12px; }
    .chart-card svg { width:100%; height:280px; background:#0f1b1a; border-radius:10px; }
    .legend { display:flex; gap:14px; margin-top:8px; font-size:12px; color:#547366; }
    .legend i { display:inline-block; width:16px; height:3px; margin-right:6px; vertical-align:middle; border-radius:4px; }
    .legend i.cpu { background:#4dd69f; }
    .legend i.mem { background:#62b4ff; }
    .info-card dl { margin:0; display:grid; grid-template-columns:96px 1fr; gap:6px 10px; font-size:13px; }
    .info-card dt { color:#63806f; }
    .info-card dd { margin:0; color:#274838; word-break:break-all; }
    #m-disks { display:flex; flex-direction:column; gap:6px; font-size:13px; color:#2c4f3e; }
    #monitor-log { min-height:130px; max-height:180px; overflow:auto; margin:6px 0 0; border-radius:10px; background:#101917; color:#b9f2d2; padding:10px; }
    @media (max-width: 1180px) { .monitor-main { grid-template-columns:1fr; } }
  `
  container.prepend(styleEl)

  const elNode = container.querySelector('#monitor-node')
  const elLocal = container.querySelector('#m-local')
  const elRemote = container.querySelector('#m-remote')
  const elCpu = container.querySelector('#m-cpu')
  const elMem = container.querySelector('#m-mem')
  const elTemp = container.querySelector('#m-temp')
  const elSystem = container.querySelector('#m-system')
  const elDisks = container.querySelector('#m-disks')
  const elChart = container.querySelector('#m-chart')
  const elLog = container.querySelector('#monitor-log')
  const elToggle = container.querySelector('#monitor-toggle')

  let stop = false
  let collecting = true
  let metricTimer = null
  let statusTimer = null
  const cpuHistory = []
  const memHistory = []

  function log(msg) {
    elLog.textContent += `[${new Date().toLocaleTimeString()}] ${msg}\n`
    elLog.scrollTop = elLog.scrollHeight
  }

  function percent(v) {
    if (v === undefined || v === null || Number.isNaN(Number(v))) return '-'
    return `${Number(v).toFixed(1)}%`
  }

  function fmtMb(v) {
    if (!v && v !== 0) return '-'
    return `${Number(v).toFixed(0)} MB`
  }

  function fmtTemp(v) {
    if (v === undefined || v === null || Number(v) < 0) return 'N/A'
    return `${Number(v).toFixed(1)} °C`
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

  async function rpc(data, expectedActions, timeout = 5000) {
    if (!elNode.value) throw new Error('请先选择 HoneyTea 节点')

    const requestId = `${data.action}-${Date.now()}-${Math.random().toString(16).slice(2, 8)}`
    const resp = await fetchJson(`${apiBase}/plugin-rpc`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        node_id: elNode.value,
        plugin: pluginName,
        data: { ...data, request_id: requestId },
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

  function pushHistory(cpu, mem) {
    cpuHistory.push(Number(cpu || 0))
    memHistory.push(Number(mem || 0))
    while (cpuHistory.length > 60) cpuHistory.shift()
    while (memHistory.length > 60) memHistory.shift()
  }

  function pathFromSeries(series, width, height) {
    if (!series.length) return ''
    const dx = width / Math.max(series.length - 1, 1)
    return series
      .map((v, i) => {
        const x = i * dx
        const y = height - Math.max(0, Math.min(100, Number(v || 0))) * (height / 100)
        return `${i === 0 ? 'M' : 'L'}${x.toFixed(2)},${y.toFixed(2)}`
      })
      .join(' ')
  }

  function renderChart() {
    const w = 620
    const h = 220
    const cpuPath = pathFromSeries(cpuHistory, w, h)
    const memPath = pathFromSeries(memHistory, w, h)
    elChart.innerHTML = `
      <rect x="0" y="0" width="${w}" height="${h}" fill="#101918" />
      <g stroke="#2b3f3f" stroke-width="1">
        <line x1="0" y1="55" x2="${w}" y2="55" />
        <line x1="0" y1="110" x2="${w}" y2="110" />
        <line x1="0" y1="165" x2="${w}" y2="165" />
      </g>
      <path d="${cpuPath}" fill="none" stroke="#4dd69f" stroke-width="2.4" />
      <path d="${memPath}" fill="none" stroke="#62b4ff" stroke-width="2.4" />
    `
  }

  function renderSystem(system = {}) {
    const rows = [
      ['主机', system.hostname],
      ['系统', system.os],
      ['内核', system.kernel],
      ['架构', system.arch],
      ['CPU', system.cpu_model],
      ['核心数', system.cpu_cores],
      ['运行时长', system.uptime_sec ? `${system.uptime_sec}s` : '-'],
    ]
    elSystem.innerHTML = rows
      .map(([k, v]) => `<dt>${k}</dt><dd>${v || '-'}</dd>`)
      .join('')
  }

  function renderDisks(disks = []) {
    if (!Array.isArray(disks) || !disks.length) {
      elDisks.innerHTML = '<span>暂无磁盘信息</span>'
      return
    }
    elDisks.innerHTML = disks
      .map((d) => {
        const used = Number(d.used_gb || 0).toFixed(2)
        const total = Number(d.total_gb || 0).toFixed(2)
        const ratio = percent(d.usage_percent)
        return `<span>${d.mount || '/'}: ${used} / ${total} GB (${ratio})</span>`
      })
      .join('')
  }

  async function refreshStatus() {
    const [pluginsResp, clientsResp] = await Promise.all([
      fetchJson(`${apiBase}/plugins`),
      fetchJson(`${apiBase}/clients`),
    ])

    const clients = (clientsResp.clients || []).filter((c) => c.connected)
    const selected = elNode.value
    elNode.innerHTML = clients
      .map((c) => `<option value="${c.node_id}">${c.node_id} (${c.address}:${c.port})</option>`)
      .join('')

    if (selected && clients.some((c) => c.node_id === selected)) {
      elNode.value = selected
    }

    const localPlugin = (pluginsResp.plugins || []).find((p) => p.name === pluginName)
    elLocal.textContent = localPlugin ? localPlugin.state : '未安装'

    if (elNode.value) {
      const remoteResp = await fetchJson(`${apiBase}/clients/${elNode.value}/plugins`)
      const remotePlugin = (remoteResp.plugins || []).find((p) => p.name === pluginName)
      elRemote.textContent = remotePlugin ? remotePlugin.state : '未安装'
    } else {
      elRemote.textContent = '无在线节点'
    }
  }

  async function refreshMetrics() {
    if (!collecting || !elNode.value) return
    const data = await rpc({ action: 'request_metrics' }, ['metrics', 'error'], 4500)
    if (data.action === 'error') {
      throw new Error(data.message || '采集失败')
    }

    const cpu = Number(data.cpu_percent || 0)
    const memUsage = Number(data.memory?.usage_percent || 0)
    elCpu.textContent = percent(cpu)
    elMem.textContent = `${percent(memUsage)} (${fmtMb(data.memory?.used_mb)} / ${fmtMb(data.memory?.total_mb)})`
    elTemp.textContent = fmtTemp(data.temperature)

    pushHistory(cpu, memUsage)
    renderChart()
    renderSystem(data.system || {})
    renderDisks(data.disks || [])
  }

  async function toggleCollecting() {
    if (!elNode.value) {
      throw new Error('请先选择 HoneyTea 节点')
    }

    if (collecting) {
      await rpc({ action: 'stop_collecting' }, ['success', 'error'])
      collecting = false
      elToggle.textContent = '开始采集'
      log('监控采集已停止')
      return
    }

    await rpc({ action: 'start_collecting', interval_ms: 2000 }, ['success', 'error'])
    collecting = true
    elToggle.textContent = '停止采集'
    log('监控采集已启动')
  }

  const onRefresh = () => Promise.all([refreshStatus(), refreshMetrics()]).catch((e) => log(`刷新失败: ${e.message}`))
  const onStartLocal = () => post(`${apiBase}/plugins/${pluginName}/start`).then(refreshStatus).catch((e) => log(`启动失败: ${e.message}`))
  const onStopLocal = () => post(`${apiBase}/plugins/${pluginName}/stop`).then(refreshStatus).catch((e) => log(`停止失败: ${e.message}`))
  const onStartRemote = () => post(`${apiBase}/clients/${elNode.value}/plugins/${pluginName}/start`).then(refreshStatus).catch((e) => log(`启动失败: ${e.message}`))
  const onStopRemote = () => post(`${apiBase}/clients/${elNode.value}/plugins/${pluginName}/stop`).then(refreshStatus).catch((e) => log(`停止失败: ${e.message}`))
  const onToggle = () => toggleCollecting().catch((e) => log(e.message))
  const onNodeChange = () => {
    cpuHistory.length = 0
    memHistory.length = 0
    renderChart()
    refreshMetrics().catch(() => {})
  }

  container.querySelector('#monitor-refresh').addEventListener('click', onRefresh)
  container.querySelector('#monitor-start-local').addEventListener('click', onStartLocal)
  container.querySelector('#monitor-stop-local').addEventListener('click', onStopLocal)
  container.querySelector('#monitor-start-remote').addEventListener('click', onStartRemote)
  container.querySelector('#monitor-stop-remote').addEventListener('click', onStopRemote)
  container.querySelector('#monitor-toggle').addEventListener('click', onToggle)
  elNode.addEventListener('change', onNodeChange)

  await refreshStatus().catch((e) => log(`初始化失败: ${e.message}`))
  await rpc({ action: 'start_collecting', interval_ms: 2000 }, ['success', 'error']).catch(() => {})
  collecting = true
  elToggle.textContent = '停止采集'
  await refreshMetrics().catch((e) => log(`采集失败: ${e.message}`))

  statusTimer = setInterval(() => {
    if (stop) return
    refreshStatus().catch(() => {})
  }, 3000)

  metricTimer = setInterval(() => {
    if (stop) return
    refreshMetrics().catch(() => {})
  }, 2000)

  return () => {
    stop = true
    clearInterval(statusTimer)
    clearInterval(metricTimer)
    container.querySelector('#monitor-refresh')?.removeEventListener('click', onRefresh)
    container.querySelector('#monitor-start-local')?.removeEventListener('click', onStartLocal)
    container.querySelector('#monitor-stop-local')?.removeEventListener('click', onStopLocal)
    container.querySelector('#monitor-start-remote')?.removeEventListener('click', onStartRemote)
    container.querySelector('#monitor-stop-remote')?.removeEventListener('click', onStopRemote)
    container.querySelector('#monitor-toggle')?.removeEventListener('click', onToggle)
    elNode?.removeEventListener('change', onNodeChange)
    styleEl.remove()
    container.innerHTML = ''
  }
}
