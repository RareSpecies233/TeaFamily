<template>
  <div class="ssh-view">
    <div class="toolbar">
      <el-select v-model="targetNode" placeholder="选择目标客户端" style="width: 200px">
        <el-option
          v-for="c in clients"
          :key="c.node_id"
          :label="`${c.node_id} (${c.address})`"
          :value="c.node_id"
        />
      </el-select>
      <el-button type="primary" @click="connect" :disabled="!targetNode || connected">
        连接
      </el-button>
      <el-button type="danger" @click="disconnect" :disabled="!connected">
        断开
      </el-button>
    </div>
    <div ref="terminalRef" class="terminal-container" />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, nextTick } from 'vue'
import { Terminal } from '@xterm/xterm'
import { FitAddon } from '@xterm/addon-fit'
import '@xterm/xterm/css/xterm.css'

const props = defineProps<{
  pluginName: string
  apiBase: string
}>()

const terminalRef = ref<HTMLElement>()
const targetNode = ref('')
const connected = ref(false)
const clients = ref<any[]>([])
const sessionId = ref('')

let term: Terminal | null = null
let fitAddon: FitAddon | null = null
let pollTimer: ReturnType<typeof setInterval> | null = null

async function fetchClients() {
  try {
    const resp = await fetch(`${props.apiBase}/clients`)
    const data = await resp.json()
    clients.value = data.clients || []
  } catch (e) {
    console.error('Failed to fetch clients:', e)
  }
}

function initTerminal() {
  if (!terminalRef.value || term) return
  term = new Terminal({
    cursorBlink: true,
    fontSize: 14,
    fontFamily: "'Menlo', 'Monaco', 'Courier New', monospace",
    theme: {
      background: '#1e1e1e',
      foreground: '#d4d4d4',
    },
  })
  fitAddon = new FitAddon()
  term.loadAddon(fitAddon)
  term.open(terminalRef.value)
  fitAddon.fit()

  term.onData((data: string) => {
    if (connected.value && sessionId.value) {
      fetch(`${props.apiBase}/plugins/ssh/input`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          session_id: sessionId.value,
          data,
        }),
      }).catch(console.error)
    }
  })

  term.onResize(({ cols, rows }: { cols: number; rows: number }) => {
    if (connected.value && sessionId.value) {
      fetch(`${props.apiBase}/plugins/ssh/resize`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          session_id: sessionId.value,
          cols,
          rows,
        }),
      }).catch(console.error)
    }
  })
}

async function connect() {
  if (!targetNode.value) return
  const sid = `ssh-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`
  sessionId.value = sid

  try {
    await fetch(`${props.apiBase}/plugins/ssh/session`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        session_id: sid,
        target_node: targetNode.value,
        cols: term?.cols || 80,
        rows: term?.rows || 24,
      }),
    })
    connected.value = true
    term?.clear()
    term?.focus()

    // Poll for output
    pollTimer = setInterval(pollOutput, 100)
  } catch (e) {
    console.error('Connect failed:', e)
  }
}

async function pollOutput() {
  if (!sessionId.value) return
  try {
    const resp = await fetch(
      `${props.apiBase}/plugins/ssh/output?session_id=${sessionId.value}`
    )
    const data = await resp.json()
    if (data.data) {
      term?.write(data.data)
    }
  } catch {
    // ignore poll errors
  }
}

function disconnect() {
  if (pollTimer) {
    clearInterval(pollTimer)
    pollTimer = null
  }
  if (sessionId.value) {
    fetch(`${props.apiBase}/plugins/ssh/session`, {
      method: 'DELETE',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ session_id: sessionId.value }),
    }).catch(console.error)
  }
  connected.value = false
  sessionId.value = ''
  term?.write('\r\n\x1b[33m--- 连接已断开 ---\x1b[0m\r\n')
}

onMounted(async () => {
  await fetchClients()
  await nextTick()
  initTerminal()

  window.addEventListener('resize', () => fitAddon?.fit())
})

onUnmounted(() => {
  disconnect()
  term?.dispose()
  term = null
})
</script>

<style scoped>
.ssh-view {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.toolbar {
  display: flex;
  gap: 8px;
  margin-bottom: 12px;
  align-items: center;
}

.terminal-container {
  flex: 1;
  min-height: 400px;
  background: #1e1e1e;
  border-radius: 4px;
  padding: 4px;
}
</style>
