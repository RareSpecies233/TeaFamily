<template>
  <div class="plugin-detail-page">
    <div class="page-header">
      <div class="header-spacer" aria-hidden="true"></div>

      <div class="title-wrap">
        <h2>{{ formattedTitle }}</h2>
      </div>

      <el-button class="header-btn" size="small" @click="loadPluginFrontend">
        <el-icon><Refresh /></el-icon>
        刷新
      </el-button>
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
const navName = ref(route.params.name as string)
const pluginUrl = ref('')
const pluginApiBase = ref('/api')
const loading = ref(true)
const error = ref('')
const errorHint = ref('')
const loaderKey = computed(() => `${pluginName.value}:${pluginUrl.value}:${pluginApiBase.value}`)
const formattedTitle = computed(() => `（${navName.value || pluginName.value}）${pluginName.value}`)
const standaloneMode = computed(() => route.name === 'plugin-window' || route.meta.standalone === true)

function handleBack() {
  if (standaloneMode.value && window.opener) {
    window.close()
    return
  }
  router.back()
}

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
  navName.value = pluginName.value

  try {
    const [frontendResp, localResp] = await Promise.all([
      api.getFrontendPlugins(),
      api.getPlugins().catch(() => null),
    ])

    const plugins: any[] = frontendResp.data.plugins || []
    const found = plugins.find((p: any) => p.name === pluginName.value)

    if (found) {
      navName.value = found.title || found.name || pluginName.value
      const entry = found.entry || 'index.js'
      pluginUrl.value = buildPluginUrl(pluginName.value, entry)
      pluginApiBase.value = buildPluginApiBase()
    } else {
      const localPlugins: any[] = localResp?.data?.plugins || []
      const localFound = localPlugins.find((p: any) => p.name === pluginName.value)
      navName.value = localFound?.name || pluginName.value

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
    navName.value = value as string
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
.plugin-detail-page {
  min-height: calc(100vh - 40px);
}

.plugin-detail-page .page-header {
  display: grid;
  grid-template-columns: auto 1fr auto;
  align-items: center;
  gap: 14px;
  padding: 14px 16px;
  margin-bottom: 20px;
  border-radius: 16px;
  border: 1px solid #ead9bb;
  background:
    radial-gradient(circle at 12% 20%, rgba(255, 239, 190, 0.86), transparent 40%),
    linear-gradient(135deg, #fff5df 0%, #ffefdc 42%, #ecfffb 100%);
}

.title-wrap {
  text-align: center;
  min-width: 0;
}

.plugin-detail-page h2 {
  margin: 0;
  color: #2f2310;
  font-size: 20px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.title-wrap p {
  margin-top: 4px;
  color: #7b6641;
  font-size: 12px;
}

.header-btn {
  min-width: 102px;
  flex-shrink: 0;
  height: 36px;
  padding: 0 14px !important;
  border-radius: 11px;
  border: 1px solid #dfcfb1;
  background: rgba(255, 255, 255, 0.84);
  color: #5f4922;
  font-weight: 600;
}

.header-btn:hover,
.header-btn:focus-visible {
  border-color: #cfb887;
  background: #fffdf8;
}

.header-btn:active {
  border-color: #c9ae77;
}

:deep(.header-btn .el-icon) {
  margin-right: 5px;
}

.recover-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.header-spacer {
  min-width: 102px;
  height: 36px;
  flex-shrink: 0;
}

@media (max-width: 768px) {
  .plugin-detail-page .page-header {
    grid-template-columns: 1fr 1fr;
  }

  .title-wrap {
    grid-column: 1 / -1;
    order: -1;
    text-align: left;
  }

  .header-btn {
    min-width: 0;
  }

  .header-spacer {
    min-width: 0;
  }
}
</style>
