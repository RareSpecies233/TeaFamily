<template>
  <div class="sidebar" :class="{ collapsed }">
    <div class="sidebar-header">
      <span v-if="!collapsed" class="sidebar-logo">🍊</span>
      <div v-if="!collapsed" class="brand">
        <span class="sidebar-title">OrangeTea</span>
        <span class="sidebar-subtitle">Plugin Console</span>
      </div>
      <el-button
        class="collapse-toggle"
        text
        :aria-label="collapsed ? '展开导航栏' : '折叠导航栏'"
        @click="toggleCollapsed"
      >
        <el-icon>
          <Expand v-if="collapsed" />
          <Fold v-else />
        </el-icon>
      </el-button>
    </div>

    <el-menu
      :default-active="activeMenuIndex"
      @select="handleMenuSelect"
      class="sidebar-menu"
      background-color="transparent"
      text-color="#6f6a5e"
      active-text-color="#1b1407"
    >
      <el-menu-item index="plugins" class="static-menu-item">
        <el-icon><Box /></el-icon>
        <span v-if="!collapsed">插件管理</span>
      </el-menu-item>

      <el-menu-item index="update" class="static-menu-item">
        <el-icon><Upload /></el-icon>
        <span v-if="!collapsed">版本更新</span>
      </el-menu-item>

      <el-menu-item
        v-for="plugin in pluginPageEntries"
        :key="plugin.name"
        :index="`plugin:${plugin.name}`"
        :data-plugin-name="plugin.name"
        class="dynamic-menu-item"
      >
        <a
          class="plugin-link"
          :href="pluginLinkHref(plugin.name)"
          @click.stop.prevent="handlePluginLinkClick(plugin.name, $event)"
        >
          <el-icon v-if="!collapsed"><Grid /></el-icon>
          <span class="dynamic-menu-label" :class="{ 'plugin-mini-text': collapsed }">
            {{ collapsed ? shortPluginLabel(plugin.title || plugin.name) : plugin.title || plugin.name }}
          </span>
        </a>
      </el-menu-item>

      <el-menu-item
        v-if="!pluginPageEntries.length"
        index="plugin:__empty"
        disabled
        class="dynamic-menu-item"
      >
        <el-icon v-if="!collapsed"><Grid /></el-icon>
        <span class="dynamic-menu-label" :class="{ 'plugin-mini-text': collapsed }">
          {{ collapsed ? '暂无' : '暂无可用页面（请先安装）' }}
        </span>
      </el-menu-item>
    </el-menu>

    <div class="sidebar-footer">
      <div class="window-mode">
        <el-tooltip
          v-if="collapsed"
          :content="`多窗口模式：${multiWindowMode ? '已开启' : '已关闭'}`"
          placement="right"
        >
          <el-switch
            v-model="multiWindowMode"
            size="small"
            inline-prompt
            active-text="开"
            inactive-text="关"
          />
        </el-tooltip>

        <template v-else>
          <div class="window-mode-row">
            <span class="window-mode-label">多窗口模式</span>
            <el-switch
              v-model="multiWindowMode"
              inline-prompt
              active-text="开"
              inactive-text="关"
            />
          </div>
          <p class="window-mode-hint">开启后插件界面会在新窗口打开（无导航栏）</p>
        </template>
      </div>

      <el-tooltip
        v-if="collapsed"
        :content="connectionTooltip"
        placement="right"
      >
        <button type="button" class="connection-mini" @click="openConfig">
          <div class="mini-status-group">
            <span class="mini-status-pill" title="LemonTea">
              <span class="mini-status-dot" :class="connectionStore.connected ? 'online' : 'offline'" />
              <span class="mini-status-text">L</span>
            </span>
            <span class="mini-status-pill" title="HoneyTea">
              <span class="mini-status-dot" :class="honeyOnline ? 'online' : 'offline'" />
              <span class="mini-status-text">H</span>
            </span>
          </div>
        </button>
      </el-tooltip>

      <template v-else>
        <div class="connection-card" @click="openConfig">
          <div class="connection-row">
            <div class="connection-top">
              <span class="connection-label">LemonTea</span>
              <el-tag :type="lemonStatusType" size="small">{{ lemonStatusLabel }}</el-tag>
            </div>
            <p class="server-url">{{ displayServerUrl }}</p>
          </div>

          <div class="connection-divider" />

          <div class="connection-row">
            <div class="connection-top">
              <span class="connection-label">HoneyTea</span>
              <el-tag :type="honeyStatusType" size="small">{{ honeyStatusLabel }}</el-tag>
            </div>
            <p class="server-url">{{ honeySummaryText }}</p>
          </div>

          <p v-if="connectionStore.error" class="connection-error">{{ connectionStore.error }}</p>
        </div>

        <div class="connection-actions">
          <el-button
            class="action-button action-button--primary"
            size="small"
            :loading="connectionStore.connecting"
            @click="quickConnect"
          >
            <el-icon><RefreshRight /></el-icon>
            {{ connectionStore.connected ? '重新连接' : '立即连接' }}
          </el-button>
          <el-button
            class="action-button action-button--danger"
            size="small"
            :disabled="!connectionStore.connected"
            @click="handleDisconnect"
          >
            <el-icon><SwitchButton /></el-icon>
            断开连接
          </el-button>
        </div>
      </template>
    </div>

    <el-dialog v-model="configVisible" title="连接配置" width="520px">
      <el-form label-position="top">
        <el-form-item label="LemonTea HTTP 地址">
          <el-input
            v-model="serverAddress"
            placeholder="例如：127.0.0.1:9528"
            :disabled="connectionStore.connecting"
          />
        </el-form-item>

        <el-form-item label="HoneyTea 目标节点">
          <el-select
            v-model="honeyNodeId"
            clearable
            filterable
            placeholder="自动选择首个在线节点"
            :disabled="!connectionStore.connected || connectionStore.connecting || connectionStore.refreshingHoneyClients"
            style="width: 100%"
          >
            <el-option
              v-for="client in connectionStore.honeyClients"
              :key="client.node_id"
              :label="formatHoneyOption(client)"
              :value="client.node_id"
            />
          </el-select>
          <p class="form-hint">用于插件管理和版本更新的默认 HoneyTea 目标节点。</p>
        </el-form-item>

        <el-alert
          v-if="connectionStore.connected && !connectionStore.honeyClients.length"
          title="尚未检测到 HoneyTea 节点，确认 HoneyTea 已连接到当前 LemonTea。"
          type="info"
          :closable="false"
          style="margin-bottom: 10px"
        />

        <el-alert
          v-if="connectionStore.error"
          :title="connectionStore.error"
          type="error"
          :closable="false"
        />
      </el-form>

      <template #footer>
        <el-button @click="configVisible = false">取消</el-button>
        <el-button
          :loading="connectionStore.refreshingHoneyClients"
          :disabled="!connectionStore.connected || connectionStore.connecting"
          @click="refreshHoneyList"
        >
          刷新 HoneyTea
        </el-button>
        <el-button type="primary" :loading="connectionStore.connecting" @click="saveAndConnect">
          保存并连接
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useConnectionStore } from '@/stores/connection'
import { usePluginStore } from '@/stores/plugins'

const route = useRoute()
const router = useRouter()
const connectionStore = useConnectionStore()
const pluginStore = usePluginStore()
const collapsed = ref(localStorage.getItem('tea_sidebar_collapsed') === '1')
const multiWindowMode = ref(localStorage.getItem('tea_plugin_multi_window') === '1')
const configVisible = ref(false)
const serverAddress = ref(connectionStore.serverUrl || 'http://127.0.0.1:9528')
const honeyNodeId = ref(connectionStore.selectedHoneyNodeId || '')
let honeyPollTimer: number | null = null

const lemonStatusType = computed(() => (connectionStore.connected ? 'success' : 'info'))
const lemonStatusLabel = computed(() => (connectionStore.connected ? '在线' : '离线'))
const displayServerUrl = computed(() => connectionStore.serverUrl || '未设置服务器地址')
const honeyOnline = computed(() => connectionStore.honeyConnectedCount > 0)
const honeyStatusType = computed(() => (honeyOnline.value ? 'success' : 'info'))
const honeyStatusLabel = computed(() => {
  const selected = connectionStore.selectedHoneyClient
  if (!connectionStore.connected) return '未连接'
  if (connectionStore.refreshingHoneyClients && !selected) return '刷新中'
  if (selected) {
    return selected.connected ? '在线' : '离线'
  }
  if (connectionStore.honeyConnectedCount > 0) {
    return `在线 ${connectionStore.honeyConnectedCount} 个`
  }
  return '离线'
})
const honeySummaryText = computed(() => {
  const selected = connectionStore.selectedHoneyClient
  if (selected) {
    const status = selected.connected ? '在线' : '离线'
    return `${selected.node_id} · ${selected.address}:${selected.port}（${status}）`
  }
  if (!connectionStore.connected) {
    return '需先连接 LemonTea'
  }
  if (connectionStore.honeyConnectedCount > 0) {
    return `已连接 ${connectionStore.honeyConnectedCount} 个节点，未指定默认节点`
  }
  return '未检测到 HoneyTea'
})
const connectionTooltip = computed(
  () => `LemonTea：${lemonStatusLabel.value} ｜ HoneyTea：${honeyStatusLabel.value}（点击配置）`
)
const activeMenuIndex = computed(() => {
  if (route.path === '/plugins') return 'plugins'
  if (route.path === '/update') return 'update'

  const pluginMatch = route.path.match(/^\/plugin\/(.+)$/)
  if (pluginMatch?.[1]) {
    return `plugin:${decodeURIComponent(pluginMatch[1])}`
  }

  return ''
})

const pluginPageEntries = computed(() => {
  const entries = new Map<string, { name: string; title: string }>()

  for (const frontend of pluginStore.frontendPlugins) {
    entries.set(frontend.name, {
      name: frontend.name,
      title: frontend.title || frontend.name,
    })
  }

  for (const local of pluginStore.localPlugins) {
    if (!local.has_frontend || entries.has(local.name)) continue
    entries.set(local.name, {
      name: local.name,
      title: local.name,
    })
  }

  return Array.from(entries.values()).sort((a, b) => a.name.localeCompare(b.name))
})

async function syncPluginEntries() {
  if (!connectionStore.connected) return
  await Promise.allSettled([pluginStore.fetchPlugins(), pluginStore.fetchFrontendPlugins()])
}

function stopHoneyPolling() {
  if (honeyPollTimer !== null) {
    window.clearInterval(honeyPollTimer)
    honeyPollTimer = null
  }
}

function startHoneyPolling() {
  stopHoneyPolling()
  if (!connectionStore.connected) {
    return
  }
  honeyPollTimer = window.setInterval(() => {
    connectionStore.refreshHoneyClients()
  }, 5000)
}

onMounted(() => {
  syncPluginEntries()
  if (connectionStore.connected) {
    connectionStore.refreshHoneyClients()
  }
  startHoneyPolling()
})

onBeforeUnmount(() => {
  stopHoneyPolling()
})

watch(
  () => connectionStore.connected,
  (connected) => {
    if (connected) {
      connectionStore.refreshHoneyClients()
      startHoneyPolling()
      syncPluginEntries()
      return
    }
    stopHoneyPolling()
    pluginStore.localPlugins = []
    pluginStore.frontendPlugins = []
  }
)

watch(
  () => connectionStore.serverUrl,
  (value) => {
    if (!configVisible.value) {
      serverAddress.value = value || 'http://127.0.0.1:9528'
    }
  }
)

watch(
  () => connectionStore.selectedHoneyNodeId,
  (value) => {
    if (!configVisible.value) {
      honeyNodeId.value = value || ''
    }
  }
)

watch(collapsed, (value) => {
  localStorage.setItem('tea_sidebar_collapsed', value ? '1' : '0')
})

watch(multiWindowMode, (value) => {
  localStorage.setItem('tea_plugin_multi_window', value ? '1' : '0')
})

function toggleCollapsed() {
  collapsed.value = !collapsed.value
}

function openPluginPage(
  name: string,
  options: { forceStandalone?: boolean } = {}
) {
  if (!name || name === '__empty') return

  if (multiWindowMode.value || options.forceStandalone) {
    const to = router.resolve({
      name: 'plugin-window',
      params: { name },
    })
    window.open(to.href, '_blank')
    return
  }

  router.push({
    name: 'plugin-detail',
    params: { name },
  })
}

function handleMenuSelect(index: string) {
  if (index === 'plugins') {
    router.push('/plugins')
    return
  }
  if (index === 'update') {
    router.push('/update')
    return
  }
}

function pluginLinkHref(name: string) {
  if (multiWindowMode.value) {
    return router.resolve({
      name: 'plugin-window',
      params: { name },
    }).href
  }

  return router.resolve({
    name: 'plugin-detail',
    params: { name },
  }).href
}

function handlePluginLinkClick(name: string, event: MouseEvent) {
  if (event.button !== 0) return
  openPluginPage(name)
}

function handleDisconnect() {
  connectionStore.disconnect()
}

async function openConfig() {
  serverAddress.value = connectionStore.serverUrl || serverAddress.value
  honeyNodeId.value = connectionStore.selectedHoneyNodeId || ''
  if (connectionStore.connected) {
    await connectionStore.refreshHoneyClients()
  }
  configVisible.value = true
}

async function quickConnect() {
  connectionStore.setSelectedHoneyNodeId(honeyNodeId.value)
  const ok = await connectionStore.connect(serverAddress.value)
  if (ok) {
    await connectionStore.refreshHoneyClients()
    await syncPluginEntries()
  }
}

async function refreshHoneyList() {
  await connectionStore.refreshHoneyClients()
}

async function saveAndConnect() {
  connectionStore.setSelectedHoneyNodeId(honeyNodeId.value)
  const ok = await connectionStore.connect(serverAddress.value)
  if (!ok) return
  await connectionStore.refreshHoneyClients()
  await syncPluginEntries()
  configVisible.value = false
}

function formatHoneyOption(client: {
  node_id: string
  address: string
  port: number
  connected: boolean
}) {
  return `${client.node_id} (${client.address}:${client.port}) ${client.connected ? '在线' : '离线'}`
}

function shortPluginLabel(value: string) {
  const text = value.trim()
  if (!text) return '--'
  const chineseChars = text.match(/[\u4e00-\u9fff]/g)
  if (chineseChars && chineseChars.length > 0) {
    return chineseChars.slice(0, 2).join('')
  }
  return Array.from(text).slice(0, 2).join('')
}
</script>

<style scoped>
.sidebar {
  width: var(--tea-sidebar-width);
  min-width: var(--tea-sidebar-width);
  flex: 0 0 var(--tea-sidebar-width);
  background:
    radial-gradient(circle at 0 0, rgba(255, 196, 111, 0.35), transparent 35%),
    radial-gradient(circle at 100% 100%, rgba(146, 223, 207, 0.3), transparent 38%),
    linear-gradient(180deg, #f7efe2 0%, #f2e5d4 100%);
  display: flex;
  flex-direction: column;
  height: 100vh;
  overflow: hidden;
  border-right: 1px solid #e6d7c0;
  transition: width 220ms ease, min-width 220ms ease;
}

.sidebar.collapsed {
  width: var(--tea-sidebar-collapsed-width);
  min-width: var(--tea-sidebar-collapsed-width);
  flex-basis: var(--tea-sidebar-collapsed-width);
}

.sidebar-header {
  padding: 22px 18px;
  display: flex;
  align-items: center;
  gap: 10px;
  min-height: 78px;
  border-bottom: 1px solid #e6d7c0;
  position: relative;
}

.collapse-toggle {
  margin-left: auto;
  color: #6f5c3f;
  width: 30px !important;
  min-width: 30px !important;
  height: 30px !important;
  padding: 0 !important;
  border-radius: 9px;
  flex-shrink: 0;
}

.sidebar.collapsed .sidebar-header {
  justify-content: flex-end;
  min-height: 48px;
  padding: 8px 10px;
}

.sidebar.collapsed .collapse-toggle {
  margin-left: 0;
  width: 28px !important;
  min-width: 28px !important;
  height: 28px !important;
}

.sidebar-logo {
  width: 38px;
  height: 38px;
  min-width: 38px;
  flex-shrink: 0;
  border-radius: 11px;
  display: grid;
  place-items: center;
  background: #1b1407;
  color: #ffdd8a;
  font-size: 13px;
  font-weight: 700;
  letter-spacing: 0.6px;
}

.sidebar-title {
  color: #2f2411;
  font-size: 18px;
  font-weight: 700;
}

.sidebar-subtitle {
  color: #846d47;
  font-size: 12px;
}

.brand {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.el-menu {
  border-right: none;
  flex: 1;
  padding: 10px;
  overflow-x: hidden;
}

:deep(.el-menu-item), :deep(.el-sub-menu__title) {
  border-radius: 10px;
  margin-bottom: 6px;
}

:deep(.el-menu-item.is-active) {
  background: linear-gradient(90deg, rgba(255, 213, 139, 0.95), rgba(255, 232, 188, 0.95));
  box-shadow: 0 8px 20px rgba(102, 77, 31, 0.15);
}

:deep(.static-menu-item) {
  justify-content: flex-start;
}

:deep(.dynamic-menu-item) {
  justify-content: flex-start;
}

.plugin-link {
  display: flex;
  align-items: center;
  gap: 8px;
  width: 100%;
  min-width: 0;
  color: inherit;
  text-decoration: none;
}

.sidebar.collapsed .plugin-link {
  justify-content: center;
}

.plugin-link .el-icon {
  margin-right: 0;
}

.plugin-mini-text {
  display: block;
  width: 100%;
  text-align: center;
  letter-spacing: 0.6px;
  font-weight: 700;
}

.dynamic-menu-label {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
}

.sidebar.collapsed :deep(.static-menu-item) {
  justify-content: center;
  padding-left: 0 !important;
}

.sidebar.collapsed :deep(.static-menu-item .el-icon) {
  margin-right: 0;
}

.sidebar.collapsed :deep(.dynamic-menu-item) {
  justify-content: center;
  padding-left: 0 !important;
  padding-right: 0 !important;
}

.sidebar-footer {
  padding: 12px;
  border-top: 1px solid #e6d7c0;
}

.sidebar.collapsed .sidebar-footer {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  padding: 10px 8px 14px;
}

.window-mode {
  margin-bottom: 10px;
  border: 1px solid #dfcfb3;
  border-radius: 12px;
  padding: 9px 10px;
  background: rgba(255, 252, 245, 0.78);
}

.sidebar.collapsed .window-mode {
  margin: 0;
  padding: 7px 8px;
  width: 100%;
  display: flex;
  justify-content: center;
}

.window-mode-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
}

.window-mode-label {
  font-size: 12px;
  color: #7a6645;
  font-weight: 600;
}

.window-mode-hint {
  margin-top: 6px;
  color: #8b7858;
  font-size: 11px;
  line-height: 1.4;
}

.connection-card {
  border: 1px solid #dfcfb3;
  border-radius: 12px;
  padding: 10px;
  cursor: pointer;
  background: rgba(255, 252, 245, 0.78);
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.connection-row {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.connection-divider {
  height: 1px;
  background: #e8ddcb;
}

.connection-top {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}

.connection-label {
  font-size: 12px;
  color: #7a6645;
}

.server-url {
  margin-top: 8px;
  font-size: 12px;
  color: #4a3a1f;
  word-break: break-all;
}

.connection-error {
  margin-top: 6px;
  font-size: 12px;
  color: #c43b45;
}

.connection-actions {
  margin-top: 10px;
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 8px;
  align-items: stretch;
}

.action-button {
  width: 100%;
  min-width: 0;
  height: 34px !important;
  padding: 0 10px !important;
  margin: 0;
  border-radius: 10px;
  border: 1px solid #dfcfb4;
  background: rgba(255, 255, 255, 0.82);
  color: #5a4725;
  font-weight: 600;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.action-button:hover,
.action-button:focus-visible {
  background: #fffdf8;
  border-color: #d4be96;
}

.action-button--primary {
  color: #8e5a07;
}

.action-button--danger {
  color: #b1464a;
}

.connection-mini {
  width: 46px;
  min-width: 46px;
  height: 46px;
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid #dfcfb3;
  border-radius: 14px;
  background: rgba(255, 252, 245, 0.82);
  padding: 0;
  cursor: pointer;
  margin: 0 auto;
  flex-shrink: 0;
}

.connection-mini:hover {
  border-color: #d6c29e;
  background: #fffdf7;
}

.mini-status-group {
  display: grid;
  grid-template-rows: repeat(2, minmax(0, 1fr));
  gap: 4px;
}

.mini-status-pill {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 4px;
}

.mini-status-dot {
  width: 7px;
  height: 7px;
  border-radius: 999px;
}

.mini-status-dot.online {
  background: #389d6f;
}

.mini-status-dot.offline {
  background: #b58f4f;
}

.mini-status-text {
  font-size: 11px;
  color: #6d5937;
  font-weight: 700;
  line-height: 1;
}

.form-hint {
  margin: 6px 0 0;
  color: #8c7757;
  font-size: 12px;
}
</style>
