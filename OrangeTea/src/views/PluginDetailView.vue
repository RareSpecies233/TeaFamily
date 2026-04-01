<template>
  <div class="plugin-detail-page">
    <div class="page-header">
      <el-button @click="router.back()" size="small">
        <el-icon><ArrowLeft /></el-icon>
        返回
      </el-button>
      <h2>{{ pluginName }}</h2>
    </div>

    <el-skeleton :loading="loading" :rows="8" animated>
      <template #default>
        <el-alert
          v-if="error"
          :title="error"
          type="error"
          show-icon
          style="margin-bottom: 16px"
        />
        <PluginLoader
          v-if="pluginUrl"
          :url="pluginUrl"
          :plugin-name="pluginName"
          :api-base="pluginApiBase"
        />
        <el-empty v-else-if="!error" description="该插件没有前端界面" />
      </template>
    </el-skeleton>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useConnectionStore } from '@/stores/connection'
import * as api from '@/api'
import PluginLoader from '@/components/PluginLoader.vue'

const route = useRoute()
const router = useRouter()
const connectionStore = useConnectionStore()
const pluginName = ref(route.params.name as string)
const pluginUrl = ref('')
const pluginApiBase = ref('/api')
const loading = ref(true)
const error = ref('')

function buildPluginUrl(name: string, entry: string) {
  const base = (connectionStore.serverUrl || '').replace(/\/+$/, '')
  const encodedName = encodeURIComponent(name)
  const encodedEntry = encodeURI(entry)
  if (!base) {
    return `/api/frontend-plugins/${encodedName}/${encodedEntry}`
  }
  return `${base}/api/frontend-plugins/${encodedName}/${encodedEntry}`
}

function buildPluginApiBase() {
  const base = (connectionStore.serverUrl || '').replace(/\/+$/, '')
  if (!base) {
    return '/api'
  }
  return `${base}/api`
}

async function loadPluginFrontend() {
  loading.value = true
  error.value = ''
  try {
    const resp = await api.getFrontendPlugins()
    const plugins: any[] = resp.data.plugins || []
    const found = plugins.find((p: any) => p.name === pluginName.value)
    if (found) {
      const entry = found.entry || 'index.js'
      pluginUrl.value = buildPluginUrl(pluginName.value, entry)
      pluginApiBase.value = buildPluginApiBase()
    } else {
      error.value = '未找到对应的前端插件'
    }
  } catch (e: any) {
    error.value = '加载插件前端失败: ' + (e.message || '未知错误')
  } finally {
    loading.value = false
  }
}

watch(
  () => route.params.name,
  (value) => {
    pluginName.value = value as string
    loadPluginFrontend()
  }
)

watch(
  () => connectionStore.serverUrl,
  () => {
    if (pluginName.value) {
      loadPluginFrontend()
    }
  }
)

onMounted(loadPluginFrontend)
</script>

<style scoped>
.plugin-detail-page .page-header {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 20px;
}

.plugin-detail-page h2 {
  margin: 0;
  color: #333;
}
</style>
