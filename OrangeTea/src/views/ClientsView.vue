<template>
  <div class="clients-page">
    <h2>客户端管理</h2>

    <el-card v-for="client in clients" :key="client.node_id" class="client-card">
      <template #header>
        <div class="client-header">
          <span>
            <el-tag :type="client.connected ? 'success' : 'danger'" size="small">
              {{ client.connected ? '在线' : '离线' }}
            </el-tag>
            <strong style="margin-left: 8px">{{ client.node_id }}</strong>
            <span class="client-addr">{{ client.address }}:{{ client.port }}</span>
          </span>
          <el-button size="small" @click="refreshClient(client.node_id)">
            <el-icon><Refresh /></el-icon>
            刷新插件
          </el-button>
        </div>
      </template>

      <el-table :data="client.plugins || []" stripe size="small">
        <el-table-column prop="name" label="插件名称" />
        <el-table-column prop="state" label="状态" width="100">
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

      <div style="margin-top: 12px">
        <el-upload
          :auto-upload="false"
          :show-file-list="false"
          accept=".tar.gz,.tgz"
          @change="(f: any) => installToClient(client.node_id, f)"
        >
          <el-button type="primary" size="small">
            <el-icon><Plus /></el-icon>
            安装插件到此客户端
          </el-button>
        </el-upload>
      </div>
    </el-card>

    <el-empty v-if="!clients.length" description="暂无客户端连接" />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { ElMessage, type UploadFile } from 'element-plus'
import * as api from '@/api'

const clients = ref<any[]>([])

async function fetchClients() {
  try {
    const resp = await api.getClients()
    clients.value = resp.data.clients || []
  } catch (e) {
    console.error(e)
  }
}

async function refreshClient(nodeId: string) {
  try {
    const resp = await api.getClientPlugins(nodeId)
    const client = clients.value.find(c => c.node_id === nodeId)
    if (client) {
      client.plugins = resp.data.plugins || []
    }
  } catch (e) {
    console.error(e)
  }
}

async function startRemote(nodeId: string, name: string) {
  try {
    await api.startRemotePlugin(nodeId, name)
    ElMessage.success(`已发送启动命令: ${name}`)
    setTimeout(() => refreshClient(nodeId), 1000)
  } catch {
    ElMessage.error('操作失败')
  }
}

async function stopRemote(nodeId: string, name: string) {
  try {
    await api.stopRemotePlugin(nodeId, name)
    ElMessage.success(`已发送停止命令: ${name}`)
    setTimeout(() => refreshClient(nodeId), 1000)
  } catch {
    ElMessage.error('操作失败')
  }
}

async function deleteRemote(nodeId: string, name: string) {
  try {
    await api.deleteRemotePlugin(nodeId, name)
    ElMessage.success(`已发送删除命令: ${name}`)
    setTimeout(() => refreshClient(nodeId), 1200)
  } catch {
    ElMessage.error('删除失败')
  }
}

async function installToClient(nodeId: string, uploadFile: UploadFile) {
  if (!uploadFile.raw) return
  try {
    await api.installRemotePlugin(nodeId, uploadFile.raw)
    ElMessage.success('插件已发送到客户端')
    setTimeout(() => refreshClient(nodeId), 2000)
  } catch {
    ElMessage.error('安装失败')
  }
}

let timer: ReturnType<typeof setInterval>
onMounted(() => {
  fetchClients()
  timer = setInterval(fetchClients, 10000)
})
onUnmounted(() => clearInterval(timer))
</script>

<style scoped>
.clients-page h2 {
  color: #333;
  margin-bottom: 20px;
}

.client-card {
  margin-bottom: 16px;
}

.client-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.client-addr {
  color: #999;
  font-size: 12px;
  margin-left: 8px;
}
</style>
