<template>
  <div class="plugin-detail-page">
    <div class="page-header">
      <el-button @click="router.back()" size="small">
        <el-icon><ArrowLeft /></el-icon>
        返回
      </el-button>
      <h2>{{ pluginName }}</h2>
      <el-button size="small" @click="loadPluginFrontend">刷新页面</el-button>
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
        <el-alert
          v-if="errorHint"
          :title="errorHint"
          type="info"
          show-icon
          :closable="false"
          style="margin-bottom: 16px"
        />
        <PluginLoader
          v-if="pluginUrl"
          :key="loaderKey"
          :url="pluginUrl"
          :plugin-name="pluginName"
          :api-base="pluginApiBase"
        />
        <el-empty v-else-if="!error" description="该插件没有前端界面" />

        <div v-if="error" class="recover-actions">
          <el-button type="primary" @click="goToPlugins">前往插件管理台</el-button>
          <el-button @click="loadPluginFrontend">重新检测</el-button>
        </div>
      </template>
    </el-skeleton>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, onMounted, watch } from 'vue'
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
const errorHint = ref('')
const loaderKey = computed(() => `${pluginName.value}:${pluginUrl.value}:${pluginApiBase.value}`)

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
  errorHint.value = ''
  pluginUrl.value = ''

  try {
    const [frontendResp, localResp] = await Promise.all([
      api.getFrontendPlugins(),
      api.getPlugins().catch(() => null),
    ])

    const plugins: any[] = frontendResp.data.plugins || []
    const found = plugins.find((p: any) => p.name === pluginName.value)

    if (found) {
      const entry = found.entry || 'index.js'
      pluginUrl.value = buildPluginUrl(pluginName.value, entry)
      pluginApiBase.value = buildPluginApiBase()
    } else {
      const localPlugins: any[] = localResp?.data?.plugins || []
      const localFound = localPlugins.find((p: any) => p.name === pluginName.value)

      if (localFound?.has_frontend) {
        error.value = '插件前端资源缺失或安装不完整'
        errorHint.value = '请在统一插件管理台重新安装该插件的统一包。'
      } else {
        error.value = '未找到对应的前端插件'
        errorHint.value = '当前插件未安装前端页面，或尚未完成统一安装。'
      }
    }
  } catch (e: any) {
    error.value = '加载插件前端失败: ' + (e.message || '未知错误')
    errorHint.value = '请确认 LemonTea 地址可访问，然后重试。'
  } finally {
    loading.value = false
  }
}

function goToPlugins() {
  router.push('/plugins')
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
  flex: 1;
}

.recover-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}
</style>
