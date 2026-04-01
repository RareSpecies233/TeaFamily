<template>
  <div class="sidebar">
    <div class="sidebar-header">
      <span class="sidebar-logo">OT</span>
      <div class="brand">
        <span class="sidebar-title">OrangeTea</span>
        <span class="sidebar-subtitle">Plugin Console</span>
      </div>
    </div>

    <el-menu
      :default-active="route.path"
      router
      background-color="transparent"
      text-color="#6f6a5e"
      active-text-color="#1b1407"
    >
      <el-menu-item index="/plugins">
        <el-icon><Box /></el-icon>
        <span>插件管理</span>
      </el-menu-item>

      <el-menu-item index="/update">
        <el-icon><Upload /></el-icon>
        <span>版本更新</span>
      </el-menu-item>

      <!-- Dynamic plugin entries -->
      <el-sub-menu index="dynamic-plugins" v-if="pluginStore.frontendPlugins.length">
        <template #title>
          <el-icon><Grid /></el-icon>
          <span>插件页面</span>
        </template>
        <el-menu-item
          v-for="plugin in pluginStore.frontendPlugins"
          :key="plugin.name"
          :index="`/plugin/${plugin.name}`"
        >
          <span>{{ plugin.title || plugin.name }}</span>
        </el-menu-item>
      </el-sub-menu>
    </el-menu>

    <div class="sidebar-footer">
      <div class="connection-card" @click="openConfig">
        <div class="connection-top">
          <span class="connection-label">LemonTea 连接</span>
          <el-tag :type="statusType" size="small">{{ statusLabel }}</el-tag>
        </div>
        <p class="server-url">{{ displayServerUrl }}</p>
        <p v-if="connectionStore.error" class="connection-error">{{ connectionStore.error }}</p>
      </div>

      <div class="connection-actions">
        <el-button text type="primary" :loading="connectionStore.connecting" @click="quickConnect">
          {{ connectionStore.connected ? '重新连接' : '立即连接' }}
        </el-button>
        <el-button v-if="connectionStore.connected" text type="danger" @click="handleDisconnect">
          断开连接
        </el-button>
      </div>
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
const configVisible = ref(false)
const serverAddress = ref(connectionStore.serverUrl || 'http://127.0.0.1:9528')

const statusType = computed(() => (connectionStore.connected ? 'success' : 'info'))
const statusLabel = computed(() => (connectionStore.connected ? '在线' : '离线'))
const displayServerUrl = computed(() => connectionStore.serverUrl || '未设置服务器地址')

onMounted(() => {
  if (connectionStore.connected) {
    pluginStore.fetchFrontendPlugins()
  }
})

watch(
  () => connectionStore.serverUrl,
  (value) => {
    if (!configVisible.value) {
      serverAddress.value = value || 'http://127.0.0.1:9528'
    }
  }
)

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
    await pluginStore.fetchFrontendPlugins()
  }
}

async function saveAndConnect() {
  const ok = await connectionStore.connect(serverAddress.value)
  if (!ok) return
  await pluginStore.fetchFrontendPlugins()
  configVisible.value = false
}
</script>

<style scoped>
.sidebar {
  width: var(--tea-sidebar-width);
  min-width: var(--tea-sidebar-width);
  background:
    radial-gradient(circle at 0 0, rgba(255, 196, 111, 0.35), transparent 35%),
    radial-gradient(circle at 100% 100%, rgba(146, 223, 207, 0.3), transparent 38%),
    linear-gradient(180deg, #f7efe2 0%, #f2e5d4 100%);
  display: flex;
  flex-direction: column;
  height: 100vh;
  border-right: 1px solid #e6d7c0;
}

.sidebar-header {
  padding: 22px 18px;
  display: flex;
  align-items: center;
  gap: 10px;
  border-bottom: 1px solid #e6d7c0;
}

.sidebar-logo {
  width: 38px;
  height: 38px;
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
}

:deep(.el-menu-item), :deep(.el-sub-menu__title) {
  border-radius: 10px;
  margin-bottom: 6px;
}

:deep(.el-menu-item.is-active) {
  background: linear-gradient(90deg, rgba(255, 213, 139, 0.95), rgba(255, 232, 188, 0.95));
  box-shadow: 0 8px 20px rgba(102, 77, 31, 0.15);
}

.sidebar-footer {
  padding: 12px;
  border-top: 1px solid #e6d7c0;
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
  margin-top: 8px;
  display: flex;
  justify-content: space-between;
}
</style>
