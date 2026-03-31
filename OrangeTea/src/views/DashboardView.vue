<template>
  <div class="dashboard">
    <h2>仪表盘</h2>

    <el-row :gutter="20" class="stats-row">
      <el-col :span="6">
        <el-card shadow="hover">
          <template #header>服务器状态</template>
          <div class="stat-value">
            <el-tag :type="serverInfo?.status === 'running' ? 'success' : 'danger'" size="large">
              {{ serverInfo?.status || '未知' }}
            </el-tag>
          </div>
        </el-card>
      </el-col>

      <el-col :span="6">
        <el-card shadow="hover">
          <template #header>连接客户端</template>
          <div class="stat-value">{{ clients.length }}</div>
        </el-card>
      </el-col>

      <el-col :span="6">
        <el-card shadow="hover">
          <template #header>本地插件</template>
          <div class="stat-value">{{ plugins.length }}</div>
        </el-card>
      </el-col>

      <el-col :span="6">
        <el-card shadow="hover">
          <template #header>运行中插件</template>
          <div class="stat-value">{{ runningPlugins }}</div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="20" style="margin-top: 20px">
      <el-col :span="12">
        <el-card>
          <template #header>客户端列表</template>
          <el-table :data="clients" stripe size="small">
            <el-table-column prop="node_id" label="节点ID" />
            <el-table-column prop="address" label="地址" />
            <el-table-column label="状态" width="100">
              <template #default="{ row }">
                <el-tag :type="row.connected ? 'success' : 'danger'" size="small">
                  {{ row.connected ? '在线' : '离线' }}
                </el-tag>
              </template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>

      <el-col :span="12">
        <el-card>
          <template #header>本地插件状态</template>
          <el-table :data="plugins" stripe size="small">
            <el-table-column prop="name" label="名称" />
            <el-table-column prop="version" label="版本" width="80" />
            <el-table-column label="状态" width="100">
              <template #default="{ row }">
                <el-tag :type="stateType(row.state)" size="small">
                  {{ row.state }}
                </el-tag>
              </template>
            </el-table-column>
          </el-table>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useConnectionStore } from '@/stores/connection'
import { usePluginStore } from '@/stores/plugins'
import * as api from '@/api'

const connectionStore = useConnectionStore()
const pluginStore = usePluginStore()

const serverInfo = computed(() => connectionStore.serverInfo)
const plugins = computed(() => pluginStore.localPlugins)
const clients = ref<any[]>([])

const runningPlugins = computed(() =>
  plugins.value.filter((p) => p.state === 'running').length
)

function stateType(state: string) {
  switch (state) {
    case 'running': return 'success'
    case 'stopped': return 'info'
    case 'crashed': return 'danger'
    default: return 'warning'
  }
}

let timer: ReturnType<typeof setInterval>

async function refresh() {
  pluginStore.fetchPlugins()
  try {
    const resp = await api.getClients()
    clients.value = resp.data.clients || []
  } catch (e) {
    console.error(e)
  }
}

onMounted(() => {
  refresh()
  timer = setInterval(refresh, 5000)
})

onUnmounted(() => {
  clearInterval(timer)
})
</script>

<style scoped>
.dashboard h2 {
  margin-bottom: 20px;
  color: #333;
}

.stats-row .el-card {
  text-align: center;
}

.stat-value {
  font-size: 28px;
  font-weight: bold;
  color: #333;
  padding: 10px 0;
}
</style>
