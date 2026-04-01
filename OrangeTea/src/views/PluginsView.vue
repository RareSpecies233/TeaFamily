<template>
  <div class="plugins-page">
    <header class="hero">
      <h2>统一插件管理台</h2>
      <p>插件包上传后将固定安装到 LemonTea、HoneyTea 与 OrangeTea 三端，不再区分分发策略。</p>
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
        上传统一插件包（.tar.gz）后会先展示插件信息，确认后执行安装。
      </p>

      <el-upload
        :auto-upload="false"
        :show-file-list="false"
        accept=".tar.gz,.tgz"
        :disabled="!connectionStore.connected || installing || previewLoading"
        @change="handleSelectPackage"
      >
        <el-button
          type="primary"
          :loading="installing || previewLoading"
          :disabled="!connectionStore.connected || installing || previewLoading"
        >
          <el-icon><Upload /></el-icon>
          上传并预览
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

        <el-table-column label="HoneyTea" width="130">
          <template #default="{ row }">
            <el-tag :type="remoteStateType(row.remoteState, row.remoteInstalled)" size="small">
              {{ remoteStateLabel(row) }}
            </el-tag>
          </template>
        </el-table-column>

        <el-table-column label="前端页面" width="130">
          <template #default="{ row }">
            <el-tag :type="row.frontendInstalled ? 'success' : 'info'" size="small">
              {{ row.frontendInstalled ? '已安装' : '未安装' }}
            </el-tag>
          </template>
        </el-table-column>

        <el-table-column label="操作" min-width="420">
          <template #default="{ row }">
            <div class="actions">
              <el-button
                size="small"
                type="success"
                class="action-btn"
                @click="startLocal(row.name)"
                :disabled="!row.localInstalled || row.localState === 'running'"
              >
                启动服务端
              </el-button>
              <el-button
                size="small"
                type="warning"
                class="action-btn"
                @click="stopLocal(row.name)"
                :disabled="!row.localInstalled || row.localState !== 'running'"
              >
                停止服务端
              </el-button>
              <el-button
                size="small"
                class="action-btn"
                @click="startRemote(row)"
                :disabled="!row.remoteInstalled || row.remoteState === 'running'"
              >
                启动远端
              </el-button>
              <el-button
                size="small"
                class="action-btn"
                @click="stopRemote(row)"
                :disabled="!row.remoteInstalled || row.remoteState !== 'running'"
              >
                停止远端
              </el-button>
              <el-button
                size="small"
                class="action-btn"
                @click="openPlugin(row.name)"
                :disabled="!row.frontendInstalled"
              >
                打开页面
              </el-button>
              <el-button size="small" type="danger" class="action-btn" @click="deleteUnified(row)">
                统一删除
              </el-button>
            </div>
          </template>
        </el-table-column>
      </el-table>
    </el-card>

    <el-dialog v-model="confirmVisible" title="确认安装插件" width="560px">
      <el-skeleton :loading="previewLoading" :rows="5" animated>
        <template #default>
          <el-descriptions border :column="1" size="small">
            <el-descriptions-item label="插件名称">{{ previewInfo?.name || '-' }}</el-descriptions-item>
            <el-descriptions-item label="版本">{{ previewInfo?.version || '-' }}</el-descriptions-item>
            <el-descriptions-item label="描述">{{ previewInfo?.description || '-' }}</el-descriptions-item>
            <el-descriptions-item label="插件类型">{{ pluginTypeLabel(previewInfo?.plugin_type) }}</el-descriptions-item>
            <el-descriptions-item label="安装目标">
              LemonTea + HoneyTea + OrangeTea（固定三端）
            </el-descriptions-item>
            <el-descriptions-item label="在线 HoneyTea 数量">
              {{ previewInfo?.connected_honey_count ?? 0 }}
            </el-descriptions-item>
            <el-descriptions-item label="能力声明">
              {{ formatList(previewInfo?.capabilities) }}
            </el-descriptions-item>
            <el-descriptions-item label="依赖插件">
              {{ formatList(previewInfo?.depends_on) }}
            </el-descriptions-item>
          </el-descriptions>

          <el-alert
            v-if="(previewInfo?.connected_honey_count ?? 0) === 0"
            title="当前没有在线 HoneyTea，确认后仍会安装 LemonTea 与前端，HoneyTea 侧会标记未完成。"
            type="warning"
            :closable="false"
            style="margin-top: 12px"
          />
        </template>
      </el-skeleton>

      <template #footer>
        <el-button @click="cancelPreview">取消</el-button>
        <el-button type="primary" :loading="installing" @click="confirmInstall">
          确认安装
        </el-button>
      </template>
    </el-dialog>
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
  remoteInstalled: boolean
  remoteState: string
  remoteNodeId: string
}

interface PluginPreviewInfo {
  name: string
  version: string
  description: string
  plugin_type: string
  connected_honey_count: number
  capabilities: string[]
  depends_on: string[]
}

const router = useRouter()
const connectionStore = useConnectionStore()
const pluginStore = usePluginStore()

const clients = ref<ClientInfo[]>([])
const refreshing = ref(false)
const previewLoading = ref(false)
const installing = ref(false)
const confirmVisible = ref(false)
const selectedPackage = ref<File | null>(null)
const previewInfo = ref<PluginPreviewInfo | null>(null)

const connectedClients = computed(() => clients.value.filter((c) => c.connected))
const primaryClient = computed(() => connectedClients.value[0] || null)
const loading = computed(() => pluginStore.loading || refreshing.value)

const pluginRows = computed<UnifiedPluginRow[]>(() => {
  const allNames = new Set<string>()
  const localMap = new Map(pluginStore.localPlugins.map((p) => [p.name, p]))
  const frontendMap = new Map(pluginStore.frontendPlugins.map((p) => [p.name, p]))
  const remoteMap = new Map<string, { state: string; version?: string; pluginType?: string }>()

  const remotePlugins = primaryClient.value?.plugins || []

  for (const local of pluginStore.localPlugins) allNames.add(local.name)
  for (const frontend of pluginStore.frontendPlugins) allNames.add(frontend.name)

  for (const remote of remotePlugins) {
    if (!remote.name) continue
    allNames.add(remote.name)
    remoteMap.set(remote.name, {
      state: remote.state || 'unknown',
      version: remote.version,
      pluginType: remote.plugin_type,
    })
  }

  return Array.from(allNames)
    .map((name) => {
      const local = localMap.get(name)
      const frontend = frontendMap.get(name)
      const remote = remoteMap.get(name)

      return {
        name,
        version: local?.version || frontend?.version || remote?.version || '-',
        pluginType: local?.plugin_type || remote?.pluginType || 'distributed',
        localInstalled: Boolean(local),
        localState: local?.state || 'not-installed',
        frontendInstalled: Boolean(frontend),
        remoteInstalled: Boolean(remote),
        remoteState: remote?.state || (primaryClient.value ? 'not-installed' : 'offline'),
        remoteNodeId: primaryClient.value?.node_id || '',
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

function remoteStateType(state: string, installed: boolean) {
  if (!primaryClient.value) return 'info'
  if (!installed) return 'warning'
  if (state === 'running') return 'success'
  if (state === 'stopped') return 'info'
  if (state === 'crashed') return 'danger'
  return 'warning'
}

function remoteStateLabel(row: UnifiedPluginRow) {
  if (!primaryClient.value) return '未连接'
  if (!row.remoteInstalled) return '未安装'
  return row.remoteState
}

function formatList(items?: string[]) {
  if (!items || items.length === 0) return '-'
  return items.join(', ')
}

async function fetchClients() {
  const resp = await api.getClients()
  const list: ClientInfo[] = resp.data.clients || []

  const detailed = await Promise.all(
    list.map(async (client) => {
      if (!client.connected) {
        return { ...client, plugins: [] }
      }
      try {
        const pluginResp = await api.getClientPlugins(client.node_id)
        return { ...client, plugins: pluginResp.data.plugins || [] }
      } catch {
        return { ...client, plugins: client.plugins || [] }
      }
    })
  )

  clients.value = detailed
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

async function startRemote(row: UnifiedPluginRow) {
  if (!row.remoteInstalled || !row.remoteNodeId) return
  try {
    await api.startRemotePlugin(row.remoteNodeId, row.name)
    ElMessage.success('已向 HoneyTea 发送启动命令')
    await refreshAll()
  } catch {
    ElMessage.error('远端启动失败')
  }
}

async function stopRemote(row: UnifiedPluginRow) {
  if (!row.remoteInstalled || !row.remoteNodeId) return
  try {
    await api.stopRemotePlugin(row.remoteNodeId, row.name)
    ElMessage.success('已向 HoneyTea 发送停止命令')
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

  if (row.remoteInstalled && row.remoteNodeId) {
    try {
      if (row.remoteState === 'running') {
        await api.stopRemotePlugin(row.remoteNodeId, row.name)
      }
      await api.deleteRemotePlugin(row.remoteNodeId, row.name)
    } catch {
      failures.push(`HoneyTea(${row.remoteNodeId}) 删除失败`)
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

async function handleSelectPackage(uploadFile: UploadFile) {
  if (!uploadFile.raw) return

  previewLoading.value = true
  try {
    selectedPackage.value = uploadFile.raw
    const resp = await api.inspectUnifiedPlugin(uploadFile.raw)
    previewInfo.value = resp.data?.plugin || null
    confirmVisible.value = true
  } catch {
    selectedPackage.value = null
    previewInfo.value = null
    ElMessage.error('插件信息解析失败，请检查插件包格式')
  } finally {
    previewLoading.value = false
  }
}

function cancelPreview() {
  confirmVisible.value = false
  selectedPackage.value = null
  previewInfo.value = null
}

async function confirmInstall() {
  if (!selectedPackage.value) return

  installing.value = true
  try {
    const resp = await api.installUnifiedPlugin(selectedPackage.value)
    const data = resp.data || {}

    if (Array.isArray(data.warnings) && data.warnings.length) {
      ElMessage.warning(data.warnings.join('；'))
    }

    if (data.success) {
      ElMessage.success(`插件 ${data.name || ''} 统一安装成功`)
    } else {
      ElMessage.warning(`插件 ${data.name || ''} 已处理，但存在部分失败，请查看日志`)
    }

    confirmVisible.value = false
    selectedPackage.value = null
    previewInfo.value = null
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

.actions {
  display: grid;
  grid-template-columns: repeat(3, minmax(118px, 1fr));
  gap: 6px;
}

.action-btn {
  width: 100%;
  justify-content: center;
}
</style>
