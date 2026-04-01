<template>
  <div class="update-page">
    <header class="hero">
      <div class="hero-left">
      <h2>程序更新</h2>
      </div>
    </header>
    
    <el-row :gutter="20">
      <el-col :span="12">
        <el-card>
          <template #header>
            <strong>更新 LemonTea（服务端）</strong>
          </template>
          <p class="tip">上传新的 LemonTea 可执行文件，GreenTea 将自动完成重启。</p>
          <el-upload
            ref="lemonUploadRef"
            :auto-upload="false"
            :limit="1"
            :on-exceed="() => ElMessage.warning('只能选择一个文件')"
            drag
          >
            <el-icon class="upload-icon"><Upload /></el-icon>
            <div class="el-upload__text">拖拽文件到此处，或 <em>点击上传</em></div>
          </el-upload>
          <el-button
            type="primary"
            style="margin-top: 12px"
            :loading="lemonUploading"
            @click="uploadLemonTea"
          >
            上传并更新
          </el-button>
        </el-card>
      </el-col>

      <el-col :span="12">
        <el-card>
          <template #header>
            <strong>更新 HoneyTea（客户端）</strong>
          </template>
          <p class="tip">选择目标客户端，上传新的 HoneyTea 可执行文件。</p>
          <el-select
            v-model="selectedClient"
            placeholder="选择目标客户端"
            style="width: 100%; margin-bottom: 12px"
          >
            <el-option
              v-for="c in clients"
              :key="c.node_id"
              :label="`${c.node_id} (${c.address})`"
              :value="c.node_id"
            />
          </el-select>
          <el-upload
            ref="honeyUploadRef"
            :auto-upload="false"
            :limit="1"
            :on-exceed="() => ElMessage.warning('只能选择一个文件')"
            drag
          >
            <el-icon class="upload-icon"><Upload /></el-icon>
            <div class="el-upload__text">拖拽文件到此处，或 <em>点击上传</em></div>
          </el-upload>
          <el-button
            type="primary"
            style="margin-top: 12px"
            :loading="honeyUploading"
            :disabled="!selectedClient"
            @click="uploadHoneyTea"
          >
            上传并更新
          </el-button>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { ElMessage, type UploadInstance } from 'element-plus'
import * as api from '@/api'

const lemonUploadRef = ref<UploadInstance>()
const honeyUploadRef = ref<UploadInstance>()
const lemonUploading = ref(false)
const honeyUploading = ref(false)
const selectedClient = ref('')
const clients = ref<any[]>([])

async function fetchClients() {
  try {
    const resp = await api.getClients()
    clients.value = resp.data.clients || []
  } catch (e) {
    console.error(e)
  }
}

async function uploadLemonTea() {
  const files = (lemonUploadRef.value as any)?.$el?.querySelector('input')?.files
  if (!files?.length) {
    ElMessage.warning('请先选择文件')
    return
  }
  lemonUploading.value = true
  try {
    await api.updateLemonTea(files[0])
    ElMessage.success('LemonTea 更新文件已上传，即将重启')
    lemonUploadRef.value?.clearFiles()
  } catch {
    ElMessage.error('上传失败')
  } finally {
    lemonUploading.value = false
  }
}

async function uploadHoneyTea() {
  if (!selectedClient.value) return
  const files = (honeyUploadRef.value as any)?.$el?.querySelector('input')?.files
  if (!files?.length) {
    ElMessage.warning('请先选择文件')
    return
  }
  honeyUploading.value = true
  try {
    await api.updateHoneyTea(selectedClient.value, files[0])
    ElMessage.success('HoneyTea 更新文件已发送到客户端')
    honeyUploadRef.value?.clearFiles()
  } catch {
    ElMessage.error('上传失败')
  } finally {
    honeyUploading.value = false
  }
}

onMounted(fetchClients)
</script>

<style scoped>
.update-page h2 {
  color: #2f2310;
  margin-bottom: 20px;
}

.tip {
  color: #7f6841;
  font-size: 13px;
  margin-bottom: 12px;
}

.upload-icon {
  font-size: 40px;
  color: #c8ae7f;
}

:deep(.el-card) {
  border-radius: 14px;
  border: 1px solid #e9dec8;
  box-shadow: 0 10px 26px rgba(128, 92, 31, 0.08);
}
</style>
