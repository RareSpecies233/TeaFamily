<template>
  <div class="plugins-page">
    <header class="hero">
      <div class="hero-left">
        <h2>统一插件管理台</h2>
        <p>删除插件后可能无法自动刷新界面，请尝试手动点击刷新按钮。</p>
      </div>

      <div class="hero-actions">
        <el-button size="small" @click="refreshAll" :disabled="refreshing">
          <el-icon><Refresh /></el-icon>
          刷新数据
        </el-button>

        <el-upload
          :auto-upload="false"
          :show-file-list="false"
          accept=".tar.gz,.tgz,application/gzip,application/x-gzip"
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
      </div>
    </header>

    <el-alert
      v-if="!connectionStore.connected"
      title="当前未连接到 LemonTea，插件列表可能不是最新数据。请先在左下角配置连接。"
      type="warning"
      :closable="false"
      class="connect-alert"
    />


    <el-card class="panel-card">
      <template #header>
        <div class="panel-header">
          <span>插件资产总览</span>
          <span class="summary">共 {{ pluginRows.length }} 个插件</span>
        </div>
      </template>

      <el-table :data="pluginRows" stripe v-loading="loading" empty-text="暂无插件数据" size="small">
        <el-table-column prop="name" label="插件" min-width="140" />

        <el-table-column label="运行开关" min-width="360">
          <template #default="{ row }">
            <div class="switch-panel">
              <div class="switch-item">
                <span class="switch-label">
                  <i class="state-dot" :class="localIndicatorClass(row)" />
                  LemonTea
                </span>
                <el-tooltip
                  effect="dark"
                  placement="top"
                  :content="localUnavailableTip(row)"
                  :disabled="!isLocalSwitchDisabled(row)"
                >
                  <span class="switch-wrap">
                    <el-switch
                      :model-value="row.localState === 'running'"
                      :disabled="isLocalSwitchDisabled(row)"
                      inline-prompt
                      active-text="运行"
                      inactive-text="停止"
                      active-color="#21a366"
                      inactive-color="#d9534f"
                      @change="onLocalSwitch(row, Boolean($event))"
                    />
                  </span>
                </el-tooltip>
              </div>

              <div class="switch-item">
                <span class="switch-label">
                  <i class="state-dot" :class="remoteIndicatorClass(row)" />
                  HoneyTea
                </span>
                <el-tooltip
                  effect="dark"
                  placement="top"
                  :content="remoteUnavailableTip(row)"
                  :disabled="!isRemoteSwitchDisabled(row)"
                >
                  <span class="switch-wrap">
                    <el-switch
                      :model-value="row.remoteState === 'running'"
                      :disabled="isRemoteSwitchDisabled(row)"
                      inline-prompt
                      active-text="运行"
                      inactive-text="停止"
                      active-color="#21a366"
                      inactive-color="#d9534f"
                      @change="onRemoteSwitch(row, Boolean($event))"
                    />
                  </span>
                </el-tooltip>
              </div>
            </div>
          </template>
        </el-table-column>

        <el-table-column label="详情" width="110" align="center">
          <template #default="{ row }">
            <el-button size="small" @click="openDetail(row)">详情</el-button>
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
              {{ distributionTargetsLabel(previewInfo) }}
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
            v-if="needsHoneyInstall(previewInfo?.plugin_type) && (previewInfo?.connected_honey_count ?? 0) === 0"
            :title="noHoneyWarning(previewInfo?.plugin_type)"
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

    <el-dialog v-model="detailVisible" title="插件详情" width="560px">
      <template v-if="detailRow">
        <el-descriptions border :column="1" size="small">
          <el-descriptions-item label="插件名称">{{ detailRow.name }}</el-descriptions-item>
          <el-descriptions-item label="版本">{{ detailRow.version }}</el-descriptions-item>
          <el-descriptions-item label="类型">{{ pluginTypeLabel(detailRow.pluginType) }}</el-descriptions-item>
          <el-descriptions-item label="描述">{{ detailRow.description || '暂无描述' }}</el-descriptions-item>
          <el-descriptions-item label="前端页面">
            {{ detailRow.frontendInstalled ? '已安装' : '未安装' }}
          </el-descriptions-item>
          <el-descriptions-item label="LemonTea 开关状态">
            {{ localSwitchStatusText(detailRow) }}
          </el-descriptions-item>
          <el-descriptions-item label="HoneyTea 开关状态">
            {{ remoteSwitchStatusText(detailRow) }}
          </el-descriptions-item>
        </el-descriptions>
      </template>

      <template #footer>
        <el-button @click="detailVisible = false">关闭</el-button>
        <el-button
          @click="detailRow && openPlugin(detailRow.name)"
          :disabled="!detailRow?.frontendInstalled"
        >
          打开插件界面
        </el-button>
        <el-button
          type="danger"
          @click="detailRow && deleteUnified(detailRow)"
          :disabled="!detailRow"
        >
          删除插件
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
  description?: string
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
  description: string
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
  distribution_targets?: string[]
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
const detailVisible = ref(false)
const detailRow = ref<UnifiedPluginRow | null>(null)

const connectedClients = computed(() => clients.value.filter((c) => c.connected))
const primaryClient = computed(() => {
  const preferredNodeId = connectionStore.selectedHoneyNodeId
  if (preferredNodeId) {
    const preferred = connectedClients.value.find((c) => c.node_id === preferredNodeId)
    if (preferred) {
      return preferred
    }
  }
  return connectedClients.value[0] || null
})
const loading = computed(() => pluginStore.loading || refreshing.value)

const pluginRows = computed<UnifiedPluginRow[]>(() => {
  const allNames = new Set<string>()
  const localMap = new Map(pluginStore.localPlugins.map((p) => [p.name, p]))
  const frontendMap = new Map(pluginStore.frontendPlugins.map((p) => [p.name, p]))
  const remoteMap = new Map<string, { state: string; version?: string; pluginType?: string; description?: string }>()

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
      description: remote.description,
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
        description: local?.description || frontend?.description || remote?.description || '',
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

function needsHoneyInstall(type?: string) {
  return type !== 'lemon-only'
}

function distributionTargetsLabel(info: PluginPreviewInfo | null) {
  if (!info) return '-'

  const targets = Array.isArray(info.distribution_targets) ? info.distribution_targets : []
  if (targets.length > 0) {
    return targets.join(' + ')
  }

  if (info.plugin_type === 'lemon-only') {
    return 'LemonTea + OrangeTea'
  }
  if (info.plugin_type === 'honey-only') {
    return 'HoneyTea + OrangeTea'
  }
  return 'LemonTea + HoneyTea + OrangeTea'
}

function noHoneyWarning(type?: string) {
  if (type === 'honey-only') {
    return '当前没有在线 HoneyTea，确认后仅会安装前端页面，HoneyTea 运行时安装将跳过。'
  }
  return '当前没有在线 HoneyTea，确认后仍会安装 LemonTea 与前端，HoneyTea 侧会标记未完成。'
}

function isLocalSwitchDisabled(row: UnifiedPluginRow) {
  return !row.localInstalled
}

function isRemoteSwitchDisabled(row: UnifiedPluginRow) {
  if (!row.remoteNodeId) return true
  return !row.remoteInstalled
}

function localUnavailableTip(row: UnifiedPluginRow) {
  if (row.localInstalled) return ''
  if (row.pluginType === 'honey-only') {
    return '该插件仅安装在 HoneyTea，LemonTea 开关不可用'
  }
  return 'LemonTea 端未安装该插件'
}

function remoteUnavailableTip(row: UnifiedPluginRow) {
  if (!row.remoteNodeId) {
    return '当前没有在线 HoneyTea，无法操作远端开关'
  }
  if (row.remoteInstalled) {
    return ''
  }
  if (row.pluginType === 'lemon-only') {
    return '该插件仅安装在 LemonTea，HoneyTea 开关不可用'
  }
  return 'HoneyTea 端未安装该插件'
}

function stateClass(state: string, installed: boolean) {
  if (!installed) return 'state-unavailable'
  if (state === 'running') return 'state-running'
  if (state === 'crashed') return 'state-crashed'
  return 'state-stopped'
}

function localIndicatorClass(row: UnifiedPluginRow) {
  return stateClass(row.localState, row.localInstalled)
}

function remoteIndicatorClass(row: UnifiedPluginRow) {
  return stateClass(row.remoteState, row.remoteInstalled && Boolean(row.remoteNodeId))
}

function localSwitchStatusText(row: UnifiedPluginRow) {
  if (!row.localInstalled) {
    return row.pluginType === 'honey-only' ? '不可用（仅 HoneyTea 安装）' : '未安装'
  }
  return row.localState === 'running' ? '运行中' : row.localState
}

function remoteSwitchStatusText(row: UnifiedPluginRow) {
  if (!row.remoteNodeId) {
    return '不可用（HoneyTea 未连接）'
  }
  if (!row.remoteInstalled) {
    return row.pluginType === 'lemon-only' ? '不可用（仅 LemonTea 安装）' : '未安装'
  }
  return row.remoteState === 'running' ? '运行中' : row.remoteState
}

function openDetail(row: UnifiedPluginRow) {
  detailRow.value = row
  detailVisible.value = true
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

  if (connectionStore.selectedHoneyNodeId) {
    const exists = detailed.some((c) => c.node_id === connectionStore.selectedHoneyNodeId)
    if (!exists) {
      connectionStore.setSelectedHoneyNodeId('')
    }
  }

  if (!connectionStore.selectedHoneyNodeId) {
    const firstConnected = detailed.find((c) => c.connected)
    if (firstConnected) {
      connectionStore.setSelectedHoneyNodeId(firstConnected.node_id)
    }
  }
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

async function refreshAfterMutation() {
  await refreshAll()
  window.setTimeout(() => {
    if (connectionStore.connected) {
      refreshAll()
    }
  }, 450)
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

async function onLocalSwitch(row: UnifiedPluginRow, enabled: boolean) {
  if (isLocalSwitchDisabled(row)) return

  if (enabled) {
    await startLocal(row.name)
  } else {
    await stopLocal(row.name)
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

async function onRemoteSwitch(row: UnifiedPluginRow, enabled: boolean) {
  if (isRemoteSwitchDisabled(row)) return

  if (enabled) {
    await startRemote(row)
  } else {
    await stopRemote(row)
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

  await refreshAfterMutation()

  if (failures.length) {
    ElMessage.warning(failures.join('；'))
  } else {
    ElMessage.success(`插件 ${row.name} 已统一删除`)
    if (detailRow.value?.name === row.name) {
      detailVisible.value = false
      detailRow.value = null
    }
  }
}

async function handleSelectPackage(uploadFile: UploadFile) {
  if (!uploadFile.raw) return

  const fileName = (uploadFile.raw.name || uploadFile.name || '').toLowerCase()
  const isTarGz = fileName.endsWith('.tar.gz') || fileName.endsWith('.tgz')
  if (!isTarGz) {
    selectedPackage.value = null
    previewInfo.value = null
    ElMessage.error('仅支持上传 .tar.gz 或 .tgz 插件包')
    return
  }

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
    await refreshAfterMutation()
  } catch {
    ElMessage.error('统一安装失败')
  } finally {
    installing.value = false
  }
}

function openPlugin(name: string) {
  const multiWindowMode = localStorage.getItem('tea_plugin_multi_window') === '1'

  if (multiWindowMode) {
    const to = router.resolve({
      name: 'plugin-window',
      params: { name },
    })
    window.open(to.href, '_blank', 'noopener,noreferrer')
    return
  }

  router.push({
    name: 'plugin-detail',
    params: { name },
  })
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

watch(
  pluginRows,
  (rows) => {
    if (!detailRow.value) return
    const latest = rows.find((row) => row.name === detailRow.value?.name)
    if (latest) {
      detailRow.value = latest
    } else {
      detailRow.value = null
      detailVisible.value = false
    }
  },
  { deep: true }
)

onMounted(refreshAll)
</script>

<style scoped>
.plugins-page {
  min-height: calc(100vh - 40px);
  min-width: 760px;
  overflow-x: auto;
}

.el-table {
  min-width: 760px;
}


.hero {
  display: flex;
  justify-content: space-between;
  align-items: center;
  background:
    radial-gradient(circle at 12% 20%, rgba(255, 239, 190, 0.9), transparent 40%),
    linear-gradient(135deg, #fff5df 0%, #ffefdc 40%, #ecfffb 100%);
  border: 1px solid #f0dfbd;
  border-radius: 18px;
  padding: 18px 20px;
  margin-bottom: 16px;
}

.hero-left {
  max-width: calc(100% - 260px);
}

.hero-actions {
  display: flex;
  gap: 8px;
  align-items: center;
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

.switch-panel {
  display: flex;
  gap: 12px;
  align-items: center;
  justify-content: flex-start;
}

.switch-item {
  display: flex;
  flex-direction: column;
  gap: 6px;
  min-width: 130px;
}

.switch-wrap {
  display: inline-flex;
}

.switch-label {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  color: #5f4c2c;
  font-size: 12px;
  font-weight: 600;
}

.state-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
  box-shadow: 0 0 0 2px rgba(255, 255, 255, 0.8);
}

.state-running {
  background: #21a366;
}

.state-stopped {
  background: #d9534f;
}

.state-crashed {
  background: #dc3545;
}

.state-unavailable {
  background: #9aa0a6;
}

.el-table th,
.el-table td {
  padding: 8px 6px;
}

.el-table-column {
  white-space: nowrap;
}

.plugins-page .panel-card {
  overflow-x: hidden;
}

@media (max-width: 1200px) {
  .plugins-page .panel-card {
    padding: 8px;
  }
  .el-table-column {
    font-size: 12px;
  }
  .switch-item {
    min-width: 116px;
  }
}

@media (max-width: 992px) {
  .hero {
    flex-wrap: wrap;
  }
  .hero-actions {
    width: 100%;
    justify-content: flex-start;
  }
  .hero-actions .el-button,
  .hero-actions .el-upload {
    width: auto;
  }
  .switch-panel {
    flex-wrap: wrap;
  }
}

</style>
