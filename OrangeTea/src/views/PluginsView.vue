<template>
  <div class="plugins-page">
    <header class="hero">
      <h2>统一插件管理台</h2>
      <p>单入口管理插件安装、分发、运行与前端页面，无需再区分 LemonTea / HoneyTea / OrangeTea。</p>
    </header>

    <el-alert
      v-if="!connectionStore.connected"
      title="当前未连接到 LemonTea，插件列表可能不是最新数据。请先在左下角配置连接。"
      type="warning"
      :closable="false"
      class="connect-alert"
    />

    <el-card class="panel-card install-card">
      <template #header>
        <div class="panel-header">
          <span>统一安装</span>
          <el-button size="small" @click="refreshAll">
            <el-icon><Refresh /></el-icon>
            刷新数据
          </el-button>
        </div>
      </template>

      <p class="install-tip">
        上传一个插件包（.tar.gz），系统会自动识别插件类型并统一分发：
        <strong>服务端运行时 + 客户端运行时 + 前端页面</strong>。
      </p>

      <div class="install-options">
        <span class="option-label">HoneyTea 分发范围：</span>
        <el-radio-group v-model="installTargetMode">
          <el-radio-button label="all">全部在线客户端</el-radio-button>
          <el-radio-button label="selected">指定客户端</el-radio-button>
          <el-radio-button label="none">不分发客户端</el-radio-button>
        </el-radio-group>
      </div>

      <el-select
        v-if="installTargetMode === 'selected'"
        v-model="selectedClientIds"
        multiple
        collapse-tags
        collapse-tags-tooltip
        placeholder="选择要分发的 HoneyTea 客户端"
        class="client-select"
      >
        <el-option
          v-for="client in connectedClients"
          :key="client.node_id"
          :label="`${client.node_id} (${client.address}:${client.port})`"
          :value="client.node_id"
        />
      </el-select>

      <el-upload
        :auto-upload="false"
        :show-file-list="false"
        accept=".tar.gz,.tgz"
        :disabled="installDisabled"
        @change="handleUnifiedInstall"
      >
        <el-button type="primary" :loading="installing" :disabled="installDisabled">
          <el-icon><Upload /></el-icon>
          上传并统一安装
        </el-button>
      </el-upload>
    </el-card>

    <el-card class="panel-card">
      <template #header>
        <div class="panel-header">
          <span>插件资产总览</span>
          <span class="summary">共 {{ pluginRows.length }} 个插件</span>
        </div>
      </template>

      <el-table :data="pluginRows" stripe v-loading="loading" empty-text="暂无插件数据">
        <el-table-column prop="name" label="插件" min-width="180" />
        <el-table-column prop="version" label="版本" width="110" />

        <el-table-column label="类型" width="130">
          <template #default="{ row }">
            <el-tag size="small" type="warning">{{ pluginTypeLabel(row.pluginType) }}</el-tag>
          </template>
        </el-table-column>

        <el-table-column label="LemonTea" width="120">
          <template #default="{ row }">
            <el-tag :type="localStateType(row.localState)" size="small">
              {{ row.localInstalled ? row.localState : '未安装' }}
            </el-tag>
          </template>
        </el-table-column>

        <el-table-column label="HoneyTea" min-width="220">
          <template #default="{ row }">
            <div class="remote-cell">
              <span>{{ remoteSummary(row) }}</span>
              <el-tooltip
                v-if="row.remoteStates.length"
                effect="light"
                placement="top"
                :content="remoteTooltip(row)"
              >
                <el-tag size="small" type="info">节点详情</el-tag>
              </el-tooltip>
            </div>
          </template>
        </el-table-column>

        <el-table-column label="前端页面" width="130">
          <template #default="{ row }">
            <el-tag :type="row.frontendInstalled ? 'success' : 'info'" size="small">
              {{ row.frontendInstalled ? '已安装' : '未安装' }}
            </el-tag>
          </template>
        </el-table-column>

        <el-table-column label="操作" min-width="380">
          <template #default="{ row }">
            <div class="actions">
              <el-button
                size="small"
                type="success"
                @click="startLocal(row.name)"
                :disabled="!row.localInstalled || row.localState === 'running'"
              >
                启动服务端
              </el-button>
              <el-button
                size="small"
                type="warning"
                @click="stopLocal(row.name)"
                :disabled="!row.localInstalled || row.localState !== 'running'"
              >
                停止服务端
              </el-button>
              <el-button
                size="small"
                @click="startRemoteAll(row)"
                :disabled="row.remoteInstalled === 0"
              >
                启动远端
              </el-button>
              <el-button
                size="small"
                @click="stopRemoteAll(row)"
                :disabled="row.remoteInstalled === 0"
              >
                停止远端
              </el-button>
              <el-button
                size="small"
                @click="openPlugin(row.name)"
                :disabled="!row.frontendInstalled"
              >
                打开页面
              </el-button>
              <el-button size="small" type="danger" @click="deleteUnified(row)">
                统一删除
              </el-button>
            </div>
          </template>
        </el-table-column>
      </el-table>
    </el-card>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage, ElMessageBox, type UploadFile } from 'element-plus'
import { useConnectionStore } from '@/stores/connection'
import { usePluginStore } from '@/stores/plugins'
import * as api from '@/api'

interface ClientPluginInfo {
  name: string
  version?: string
  state?: string
  plugin_type?: string
}

interface ClientInfo {
  node_id: string
  address: string
  port: number
  connected: boolean
  plugins?: ClientPluginInfo[]
}

interface UnifiedPluginRow {
  name: string
  version: string
  pluginType: string
  localInstalled: boolean
  localState: string
  frontendInstalled: boolean
  remoteInstalled: number
  remoteRunning: number
  remoteTotal: number
  remoteStates: Array<{ nodeId: string; state: string }>
}

const router = useRouter()
const connectionStore = useConnectionStore()
const pluginStore = usePluginStore()

const clients = ref<ClientInfo[]>([])
const refreshing = ref(false)
const installing = ref(false)
const installTargetMode = ref<'all' | 'selected' | 'none'>('all')
const selectedClientIds = ref<string[]>([])

const connectedClients = computed(() => clients.value.filter((c) => c.connected))
const loading = computed(() => pluginStore.loading || refreshing.value)
const installDisabled = computed(() => {
  if (!connectionStore.connected || installing.value) return true
  if (installTargetMode.value === 'selected' && selectedClientIds.value.length === 0) return true
  return false
})

const pluginRows = computed<UnifiedPluginRow[]>(() => {
  const allNames = new Set<string>()
  const localMap = new Map(pluginStore.localPlugins.map((p) => [p.name, p]))
  const frontendMap = new Map(pluginStore.frontendPlugins.map((p) => [p.name, p]))
  const remoteMap = new Map<string, Array<{ nodeId: string; state: string; version?: string; pluginType?: string }>>()

  for (const local of pluginStore.localPlugins) allNames.add(local.name)
  for (const frontend of pluginStore.frontendPlugins) allNames.add(frontend.name)

  for (const client of connectedClients.value) {
    const plugins = client.plugins || []
    for (const remote of plugins) {
      if (!remote.name) continue
      allNames.add(remote.name)
      const list = remoteMap.get(remote.name) || []
      list.push({
        nodeId: client.node_id,
        state: remote.state || 'unknown',
        version: remote.version,
        pluginType: remote.plugin_type,
      })
      remoteMap.set(remote.name, list)
    }
  }

  return Array.from(allNames)
    .map((name) => {
      const local = localMap.get(name)
      const frontend = frontendMap.get(name)
      const remoteStates = remoteMap.get(name) || []
      const running = remoteStates.filter((s) => s.state === 'running').length
      const sampleRemote = remoteStates[0]

      return {
        name,
        version: local?.version || frontend?.version || sampleRemote?.version || '-',
        pluginType: local?.plugin_type || sampleRemote?.pluginType || 'distributed',
        localInstalled: Boolean(local),
        localState: local?.state || 'not-installed',
        frontendInstalled: Boolean(frontend),
        remoteInstalled: remoteStates.length,
        remoteRunning: running,
        remoteTotal: connectedClients.value.length,
        remoteStates,
      }
    })
    .sort((a, b) => a.name.localeCompare(b.name))
})

function pluginTypeLabel(type?: string) {
  if (type === 'lemon-only') return '仅 LemonTea'
  if (type === 'honey-only') return '仅 HoneyTea'
  return '双端插件'
}

function localStateType(state: string) {
  if (state === 'running') return 'success'
  if (state === 'stopped') return 'info'
  if (state === 'crashed') return 'danger'
  return 'warning'
}

function remoteSummary(row: UnifiedPluginRow) {
  if (row.remoteTotal === 0) return '暂无在线客户端'
  if (row.remoteInstalled === 0) return `0/${row.remoteTotal} 已安装`
  return `${row.remoteInstalled}/${row.remoteTotal} 已安装，${row.remoteRunning} 运行中`
}

function remoteTooltip(row: UnifiedPluginRow) {
  return row.remoteStates.map((state) => `${state.nodeId}:${state.state}`).join(' | ')
}

async function fetchClients() {
  const resp = await api.getClients()
  clients.value = resp.data.clients || []
}

async function refreshAll() {
  if (!connectionStore.connected) return

  refreshing.value = true
  try {
    await Promise.all([
      pluginStore.fetchPlugins(),
      pluginStore.fetchFrontendPlugins(),
      fetchClients(),
    ])
  } catch {
    ElMessage.error('刷新失败')
  } finally {
    refreshing.value = false
  }
}

async function startLocal(name: string) {
  try {
    await pluginStore.startPlugin(name)
    ElMessage.success(`已启动服务端插件：${name}`)
    await refreshAll()
  } catch {
    ElMessage.error('启动失败')
  }
}

async function stopLocal(name: string) {
  try {
    await pluginStore.stopPlugin(name)
    ElMessage.success(`已停止服务端插件：${name}`)
    await refreshAll()
  } catch {
    ElMessage.error('停止失败')
  }
}

async function startRemoteAll(row: UnifiedPluginRow) {
  if (!row.remoteStates.length) return
  try {
    await Promise.all(row.remoteStates.map((state) => api.startRemotePlugin(state.nodeId, row.name)))
    ElMessage.success(`已向 ${row.remoteStates.length} 个客户端发送启动命令`)
    await refreshAll()
  } catch {
    ElMessage.error('远端启动失败')
  }
}

async function stopRemoteAll(row: UnifiedPluginRow) {
  if (!row.remoteStates.length) return
  try {
    await Promise.all(row.remoteStates.map((state) => api.stopRemotePlugin(state.nodeId, row.name)))
    ElMessage.success(`已向 ${row.remoteStates.length} 个客户端发送停止命令`)
    await refreshAll()
  } catch {
    ElMessage.error('远端停止失败')
  }
}

async function deleteUnified(row: UnifiedPluginRow) {
  try {
    await ElMessageBox.confirm(`确认统一删除插件 ${row.name}？`, '确认操作')
  } catch {
    return
  }

  const failures: string[] = []

  if (row.localInstalled) {
    try {
      if (row.localState === 'running') {
        await pluginStore.stopPlugin(row.name)
      }
      await pluginStore.deletePlugin(row.name)
    } catch {
      failures.push('LemonTea 删除失败')
    }
  }

  for (const remote of row.remoteStates) {
    try {
      if (remote.state === 'running') {
        await api.stopRemotePlugin(remote.nodeId, row.name)
      }
      await api.deleteRemotePlugin(remote.nodeId, row.name)
    } catch {
      failures.push(`HoneyTea(${remote.nodeId}) 删除失败`)
    }
  }

  if (row.frontendInstalled) {
    try {
      await api.deleteFrontendPlugin(row.name)
    } catch {
      failures.push('前端插件删除失败')
    }
  }

  await refreshAll()

  if (failures.length) {
    ElMessage.warning(failures.join('；'))
  } else {
    ElMessage.success(`插件 ${row.name} 已统一删除`)
  }
}

async function handleUnifiedInstall(uploadFile: UploadFile) {
  if (!uploadFile.raw) return

  const nodeIds = installTargetMode.value === 'selected' ? selectedClientIds.value : []
  if (installTargetMode.value === 'selected' && nodeIds.length === 0) {
    ElMessage.warning('请先选择至少一个客户端')
    return
  }

  installing.value = true
  try {
    const resp = await api.installUnifiedPlugin(uploadFile.raw, installTargetMode.value, nodeIds)
    const data = resp.data || {}

    if (Array.isArray(data.warnings) && data.warnings.length) {
      ElMessage.warning(data.warnings.join('；'))
    }

    if (data.success) {
      ElMessage.success(`插件 ${data.name || ''} 统一安装成功`)
    } else {
      ElMessage.warning(`插件 ${data.name || ''} 已处理，但存在部分失败，请查看日志`)
    }

    await refreshAll()
  } catch {
    ElMessage.error('统一安装失败')
  } finally {
    installing.value = false
  }
}

function openPlugin(name: string) {
  router.push(`/plugin/${name}`)
}

watch(
  () => connectionStore.connected,
  (connected) => {
    if (connected) {
      refreshAll()
    } else {
      clients.value = []
      pluginStore.localPlugins = []
      pluginStore.frontendPlugins = []
    }
  }
)

onMounted(refreshAll)
</script>

<style scoped>
.plugins-page {
  min-height: calc(100vh - 40px);
}

.hero {
  background:
    radial-gradient(circle at 12% 20%, rgba(255, 239, 190, 0.9), transparent 40%),
    linear-gradient(135deg, #fff5df 0%, #ffefdc 40%, #ecfffb 100%);
  border: 1px solid #f0dfbd;
  border-radius: 18px;
  padding: 18px 20px;
  margin-bottom: 16px;
}

.hero h2 {
  margin: 0;
  color: #2d210c;
  letter-spacing: 0.2px;
}

.hero p {
  margin: 8px 0 0;
  color: #7d6841;
}

.connect-alert {
  margin-bottom: 16px;
}

.panel-card {
  border-radius: 14px;
  border: 1px solid #e9dec8;
  box-shadow: 0 10px 26px rgba(128, 92, 31, 0.08);
  margin-bottom: 14px;
}

.install-card {
  margin-bottom: 14px;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
}

.summary {
  color: #8f7853;
  font-size: 12px;
}

.install-tip {
  margin: 0 0 12px;
  color: #6e5d3f;
}

.install-options {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 12px;
  flex-wrap: wrap;
}

.option-label {
  color: #6f5d41;
  font-size: 13px;
}

.client-select {
  width: 100%;
  margin-bottom: 12px;
}

.remote-cell {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
}

.actions {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-wrap: wrap;
}
</style>
