export async function mount(container, ctx) {
  const pluginName = ctx?.pluginName || 'file-manager'
  const apiBase = (ctx?.apiBase || '/api').replace(/\/+$/, '')

  container.innerHTML = `
    <section class="fm-panel">
      <header class="fm-header">
        <div>
          <h2>文件管理器</h2>
          <p>参考 dufs 的双区布局：左侧目录浏览，右侧文件预览与操作日志。</p>
        </div>
        <div class="header-actions">
          <button id="fm-refresh" class="ghost">刷新</button>
          <button id="fm-open-terminal" class="accent">在此打开终端</button>
        </div>
      </header>

      <section class="card status-card">
        <div class="status-grid">
          <label>
            HoneyTea 节点
            <select id="fm-node"></select>
          </label>
          <label>
            LemonTea 插件状态
            <span id="fm-local-state" class="badge">-</span>
          </label>
          <label>
            HoneyTea 插件状态
            <span id="fm-remote-state" class="badge">-</span>
          </label>
        </div>
        <div class="btn-row">
          <button id="fm-start-local">启动 LemonTea 插件</button>
          <button id="fm-stop-local" class="warn">停止 LemonTea 插件</button>
          <button id="fm-start-remote">启动 HoneyTea 插件</button>
          <button id="fm-stop-remote" class="warn">停止 HoneyTea 插件</button>
        </div>
      </section>

      <section class="fm-main">
        <article class="card browser-card">
          <div class="path-row">
            <button id="fm-up" class="ghost">上一级</button>
            <input id="fm-path" value="/" />
            <button id="fm-go" class="ghost">进入</button>
          </div>

          <div class="path-actions">
            <button id="fm-new-folder">新建目录</button>
            <button id="fm-rename">重命名</button>
            <button id="fm-delete" class="warn">删除</button>
            <button id="fm-download">下载</button>
            <button id="fm-upload">上传文件</button>
            <input id="fm-upload-input" type="file" hidden />
          </div>

          <div class="crumbs" id="fm-crumbs"></div>

          <table class="file-table">
            <thead>
              <tr>
                <th>名称</th>
                <th>类型</th>
                <th>大小</th>
              </tr>
            </thead>
            <tbody id="fm-body"></tbody>
          </table>
        </article>

        <article class="card preview-card">
          <h3>文件预览</h3>
          <textarea id="fm-preview" placeholder="选择文本文件后可预览内容"></textarea>
          <div class="btn-row compact">
            <button id="fm-load-preview" class="ghost">读取文件</button>
            <button id="fm-save-preview">保存修改</button>
          </div>
          <h3 class="log-title">操作日志</h3>
          <pre id="fm-log"></pre>
        </article>
      </section>
    </section>
  `

  const styleEl = document.createElement('style')
  styleEl.textContent = `
    .fm-panel { font-family: Manrope, 'Noto Sans SC', sans-serif; color:#2a2012; }
    .fm-header { display:flex; justify-content:space-between; gap:14px; align-items:flex-start; margin-bottom:12px; }
    .fm-header h2 { margin:0; font-size:22px; }
    .fm-header p { margin:6px 0 0; color:#6f5c3a; }
    .header-actions { display:flex; gap:8px; }
    .card { border:1px solid #d6e2ef; border-radius:14px; padding:12px; background:linear-gradient(135deg,#f7fbff,#fff8ee); box-shadow:0 10px 20px rgba(38,83,130,.08); }
    .status-card { margin-bottom:12px; }
    .status-grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(220px, 1fr)); gap:10px; margin-bottom:10px; }
    label { display:flex; flex-direction:column; gap:6px; font-size:12px; color:#5f708f; }
    input, select, textarea { width:100%; padding:8px 10px; border-radius:8px; border:1px solid #b9cae0; background:#fff; box-sizing:border-box; }
    .badge { display:inline-flex; align-items:center; min-height:34px; padding:0 10px; border-radius:8px; background:#edf3fb; color:#355175; font-size:13px; }
    .btn-row { display:flex; flex-wrap:wrap; gap:8px; }
    .btn-row.compact { margin-top:8px; }
    button { border:0; border-radius:9px; padding:9px 12px; cursor:pointer; background:#2f8f6d; color:#fff; font-weight:600; min-width:120px; text-align:center; }
    button.warn { background:#b84f4f; }
    button.ghost { background:#e9f0f8; color:#315173; }
    button.accent { background:#2f5eb8; }
    .fm-main { display:grid; grid-template-columns:minmax(520px, 2fr) minmax(320px, 1fr); gap:12px; }
    .browser-card { min-height:540px; }
    .path-row { display:grid; grid-template-columns:96px 1fr 72px; gap:8px; margin-bottom:8px; }
    .path-actions { display:flex; flex-wrap:wrap; gap:8px; margin-bottom:10px; }
    .crumbs { display:flex; flex-wrap:wrap; gap:6px; font-size:12px; color:#5978a2; margin-bottom:10px; }
    .crumbs a { color:#2f5eb8; cursor:pointer; text-decoration:none; }
    .file-table { width:100%; border-collapse:collapse; font-size:13px; }
    .file-table th, .file-table td { padding:8px; border-bottom:1px solid #e4edf7; text-align:left; }
    .file-table tbody tr { cursor:pointer; }
    .file-table tbody tr:hover { background:#eef5ff; }
    .file-table tbody tr.selected { background:#dceafe; }
    .preview-card textarea { min-height:240px; resize:vertical; font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; }
    .log-title { margin:14px 0 8px; font-size:16px; }
    #fm-log { min-height:170px; max-height:220px; overflow:auto; margin:0; border-radius:10px; background:#121923; color:#d3e9ff; padding:10px; }
    @media (max-width: 1180px) { .fm-main { grid-template-columns:1fr; } }
  `
  container.prepend(styleEl)

  const elNode = container.querySelector('#fm-node')
  const elLocalState = container.querySelector('#fm-local-state')
  const elRemoteState = container.querySelector('#fm-remote-state')
  const elPath = container.querySelector('#fm-path')
  const elBody = container.querySelector('#fm-body')
  const elPreview = container.querySelector('#fm-preview')
  const elLog = container.querySelector('#fm-log')
  const elCrumbs = container.querySelector('#fm-crumbs')
  const elUploadInput = container.querySelector('#fm-upload-input')

  let stop = false
  let entries = []
  let selected = null
  let statusTimer = null

  function nowText() {
    return new Date().toLocaleTimeString()
  }

  function log(text) {
    elLog.textContent += `[${nowText()}] ${text}\n`
    elLog.scrollTop = elLog.scrollHeight
  }

  function normalizePath(path) {
    if (!path) return '/'
    let out = path.trim()
    if (!out.startsWith('/')) out = `/${out}`
    out = out.replace(/\/{2,}/g, '/')
    if (out.length > 1 && out.endsWith('/')) out = out.slice(0, -1)
    return out || '/'
  }

  function joinPath(base, name) {
    return normalizePath(`${normalizePath(base)}/${name}`)
  }

  function parentPath(path) {
    const clean = normalizePath(path)
    if (clean === '/') return '/'
    const idx = clean.lastIndexOf('/')
    return idx <= 0 ? '/' : clean.slice(0, idx)
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

  async function rpc(data, expectedActions, timeout = 4000) {
    if (!elNode.value) {
      throw new Error('请先选择 HoneyTea 节点')
    }

    const requestId = `${data.action || 'req'}-${Date.now()}-${Math.random().toString(16).slice(2, 8)}`
    const payload = {
      ...data,
      request_id: requestId,
    }

    const resp = await fetchJson(`${apiBase}/plugin-rpc`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        node_id: elNode.value,
        plugin: pluginName,
        data: payload,
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
    const selectedNode = elNode.value
    elNode.innerHTML = clients
      .map((item) => `<option value="${item.node_id}">${item.node_id} (${item.address}:${item.port})</option>`)
      .join('')

    if (selectedNode && clients.some((item) => item.node_id === selectedNode)) {
      elNode.value = selectedNode
    }

    const localPlugin = (localResp.plugins || []).find((item) => item.name === pluginName)
    elLocalState.textContent = localPlugin ? localPlugin.state : '未安装'

    if (elNode.value) {
      const remoteResp = await fetchJson(`${apiBase}/clients/${elNode.value}/plugins`)
      const remotePlugin = (remoteResp.plugins || []).find((item) => item.name === pluginName)
      elRemoteState.textContent = remotePlugin ? remotePlugin.state : '未安装'
    } else {
      elRemoteState.textContent = '无在线节点'
    }
  }

  function renderCrumbs(path) {
    const clean = normalizePath(path)
    const parts = clean.split('/').filter(Boolean)
    const nodes = ['<a data-path="/">/</a>']
    let cur = ''
    for (const part of parts) {
      cur = `${cur}/${part}`
      nodes.push(`<span>/</span><a data-path="${cur}">${part}</a>`)
    }
    elCrumbs.innerHTML = nodes.join('')

    elCrumbs.querySelectorAll('a').forEach((a) => {
      a.addEventListener('click', () => {
        const p = a.getAttribute('data-path') || '/'
        listDir(p).catch((e) => log(`进入失败: ${e.message}`))
      })
    })
  }

  function renderTable() {
    elBody.innerHTML = ''
    for (const item of entries) {
      const tr = document.createElement('tr')
      if (selected?.name === item.name) {
        tr.classList.add('selected')
      }

      const type = item.is_dir ? '目录' : '文件'
      const size = item.is_dir ? '-' : `${Number(item.size || 0).toLocaleString()} B`
      tr.innerHTML = `
        <td>${item.is_dir ? '📁' : '📄'} ${item.name}</td>
        <td>${type}</td>
        <td>${size}</td>
      `

      tr.addEventListener('click', () => {
        selected = item
        renderTable()
      })

      tr.addEventListener('dblclick', () => {
        if (item.is_dir) {
          listDir(joinPath(elPath.value, item.name)).catch((e) => log(`打开目录失败: ${e.message}`))
        } else {
          loadPreview().catch((e) => log(`读取失败: ${e.message}`))
        }
      })

      elBody.appendChild(tr)
    }
  }

  async function listDir(path) {
    const dir = normalizePath(path)
    const data = await rpc({ action: 'list', path: dir }, ['list_result', 'error'])
    if (data.action === 'error') {
      throw new Error(data.message || '列目录失败')
    }
    entries = data.entries || []
    entries.sort((a, b) => {
      if (a.is_dir !== b.is_dir) return a.is_dir ? -1 : 1
      return String(a.name || '').localeCompare(String(b.name || ''))
    })
    elPath.value = dir
    selected = null
    renderCrumbs(dir)
    renderTable()
    log(`目录已加载: ${dir}`)
  }

  async function loadPreview() {
    if (!selected || selected.is_dir) {
      throw new Error('请先选择一个文件')
    }
    const filePath = joinPath(elPath.value, selected.name)
    const data = await rpc({ action: 'read', path: filePath }, ['read_result', 'error'])
    if (data.action === 'error') {
      throw new Error(data.message || '读取失败')
    }
    elPreview.value = data.data || ''
    log(`已读取文件: ${filePath}`)
  }

  async function savePreview() {
    if (!selected || selected.is_dir) {
      throw new Error('请先选择一个文件')
    }
    const filePath = joinPath(elPath.value, selected.name)
    const data = await rpc(
      { action: 'write', path: filePath, data: elPreview.value || '' },
      ['success', 'error']
    )
    if (data.action === 'error') {
      throw new Error(data.message || '保存失败')
    }
    log(`已保存文件: ${filePath}`)
  }

  async function createDir() {
    const name = window.prompt('输入新目录名称')
    if (!name) return
    const path = joinPath(elPath.value, name)
    const data = await rpc({ action: 'mkdir', path }, ['success', 'error'])
    if (data.action === 'error') {
      throw new Error(data.message || '新建目录失败')
    }
    await listDir(elPath.value)
  }

  async function renameEntry() {
    if (!selected) {
      throw new Error('请先选择文件或目录')
    }
    const next = window.prompt('输入新名称', selected.name)
    if (!next || next === selected.name) return
    const from = joinPath(elPath.value, selected.name)
    const to = joinPath(elPath.value, next)
    const data = await rpc({ action: 'rename', from, to }, ['success', 'error'])
    if (data.action === 'error') {
      throw new Error(data.message || '重命名失败')
    }
    await listDir(elPath.value)
  }

  async function deleteEntry() {
    if (!selected) {
      throw new Error('请先选择文件或目录')
    }
    const ok = window.confirm(`确认删除 ${selected.name} ?`)
    if (!ok) return
    const path = joinPath(elPath.value, selected.name)
    const data = await rpc({ action: 'delete', path }, ['success', 'error'])
    if (data.action === 'error') {
      throw new Error(data.message || '删除失败')
    }
    await listDir(elPath.value)
  }

  async function downloadEntry() {
    if (!selected || selected.is_dir) {
      throw new Error('请先选择要下载的文件')
    }
    const path = joinPath(elPath.value, selected.name)
    const data = await rpc({ action: 'download', path }, ['download_result', 'error'], 15000)
    if (data.action === 'error') {
      throw new Error(data.message || '下载失败')
    }

    const b64 = data.data_b64 || ''
    const binary = atob(b64)
    const bytes = new Uint8Array(binary.length)
    for (let i = 0; i < binary.length; i++) {
      bytes[i] = binary.charCodeAt(i)
    }

    const blob = new Blob([bytes])
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = data.filename || selected.name
    document.body.appendChild(a)
    a.click()
    a.remove()
    URL.revokeObjectURL(url)
    log(`下载完成: ${a.download}`)
  }

  async function uploadEntry(file) {
    const target = normalizePath(joinPath(elPath.value, file.name))
    const buffer = await file.arrayBuffer()
    const bytes = new Uint8Array(buffer)
    let binary = ''
    for (const b of bytes) {
      binary += String.fromCharCode(b)
    }
    const dataB64 = btoa(binary)

    const data = await rpc(
      { action: 'upload', path: target, data_b64: dataB64 },
      ['success', 'error'],
      20000
    )
    if (data.action === 'error') {
      throw new Error(data.message || '上传失败')
    }
    log(`上传完成: ${file.name}`)
    await listDir(elPath.value)
  }

  function openTerminalAtPath() {
    const path = normalizePath(elPath.value)
    localStorage.setItem('tea_ssh_init_command', `cd "${path.replace(/"/g, '\\"')}"`)
    const target = `/plugin/ssh?cwd=${encodeURIComponent(path)}`
    window.location.assign(target)
  }

  const onRefresh = () => Promise.all([refreshStatus(), listDir(elPath.value)]).catch((e) => log(`刷新失败: ${e.message}`))
  const onGo = () => listDir(elPath.value).catch((e) => log(`进入失败: ${e.message}`))
  const onUp = () => listDir(parentPath(elPath.value)).catch((e) => log(`进入失败: ${e.message}`))
  const onCreateDir = () => createDir().catch((e) => log(e.message))
  const onRename = () => renameEntry().catch((e) => log(e.message))
  const onDelete = () => deleteEntry().catch((e) => log(e.message))
  const onLoadPreview = () => loadPreview().catch((e) => log(e.message))
  const onSavePreview = () => savePreview().catch((e) => log(e.message))
  const onDownload = () => downloadEntry().catch((e) => log(e.message))
  const onStartLocal = () => post(`${apiBase}/plugins/${pluginName}/start`).then(refreshStatus).catch((e) => log(`启动失败: ${e.message}`))
  const onStopLocal = () => post(`${apiBase}/plugins/${pluginName}/stop`).then(refreshStatus).catch((e) => log(`停止失败: ${e.message}`))
  const onStartRemote = () => post(`${apiBase}/clients/${elNode.value}/plugins/${pluginName}/start`).then(refreshStatus).catch((e) => log(`启动失败: ${e.message}`))
  const onStopRemote = () => post(`${apiBase}/clients/${elNode.value}/plugins/${pluginName}/stop`).then(refreshStatus).catch((e) => log(`停止失败: ${e.message}`))
  const onUploadClick = () => elUploadInput.click()
  const onUploadInput = () => {
    const file = elUploadInput.files?.[0]
    if (!file) return
    uploadEntry(file).catch((e) => log(e.message)).finally(() => {
      elUploadInput.value = ''
    })
  }
  const onOpenTerminal = () => openTerminalAtPath()

  container.querySelector('#fm-refresh').addEventListener('click', onRefresh)
  container.querySelector('#fm-go').addEventListener('click', onGo)
  container.querySelector('#fm-up').addEventListener('click', onUp)
  container.querySelector('#fm-new-folder').addEventListener('click', onCreateDir)
  container.querySelector('#fm-rename').addEventListener('click', onRename)
  container.querySelector('#fm-delete').addEventListener('click', onDelete)
  container.querySelector('#fm-load-preview').addEventListener('click', onLoadPreview)
  container.querySelector('#fm-save-preview').addEventListener('click', onSavePreview)
  container.querySelector('#fm-download').addEventListener('click', onDownload)
  container.querySelector('#fm-upload').addEventListener('click', onUploadClick)
  container.querySelector('#fm-start-local').addEventListener('click', onStartLocal)
  container.querySelector('#fm-stop-local').addEventListener('click', onStopLocal)
  container.querySelector('#fm-start-remote').addEventListener('click', onStartRemote)
  container.querySelector('#fm-stop-remote').addEventListener('click', onStopRemote)
  container.querySelector('#fm-open-terminal').addEventListener('click', onOpenTerminal)
  elUploadInput.addEventListener('change', onUploadInput)

  await refreshStatus().catch((e) => log(`状态刷新失败: ${e.message}`))
  await listDir('/').catch((e) => log(`目录初始化失败: ${e.message}`))
  statusTimer = setInterval(() => {
    if (stop) return
    refreshStatus().catch(() => {})
  }, 3000)

  return () => {
    stop = true
    clearInterval(statusTimer)
    container.querySelector('#fm-refresh')?.removeEventListener('click', onRefresh)
    container.querySelector('#fm-go')?.removeEventListener('click', onGo)
    container.querySelector('#fm-up')?.removeEventListener('click', onUp)
    container.querySelector('#fm-new-folder')?.removeEventListener('click', onCreateDir)
    container.querySelector('#fm-rename')?.removeEventListener('click', onRename)
    container.querySelector('#fm-delete')?.removeEventListener('click', onDelete)
    container.querySelector('#fm-load-preview')?.removeEventListener('click', onLoadPreview)
    container.querySelector('#fm-save-preview')?.removeEventListener('click', onSavePreview)
    container.querySelector('#fm-download')?.removeEventListener('click', onDownload)
    container.querySelector('#fm-upload')?.removeEventListener('click', onUploadClick)
    container.querySelector('#fm-start-local')?.removeEventListener('click', onStartLocal)
    container.querySelector('#fm-stop-local')?.removeEventListener('click', onStopLocal)
    container.querySelector('#fm-start-remote')?.removeEventListener('click', onStartRemote)
    container.querySelector('#fm-stop-remote')?.removeEventListener('click', onStopRemote)
    container.querySelector('#fm-open-terminal')?.removeEventListener('click', onOpenTerminal)
    elUploadInput?.removeEventListener('change', onUploadInput)
    styleEl.remove()
    container.innerHTML = ''
  }
}
