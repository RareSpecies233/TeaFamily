<template>
  <div class="sidebar">
    <div class="sidebar-header">
      <span class="sidebar-logo">🍊</span>
      <span class="sidebar-title">OrangeTea</span>
    </div>

    <el-menu
      :default-active="route.path"
      router
      background-color="#1d1e1f"
      text-color="#bbb"
      active-text-color="#e6a23c"
    >
      <el-menu-item index="/">
        <el-icon><Monitor /></el-icon>
        <span>仪表盘</span>
      </el-menu-item>

      <el-menu-item index="/plugins">
        <el-icon><Box /></el-icon>
        <span>插件管理</span>
      </el-menu-item>

      <el-menu-item index="/clients">
        <el-icon><Connection /></el-icon>
        <span>客户端</span>
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
  background: var(--tea-sidebar-bg);
  display: flex;
  flex-direction: column;
  height: 100vh;
}

.sidebar-header {
  padding: 20px;
  display: flex;
  align-items: center;
  gap: 10px;
  border-bottom: 1px solid #333;
}

.sidebar-logo {
  font-size: 28px;
}

.sidebar-title {
  color: #e6a23c;
  font-size: 18px;
  font-weight: bold;
}

.el-menu {
  border-right: none;
  flex: 1;
}

.sidebar-footer {
  padding: 12px;
  border-top: 1px solid #333;
  text-align: center;
}
</style>
