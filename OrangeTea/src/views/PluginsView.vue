<template>
  <div class="plugins-page">
    <div class="page-header">
      <h2>插件管理</h2>
      <div class="actions">
        <el-upload
          :auto-upload="false"
          :show-file-list="false"
          accept=".tar.gz,.tgz"
          @change="handleInstall"
        >
          <el-button type="primary">
            <el-icon><Plus /></el-icon>
            安装插件
          </el-button>
        </el-upload>

        <el-upload
          :auto-upload="false"
          :show-file-list="false"
          accept=".tar.gz,.tgz"
          @change="handleFrontendInstall"
        >
          <el-button>
            <el-icon><Upload /></el-icon>
            安装前端插件
          </el-button>
        </el-upload>
      </div>
    </div>

    <el-card>
      <el-table :data="pluginStore.localPlugins" stripe v-loading="pluginStore.loading">
        <el-table-column prop="name" label="名称" />
        <el-table-column prop="version" label="版本" width="100" />
        <el-table-column prop="description" label="描述" />
        <el-table-column label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="stateType(row.state)" size="small">
              {{ row.state }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="PID" width="80">
          <template #default="{ row }">
            {{ row.pid || '-' }}
          </template>
        </el-table-column>
        <el-table-column label="前端" width="60">
          <template #default="{ row }">
            <el-icon v-if="row.has_frontend" color="#67c23a"><Check /></el-icon>
            <el-icon v-else color="#ccc"><Close /></el-icon>
          </template>
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
  </div>
</template>

<script setup lang="ts">
import { onMounted } from 'vue'
import { ElMessage, ElMessageBox, type UploadFile } from 'element-plus'
import { usePluginStore } from '@/stores/plugins'
import * as api from '@/api'

const pluginStore = usePluginStore()

function stateType(state: string) {
  switch (state) {
    case 'running': return 'success'
    case 'stopped': return 'info'
    case 'crashed': return 'danger'
    default: return 'warning'
  }
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
    await ElMessageBox.confirm(`确认删除插件 ${name}？`, '确认')
    await pluginStore.deletePlugin(name)
    ElMessage.success('删除成功')
  } catch {}
}

async function handleInstall(uploadFile: UploadFile) {
  if (!uploadFile.raw) return
  try {
    await pluginStore.installPlugin(uploadFile.raw)
    ElMessage.success('插件安装成功')
  } catch {
    ElMessage.error('安装失败')
  }
}

async function handleFrontendInstall(uploadFile: UploadFile) {
  if (!uploadFile.raw) return
  try {
    await api.installFrontendPlugin(uploadFile.raw)
    ElMessage.success('前端插件安装成功')
    pluginStore.fetchFrontendPlugins()
  } catch {
    ElMessage.error('安装失败')
  }
}

onMounted(() => {
  pluginStore.fetchPlugins()
})
</script>

<style scoped>
.page-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
}

.page-header h2 {
  color: #333;
}

.actions {
  display: flex;
  gap: 10px;
}
</style>
