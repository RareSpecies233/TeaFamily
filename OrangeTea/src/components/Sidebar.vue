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
      :default-active="route.path"
      router
      class="sidebar-menu"
      background-color="transparent"
      text-color="#6f6a5e"
      active-text-color="#1b1407"
    >
      <el-menu-item index="/plugins" class="static-menu-item">
        <el-icon><Box /></el-icon>
        <span v-if="!collapsed">插件管理</span>
      </el-menu-item>

      <el-menu-item index="/update" class="static-menu-item">
        <el-icon><Upload /></el-icon>
        <span v-if="!collapsed">版本更新</span>
      </el-menu-item>

      <el-menu-item
        v-for="plugin in pluginPageEntries"
        :key="plugin.name"
        :index="`/plugin/${plugin.name}`"
        class="dynamic-menu-item"
      >
        <el-icon><Grid /></el-icon>
        <span :class="{ 'plugin-mini-text': collapsed }">
          {{ collapsed ? shortPluginLabel(plugin.title || plugin.name) : plugin.title || plugin.name }}
        </span>
      </el-menu-item>

      <el-menu-item
        v-if="!pluginPageEntries.length"
        index="/plugins"
        disabled
        class="dynamic-menu-item"
      >
        <el-icon><Grid /></el-icon>
        <span :class="{ 'plugin-mini-text': collapsed }">
          {{ collapsed ? '暂无' : '暂无可用页面（请先安装）' }}
        </span>
      </el-menu-item>
    </el-menu>

    <div class="sidebar-footer">
      <el-tooltip
        v-if="collapsed"
        :content="`LemonTea：${statusLabel}（点击配置）`"
        placement="right"
      >
        <button type="button" class="connection-mini" @click="openConfig">
          <el-icon v-if="connectionStore.connected" class="status-icon online">
            <CircleCheckFilled />
          </el-icon>
          <el-icon v-else class="status-icon offline">
            <CircleCloseFilled />
          </el-icon>
        </button>
      </el-tooltip>

      <template v-else>
        <div class="connection-card" @click="openConfig">
          <div class="connection-top">
            <span class="connection-label">LemonTea 连接</span>
            <el-tag :type="statusType" size="small">{{ statusLabel }}</el-tag>
          </div>
          <p class="server-url">{{ displayServerUrl }}</p>
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

    <el-dialog v-model="configVisible" title="LemonTea 连接配置" width="460px">
      <el-form label-position="top">
        <el-form-item label="服务器地址">
          <el-input
            v-model="serverAddress"
            placeholder="例如：127.0.0.1:9528"
            :disabled="connectionStore.connecting"
          />
        </el-form-item>
        <el-alert
          v-if="connectionStore.error"
          :title="connectionStore.error"
          type="error"
          :closable="false"
        />
      </el-form>

      <template #footer>
        <el-button @click="configVisible = false">取消</el-button>
        <el-button type="primary" :loading="connectionStore.connecting" @click="saveAndConnect">
          保存并连接
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useConnectionStore } from '@/stores/connection'
import { usePluginStore } from '@/stores/plugins'

const route = useRoute()
const connectionStore = useConnectionStore()
const pluginStore = usePluginStore()
const collapsed = ref(localStorage.getItem('tea_sidebar_collapsed') === '1')
const configVisible = ref(false)
const serverAddress = ref(connectionStore.serverUrl || 'http://127.0.0.1:9528')

const statusType = computed(() => (connectionStore.connected ? 'success' : 'info'))
const statusLabel = computed(() => (connectionStore.connected ? '在线' : '离线'))
const displayServerUrl = computed(() => connectionStore.serverUrl || '未设置服务器地址')
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

onMounted(() => {
  syncPluginEntries()
})

watch(
  () => connectionStore.connected,
  (connected) => {
    if (connected) {
      syncPluginEntries()
      return
    }
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

watch(collapsed, (value) => {
  localStorage.setItem('tea_sidebar_collapsed', value ? '1' : '0')
})

function toggleCollapsed() {
  collapsed.value = !collapsed.value
}

function handleDisconnect() {
  connectionStore.disconnect()
}

function openConfig() {
  serverAddress.value = connectionStore.serverUrl || serverAddress.value
  configVisible.value = true
}

async function quickConnect() {
  const ok = await connectionStore.connect(serverAddress.value)
  if (ok) {
    await syncPluginEntries()
  }
}

async function saveAndConnect() {
  const ok = await connectionStore.connect(serverAddress.value)
  if (!ok) return
  await syncPluginEntries()
  configVisible.value = false
}

function shortPluginLabel(value: string) {
  const text = value.trim()
  if (!text) return '--'
  const chineseChars = text.match(/[\u4e00-\u9fff]/g)
  if (chineseChars && chineseChars.length >= 2) {
    return chineseChars.slice(0, 2).join('')
  }
  return text.slice(0, 2)
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

.plugin-mini-text {
  letter-spacing: 0.6px;
  font-weight: 700;
}

.sidebar.collapsed :deep(.static-menu-item) {
  justify-content: center;
  padding-left: 0 !important;
}

.sidebar.collapsed :deep(.static-menu-item .el-icon) {
  margin-right: 0;
}

.sidebar.collapsed :deep(.dynamic-menu-item) {
  padding-left: 14px !important;
}

.sidebar-footer {
  padding: 12px;
  border-top: 1px solid #e6d7c0;
}

.sidebar.collapsed .sidebar-footer {
  display: flex;
  justify-content: center;
  padding: 10px 8px 14px;
}

.connection-card {
  border: 1px solid #dfcfb3;
  border-radius: 12px;
  padding: 10px;
  cursor: pointer;
  background: rgba(255, 252, 245, 0.78);
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

.status-icon {
  font-size: 24px;
  line-height: 1;
}

.status-icon.online {
  color: #369f6d;
}

.status-icon.offline {
  color: #9f7c42;
}
</style>
