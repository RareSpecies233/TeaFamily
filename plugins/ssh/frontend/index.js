export async function mount(container, ctx) {
  const pluginName = ctx?.pluginName || 'ssh'
  const apiBase = (ctx?.apiBase || '/api').replace(/\/+$/, '')

  container.innerHTML = `
    <section class="ssh-panel">
      <header class="ssh-header">
        <div>
          <h2>Web SSH</h2>
          <p>基于 xterm 的实时终端，支持键盘输入、窗口缩放、滚屏与粘贴。</p>
        </div>
        <div class="header-actions">
          <button id="ssh-refresh" class="ghost">刷新状态</button>
          <button id="ssh-clear" class="ghost">清屏</button>
        </div>
      </header>

      <section class="ssh-controls card">
        <div class="control-grid">
          <label>
            HoneyTea 节点
            <select id="ssh-node"></select>
          </label>
          <label>
            会话状态
            <span id="ssh-session-state" class="badge">未连接</span>
          </label>
          <label>
            LemonTea 插件
            <span id="ssh-local-state" class="badge">-</span>
          </label>
          <label>
            HoneyTea 插件
            <span id="ssh-remote-state" class="badge">-</span>
          </label>
        </div>
        <div class="btn-row">
          <button id="ssh-start-local">启动 LemonTea 插件</button>
          <button id="ssh-stop-local" class="warn">停止 LemonTea 插件</button>
          <button id="ssh-start-remote">启动 HoneyTea 插件</button>
          <button id="ssh-stop-remote" class="warn">停止 HoneyTea 插件</button>
          <button id="ssh-open" class="accent">连接终端</button>
          <button id="ssh-close" class="warn">断开终端</button>
        </div>
      </section>

      <section class="card terminal-wrap">
        <div id="ssh-terminal"></div>
      </section>
    </section>
  `

  const styleEl = document.createElement('style')
  styleEl.textContent = `
    .ssh-panel { font-family: Manrope, 'Noto Sans SC', sans-serif; color: #2a2012; }
    .ssh-header { display:flex; justify-content:space-between; gap:16px; align-items:flex-start; margin-bottom:12px; }
    .ssh-header h2 { margin:0; font-size:22px; }
    .ssh-header p { margin:6px 0 0; color:#725b34; }
    .header-actions { display:flex; gap:8px; }
    .card { border:1px solid #e2d2b4; border-radius:14px; padding:12px; background:linear-gradient(135deg,#fff9ed,#f7fbff); box-shadow:0 10px 20px rgba(113,83,34,.08); }
    .ssh-controls { margin-bottom:12px; }
    .control-grid { display:grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap:10px; margin-bottom:10px; }
    label { display:flex; flex-direction:column; gap:6px; color:#6e5834; font-size:12px; }
    select { padding:8px 10px; border-radius:8px; border:1px solid #d9c8a8; background:#fff; }
    .badge { display:inline-flex; align-items:center; min-height:34px; padding:0 10px; border-radius:8px; background:#f2e9d8; color:#5f4828; font-size:13px; }
    .btn-row { display:flex; flex-wrap:wrap; gap:8px; }
    button { border:0; border-radius:9px; padding:9px 12px; cursor:pointer; background:#2f8f6d; color:#fff; font-weight:600; min-width:132px; text-align:center; }
    button.warn { background:#b84f4f; }
    button.accent { background:#2f5eb8; }
    button.ghost { background:#e9f0f8; color:#2b4a73; min-width:92px; }
    .terminal-wrap { padding:8px; background:#111317; border-color:#2a2e36; }
    #ssh-terminal { height:58vh; min-height:360px; }
    #ssh-terminal .xterm-viewport { border-radius:8px; }
  `
  container.prepend(styleEl)

  const elNode = container.querySelector('#ssh-node')
  const elLocalState = container.querySelector('#ssh-local-state')
  const elRemoteState = container.querySelector('#ssh-remote-state')
  const elSessionState = container.querySelector('#ssh-session-state')
  const elTerminal = container.querySelector('#ssh-terminal')

  let stop = false
  let lastSeq = 0
  let activeSession = ''
  let pollTimer = null
  let statusTimer = null
  let terminal = null
  let fitAddon = null
  let removeResize = null

  function setSessionState(text, kind = 'idle') {
    elSessionState.textContent = text
    if (kind === 'ok') {
      elSessionState.style.background = '#d7f2e7'
      elSessionState.style.color = '#1f6d4f'
      return
    }
    if (kind === 'warn') {
      elSessionState.style.background = '#f8e1e1'
      elSessionState.style.color = '#8a2f2f'
      return
    }
    elSessionState.style.background = '#f2e9d8'
    elSessionState.style.color = '#5f4828'
  }

  function terminalWrite(text) {
    if (!terminal) return
    terminal.write(text)
  }

  async function fetchJson(url, options) {
    const resp = await fetch(url, options)
    const data = await resp.json().catch(() => ({}))
    if (!resp.ok) {
      throw new Error(data.error || `${resp.status} ${resp.statusText}`)
    }
    return data
  }

  async function ensureXterm() {
    if (window.__TeaXterm?.Terminal && window.__TeaXterm?.FitAddon) {
      return window.__TeaXterm
    }

    const xtermModule = await import('https://esm.sh/@xterm/xterm@5.4.0')
    const fitModule = await import('https://esm.sh/@xterm/addon-fit@0.9.0')

    const cssId = 'tea-xterm-css'
    if (!document.getElementById(cssId)) {
      const link = document.createElement('link')
      link.id = cssId
      link.rel = 'stylesheet'
      link.href = 'https://cdn.jsdelivr.net/npm/@xterm/xterm@5.4.0/css/xterm.min.css'
      document.head.appendChild(link)
    }

    window.__TeaXterm = {
      Terminal: xtermModule.Terminal,
      FitAddon: fitModule.FitAddon,
    }
    return window.__TeaXterm
  }

  async function initTerminal() {
    const X = await ensureXterm()
    terminal = new X.Terminal({
      cursorBlink: true,
      scrollback: 6000,
      fontSize: 14,
      fontFamily: 'Menlo, Monaco, Consolas, monospace',
      theme: {
        background: '#10141b',
        foreground: '#e6edf3',
        cursor: '#f4c86b',
      },
    })
    fitAddon = new X.FitAddon()
    terminal.loadAddon(fitAddon)
    terminal.open(elTerminal)
    fitAddon.fit()

    terminal.writeln('Welcome to TeaFamily Web SSH')
    terminal.writeln('请选择节点并点击“连接终端”。')

    terminal.onData((data) => {
      if (!activeSession) return
      sendPluginMessage({ action: 'input', session_id: activeSession, data }).catch(() => {})
    })

    const onResize = () => {
      if (!fitAddon || !terminal) return
      fitAddon.fit()
      if (!activeSession) return
      sendPluginMessage({
        action: 'resize',
        session_id: activeSession,
        cols: terminal.cols,
        rows: terminal.rows,
      }).catch(() => {})
    }
    window.addEventListener('resize', onResize)
    removeResize = () => window.removeEventListener('resize', onResize)
  }

  async function post(url) {
    return fetchJson(url, { method: 'POST' })
  }

  async function sendPluginMessage(payload) {
    const nodeId = elNode.value
    if (!nodeId) {
      throw new Error('请先选择 HoneyTea 节点')
    }
    return fetchJson(`${apiBase}/plugin-message`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ node_id: nodeId, plugin: pluginName, data: payload }),
    })
  }

  async function refreshStatus() {
    const [clientsResp, localResp] = await Promise.all([
      fetchJson(`${apiBase}/clients`),
      fetchJson(`${apiBase}/plugins`),
    ])

    const clients = clientsResp.clients || []
    const selected = elNode.value
    elNode.innerHTML = clients
      .filter((item) => item.connected)
      .map((item) => `<option value="${item.node_id}">${item.node_id} (${item.address}:${item.port})</option>`)
      .join('')

    if (selected && clients.some((item) => item.node_id === selected && item.connected)) {
      elNode.value = selected
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

  function buildSessionId() {
    return `ssh-${Date.now()}`
  }

  function readInitialCommand() {
    const url = new URL(window.location.href)
    const cwd = url.searchParams.get('cwd')
    if (cwd) {
      return `cd "${cwd.replace(/"/g, '\\"')}"\n`
    }
    const stored = localStorage.getItem('tea_ssh_init_command') || ''
    if (stored) {
      localStorage.removeItem('tea_ssh_init_command')
      return stored.endsWith('\n') ? stored : `${stored}\n`
    }
    return ''
  }

  async function openSession() {
    if (!terminal || !fitAddon) return
    if (!elNode.value) {
      terminalWrite('\r\n[error] 没有可用的 HoneyTea 节点。\r\n')
      return
    }

    const sessionId = buildSessionId()
    await sendPluginMessage({
      action: 'create_session',
      session_id: sessionId,
      target_node: elNode.value,
      cols: terminal.cols,
      rows: terminal.rows,
    })

    activeSession = sessionId
    setSessionState('已连接', 'ok')
    terminalWrite('\r\n[system] session created\r\n')

    const initialCmd = readInitialCommand()
    if (initialCmd) {
      setTimeout(() => {
        if (!activeSession) return
        sendPluginMessage({ action: 'input', session_id: activeSession, data: initialCmd }).catch(() => {})
      }, 120)
    }
  }

  async function closeSession() {
    if (!activeSession) return
    const sid = activeSession
    activeSession = ''
    setSessionState('已断开', 'warn')
    await sendPluginMessage({ action: 'close_session', session_id: sid }).catch(() => {})
    terminalWrite('\r\n[system] session closed\r\n')
  }

  async function pollEvents() {
    if (stop) return
    try {
      const resp = await fetchJson(`${apiBase}/plugin-events?plugin=${encodeURIComponent(pluginName)}&after=${lastSeq}&limit=300`)
      const events = resp.events || []

      for (const event of events) {
        const data = event.data || {}
        const action = data.action || data.event || ''
        const sid = data.session_id || ''

        if (action === 'output') {
          if (activeSession && sid && sid !== activeSession) continue
          terminalWrite(data.data || '')
        } else if (action === 'session_ended') {
          if (!activeSession || sid === activeSession) {
            activeSession = ''
            setSessionState('会话结束', 'warn')
            terminalWrite('\r\n[system] session ended\r\n')
          }
        } else if (action === 'error') {
          terminalWrite(`\r\n[error] ${data.message || 'unknown error'}\r\n`)
        }

        lastSeq = Math.max(lastSeq, Number(event.seq || lastSeq))
      }

      if (resp.next_seq) {
        lastSeq = Math.max(lastSeq, Number(resp.next_seq))
      }
    } catch {
      // Keep polling; transient network issues should self-recover.
    }
  }

  const onRefresh = () => refreshStatus().catch(() => {})
  const onStartLocal = () => post(`${apiBase}/plugins/${pluginName}/start`).then(onRefresh).catch(() => {})
  const onStopLocal = () => post(`${apiBase}/plugins/${pluginName}/stop`).then(onRefresh).catch(() => {})
  const onStartRemote = () => post(`${apiBase}/clients/${elNode.value}/plugins/${pluginName}/start`).then(onRefresh).catch(() => {})
  const onStopRemote = () => post(`${apiBase}/clients/${elNode.value}/plugins/${pluginName}/stop`).then(onRefresh).catch(() => {})
  const onOpen = () => openSession().catch((e) => terminalWrite(`\r\n[error] ${e.message}\r\n`))
  const onClose = () => closeSession().catch(() => {})
  const onClear = () => terminal?.clear()

  container.querySelector('#ssh-refresh').addEventListener('click', onRefresh)
  container.querySelector('#ssh-start-local').addEventListener('click', onStartLocal)
  container.querySelector('#ssh-stop-local').addEventListener('click', onStopLocal)
  container.querySelector('#ssh-start-remote').addEventListener('click', onStartRemote)
  container.querySelector('#ssh-stop-remote').addEventListener('click', onStopRemote)
  container.querySelector('#ssh-open').addEventListener('click', onOpen)
  container.querySelector('#ssh-close').addEventListener('click', onClose)
  container.querySelector('#ssh-clear').addEventListener('click', onClear)

  await initTerminal()
  await refreshStatus().catch(() => {})
  pollTimer = setInterval(pollEvents, 120)
  statusTimer = setInterval(refreshStatus, 2500)

  return () => {
    stop = true
    clearInterval(pollTimer)
    clearInterval(statusTimer)
    container.querySelector('#ssh-refresh')?.removeEventListener('click', onRefresh)
    container.querySelector('#ssh-start-local')?.removeEventListener('click', onStartLocal)
    container.querySelector('#ssh-stop-local')?.removeEventListener('click', onStopLocal)
    container.querySelector('#ssh-start-remote')?.removeEventListener('click', onStartRemote)
    container.querySelector('#ssh-stop-remote')?.removeEventListener('click', onStopRemote)
    container.querySelector('#ssh-open')?.removeEventListener('click', onOpen)
    container.querySelector('#ssh-close')?.removeEventListener('click', onClose)
    container.querySelector('#ssh-clear')?.removeEventListener('click', onClear)
    if (removeResize) removeResize()
    if (terminal) terminal.dispose()
    styleEl.remove()
    container.innerHTML = ''
  }
}
