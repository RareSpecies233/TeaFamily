export async function mount(container, ctx) {
  const pluginName = ctx?.pluginName || 'monitor'
  const apiBase = ctx?.apiBase || '/api'

  container.innerHTML = `
    <section style="font-family: Manrope, 'Noto Sans SC', sans-serif; border:1px solid #f0dcc0; border-radius:14px; padding:16px; background:linear-gradient(135deg,#f9fff0,#edf7ff)">
      <h3 style="margin:0 0 6px; color:#2f220f;">监控插件</h3>
      <p style="margin:0 0 12px; color:#7e6a49;">监控插件进程并查看节点上的插件状态快照。</p>
      <div style="display:flex; flex-wrap:wrap; gap:8px; align-items:center; margin-bottom:12px;">
        <select id="monitor-node" style="padding:8px 10px; border-radius:8px; border:1px solid #dbc9aa; min-width:220px;"></select>
        <button id="monitor-refresh" style="padding:8px 12px; border:0; border-radius:8px; background:#d9ece4; cursor:pointer;">刷新节点</button>
        <button id="monitor-start-local" style="padding:8px 12px; border:0; border-radius:8px; background:#2f8f6d; color:#fff; cursor:pointer;">启动 LemonTea 侧</button>
        <button id="monitor-stop-local" style="padding:8px 12px; border:0; border-radius:8px; background:#d08b22; color:#fff; cursor:pointer;">停止 LemonTea 侧</button>
        <button id="monitor-start-remote" style="padding:8px 12px; border:0; border-radius:8px; background:#3e7bb8; color:#fff; cursor:pointer;">启动 HoneyTea 侧</button>
        <button id="monitor-stop-remote" style="padding:8px 12px; border:0; border-radius:8px; background:#785fa8; color:#fff; cursor:pointer;">停止 HoneyTea 侧</button>
      </div>
      <pre id="monitor-log" style="margin:0; min-height:220px; max-height:340px; overflow:auto; border-radius:10px; background:#111b1a; color:#d0ffef; padding:12px; font:12px/1.5 ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;"></pre>
    </section>
  `

  const nodeSelect = container.querySelector('#monitor-node')
  const logEl = container.querySelector('#monitor-log')

  function log(line) {
    logEl.textContent += `[${new Date().toLocaleTimeString()}] ${line}\n`
    logEl.scrollTop = logEl.scrollHeight
  }

  async function fetchJson(url, options) {
    const resp = await fetch(url, options)
    const data = await resp.json().catch(() => ({}))
    if (!resp.ok) {
      throw new Error(data.error || `HTTP ${resp.status}`)
    }
    return data
  }

  async function refreshNodes() {
    const data = await fetchJson(`${apiBase}/clients`)
    const clients = data.clients || []
    nodeSelect.innerHTML = clients
      .map((c) => `<option value="${c.node_id}">${c.node_id} (${c.address}:${c.port})</option>`)
      .join('')
    log(`已加载 ${clients.length} 个 HoneyTea 节点`)
    if (nodeSelect.value) {
      const pluginData = await fetchJson(`${apiBase}/clients/${nodeSelect.value}/plugins`)
      log(`当前节点插件: ${JSON.stringify(pluginData.plugins || [])}`)
    }
  }

  async function startLocal() {
    const data = await fetchJson(`${apiBase}/plugins/${pluginName}/start`, { method: 'POST' })
    log(`LemonTea 启动结果: ${JSON.stringify(data)}`)
  }

  async function stopLocal() {
    const data = await fetchJson(`${apiBase}/plugins/${pluginName}/stop`, { method: 'POST' })
    log(`LemonTea 停止结果: ${JSON.stringify(data)}`)
  }

  async function startRemote() {
    const nodeId = nodeSelect.value
    if (!nodeId) throw new Error('请先选择 HoneyTea 节点')
    const data = await fetchJson(`${apiBase}/clients/${nodeId}/plugins/${pluginName}/start`, { method: 'POST' })
    log(`HoneyTea(${nodeId}) 启动结果: ${JSON.stringify(data)}`)
  }

  async function stopRemote() {
    const nodeId = nodeSelect.value
    if (!nodeId) throw new Error('请先选择 HoneyTea 节点')
    const data = await fetchJson(`${apiBase}/clients/${nodeId}/plugins/${pluginName}/stop`, { method: 'POST' })
    log(`HoneyTea(${nodeId}) 停止结果: ${JSON.stringify(data)}`)
  }

  const onRefresh = () => refreshNodes().catch((e) => log(`刷新失败: ${e.message}`))
  const onStartLocal = () => startLocal().catch((e) => log(`操作失败: ${e.message}`))
  const onStopLocal = () => stopLocal().catch((e) => log(`操作失败: ${e.message}`))
  const onStartRemote = () => startRemote().catch((e) => log(`操作失败: ${e.message}`))
  const onStopRemote = () => stopRemote().catch((e) => log(`操作失败: ${e.message}`))

  container.querySelector('#monitor-refresh').addEventListener('click', onRefresh)
  container.querySelector('#monitor-start-local').addEventListener('click', onStartLocal)
  container.querySelector('#monitor-stop-local').addEventListener('click', onStopLocal)
  container.querySelector('#monitor-start-remote').addEventListener('click', onStartRemote)
  container.querySelector('#monitor-stop-remote').addEventListener('click', onStopRemote)

  refreshNodes().catch((e) => log(`初始化失败: ${e.message}`))
  log('监控页面已加载')

  return () => {
    container.querySelector('#monitor-refresh')?.removeEventListener('click', onRefresh)
    container.querySelector('#monitor-start-local')?.removeEventListener('click', onStartLocal)
    container.querySelector('#monitor-stop-local')?.removeEventListener('click', onStopLocal)
    container.querySelector('#monitor-start-remote')?.removeEventListener('click', onStartRemote)
    container.querySelector('#monitor-stop-remote')?.removeEventListener('click', onStopRemote)
    container.innerHTML = ''
  }
}
