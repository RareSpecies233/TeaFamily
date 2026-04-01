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
      <el-button text type="danger" @click="handleDisconnect">
        <el-icon><SwitchButton /></el-icon>
        断开连接
      </el-button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { useConnectionStore } from '@/stores/connection'
import { usePluginStore } from '@/stores/plugins'

const route = useRoute()
const connectionStore = useConnectionStore()
const pluginStore = usePluginStore()

onMounted(() => {
  pluginStore.fetchFrontendPlugins()
})

function handleDisconnect() {
  connectionStore.disconnect()
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
  text-align: center;
}
</style>
