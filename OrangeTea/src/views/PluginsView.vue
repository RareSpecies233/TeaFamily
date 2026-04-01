<template>
  <div class="plugins-page">
    <header class="hero">
      <h2>插件管理台</h2>
      <p>统一管理 LemonTea、HoneyTea 与 OrangeTea 前端插件</p>
    </header>

    <el-tabs v-model="activeTab" class="plugin-tabs">
      <el-tab-pane label="LemonTea 插件" name="lemon">
        <el-card class="panel-card">
          <template #header>
            <div class="panel-header">
              <span>服务端插件（LemonTea）</span>
              <el-upload
                :auto-upload="false"
                :show-file-list="false"
                accept=".tar.gz,.tgz"
                @change="handleInstall"
              >
                <el-button type="primary">
                  <el-icon><Plus /></el-icon>
                  安装 LemonTea 插件
                </el-button>
              </el-upload>
            </div>
          </template>

          <el-table :data="pluginStore.localPlugins" stripe v-loading="pluginStore.loading">
            <el-table-column prop="name" label="名称" />
            <el-table-column prop="version" label="版本" width="100" />
            <el-table-column label="类型" width="140">
              <template #default="{ row }">
                <el-tag size="small" type="warning">{{ pluginTypeLabel(row.plugin_type) }}</el-tag>
              </template>
            </el-table-column>
            <el-table-column prop="description" label="描述" />
            <el-table-column label="状态" width="100">
              <template #default="{ row }">
                <el-tag :type="stateType(row.state)" size="small">{{ row.state }}</el-tag>
              </template>
            </el-table-column>
            <el-table-column label="PID" width="90">
              <template #default="{ row }">{{ row.pid || '-' }}</template>
            </el-table-column>
            <el-table-column label="操作" width="220">
              <template #default="{ row }">
                <el-button-group>
                  <el-button
                    size="small"
                    type="success"
                    @click="handleStart(row.name)"
                    :disabled="row.state === 'running'"
                  >
                    启动
                  </el-button>
                  <el-button
                    size="small"
                    type="warning"
                    @click="handleStop(row.name)"
                    :disabled="row.state !== 'running'"
                  >
                    停止
                  </el-button>
                  <el-button
                    size="small"
                    type="danger"
                    @click="handleDelete(row.name)"
                    :disabled="row.state === 'running'"
                  >
                    删除
                  </el-button>
                </el-button-group>
              </template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-tab-pane>

      <el-tab-pane label="HoneyTea 插件" name="honey">
        <el-card class="panel-card" v-for="client in clients" :key="client.node_id">
          <template #header>
            <div class="panel-header panel-header-wrap">
              <span>
                <el-tag :type="client.connected ? 'success' : 'danger'" size="small">
                  {{ client.connected ? '在线' : '离线' }}
                </el-tag>
                <strong class="client-id">{{ client.node_id }}</strong>
                <span class="client-addr">{{ client.address }}:{{ client.port }}</span>
              </span>
              <div class="actions-row">
                <el-button size="small" @click="refreshClient(client.node_id)">
                  <el-icon><Refresh /></el-icon>
                  刷新
                </el-button>
                <el-upload
                  :auto-upload="false"
                  :show-file-list="false"
                  accept=".tar.gz,.tgz"
                  @change="(f: any) => installToClient(client.node_id, f)"
                >
                  <el-button type="primary" size="small">
                    <el-icon><Upload /></el-icon>
                    安装到客户端
                  </el-button>
                </el-upload>
              </div>
            </div>
          </template>

          <el-table :data="client.plugins || []" stripe size="small">
            <el-table-column prop="name" label="插件" />
            <el-table-column prop="version" label="版本" width="100" />
            <el-table-column label="类型" width="140">
              <template #default="{ row }">
                <el-tag size="small" type="warning">{{ pluginTypeLabel(row.plugin_type) }}</el-tag>
              </template>
            </el-table-column>
            <el-table-column label="状态" width="100">
              <template #default="{ row }">
                <el-tag :type="row.state === 'running' ? 'success' : 'info'" size="small">
                  {{ row.state }}
                </el-tag>
              </template>
            </el-table-column>
            <el-table-column label="操作" width="240">
              <template #default="{ row }">
                <el-button-group>
                  <el-button
                    size="small"
                    type="success"
                    @click="startRemote(client.node_id, row.name)"
                    :disabled="row.state === 'running'"
                  >
                    启动
                  </el-button>
                  <el-button
                    size="small"
                    type="warning"
                    @click="stopRemote(client.node_id, row.name)"
                    :disabled="row.state !== 'running'"
                  >
                    停止
                  </el-button>
                  <el-button
                    size="small"
                    type="danger"
                    @click="deleteRemote(client.node_id, row.name)"
                    :disabled="row.state === 'running'"
                  >
                    删除
                  </el-button>
                </el-button-group>
              </template>
            </el-table-column>
          </el-table>
        </el-card>

        <el-empty v-if="!clients.length" description="暂无 HoneyTea 客户端连接" />
      </el-tab-pane>

      <el-tab-pane label="OrangeTea 前端插件" name="orange">
        <el-card class="panel-card">
          <template #header>
            <div class="panel-header">
              <span>前端插件（OrangeTea 动态页面）</span>
              <el-upload
                :auto-upload="false"
                :show-file-list="false"
                accept=".tar.gz,.tgz"
                @change="handleFrontendInstall"
              >
                <el-button type="primary">
                  <el-icon><Upload /></el-icon>
                  安装前端插件
                </el-button>
              </el-upload>
            </div>
          </template>

          <el-table :data="pluginStore.frontendPlugins" stripe>
            <el-table-column prop="name" label="名称" />
            <el-table-column prop="version" label="版本" width="100" />
            <el-table-column prop="title" label="页面标题" width="180" />
            <el-table-column prop="entry" label="入口文件" />
            <el-table-column label="操作" width="220">
              <template #default="{ row }">
                <el-button-group>
                  <el-button size="small" @click="openPlugin(row.name)">打开页面</el-button>
                  <el-button size="small" type="danger" @click="deleteFrontend(row.name)">
                    删除
                  </el-button>
                </el-button-group>
              </template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-tab-pane>
    </el-tabs>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage, ElMessageBox, type UploadFile } from 'element-plus'
import { usePluginStore } from '@/stores/plugins'
import * as api from '@/api'

const router = useRouter()
const pluginStore = usePluginStore()
const activeTab = ref('lemon')
const clients = ref<any[]>([])

function stateType(state: string) {
  switch (state) {
    case 'running':
      return 'success'
    case 'stopped':
      return 'info'
    case 'crashed':
      return 'danger'
    default:
      return 'warning'
  }
}

function pluginTypeLabel(type?: string) {
  if (type === 'lemon-only') return '仅 LemonTea'
  return '双端插件'
}

async function fetchClients() {
  const resp = await api.getClients()
  clients.value = resp.data.clients || []
}

async function refreshAll() {
  await Promise.all([
    pluginStore.fetchPlugins(),
    pluginStore.fetchFrontendPlugins(),
    fetchClients(),
  ])
}

async function handleStart(name: string) {
  try {
    await pluginStore.startPlugin(name)
    ElMessage.success(`插件 ${name} 已启动`)
  } catch {
    ElMessage.error('启动失败')
  }
}

async function handleStop(name: string) {
  try {
    await pluginStore.stopPlugin(name)
    ElMessage.success(`插件 ${name} 已停止`)
  } catch {
    ElMessage.error('停止失败')
  }
}

async function handleDelete(name: string) {
  try {
    await ElMessageBox.confirm(`确认删除 LemonTea 插件 ${name}？`, '确认')
    await pluginStore.deletePlugin(name)
    ElMessage.success('删除成功')
  } catch {
    // ignored
  }
}

async function handleInstall(uploadFile: UploadFile) {
  if (!uploadFile.raw) return
  try {
    await pluginStore.installPlugin(uploadFile.raw)
    ElMessage.success('LemonTea 插件安装成功')
  } catch {
    ElMessage.error('安装失败')
  }
}

async function refreshClient(_nodeId: string) {
  try {
    await fetchClients()
  } catch {
    ElMessage.error('刷新失败')
  }
}

async function startRemote(nodeId: string, name: string) {
  try {
    await api.startRemotePlugin(nodeId, name)
    ElMessage.success(`已发送启动命令: ${name}`)
    setTimeout(() => refreshClient(nodeId), 800)
  } catch {
    ElMessage.error('操作失败')
  }
}

async function stopRemote(nodeId: string, name: string) {
  try {
    await api.stopRemotePlugin(nodeId, name)
    ElMessage.success(`已发送停止命令: ${name}`)
    setTimeout(() => refreshClient(nodeId), 800)
  } catch {
    ElMessage.error('操作失败')
  }
}

async function deleteRemote(nodeId: string, name: string) {
  try {
    await ElMessageBox.confirm(`确认删除 HoneyTea 插件 ${name}？`, '确认')
    await api.deleteRemotePlugin(nodeId, name)
    ElMessage.success(`已发送删除命令: ${name}`)
    setTimeout(() => refreshClient(nodeId), 1200)
  } catch {
    // ignored
  }
}

async function installToClient(nodeId: string, uploadFile: UploadFile) {
  if (!uploadFile.raw) return
  try {
    await api.installRemotePlugin(nodeId, uploadFile.raw)
    ElMessage.success('插件已发送到客户端')
    setTimeout(() => refreshClient(nodeId), 1200)
  } catch {
    ElMessage.error('安装失败')
  }
}

async function handleFrontendInstall(uploadFile: UploadFile) {
  if (!uploadFile.raw) return
  try {
    await api.installFrontendPlugin(uploadFile.raw)
    ElMessage.success('前端插件安装成功')
    await pluginStore.fetchFrontendPlugins()
  } catch {
    ElMessage.error('安装失败')
  }
}

async function deleteFrontend(name: string) {
  try {
    await ElMessageBox.confirm(`确认删除前端插件 ${name}？`, '确认')
    await api.deleteFrontendPlugin(name)
    ElMessage.success('前端插件已删除')
    await pluginStore.fetchFrontendPlugins()
  } catch {
    // ignored
  }
}

function openPlugin(name: string) {
  router.push(`/plugin/${name}`)
}

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

.panel-card {
  border-radius: 14px;
  border: 1px solid #e9dec8;
  box-shadow: 0 10px 26px rgba(128, 92, 31, 0.08);
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
}

.panel-header-wrap {
  flex-wrap: wrap;
}

.actions-row {
  display: flex;
  gap: 8px;
}

.client-id {
  margin-left: 8px;
}

.client-addr {
  color: #9a8f79;
  margin-left: 8px;
  font-size: 12px;
}

:deep(.el-tabs__item) {
  font-weight: 700;
}
</style>
