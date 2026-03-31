<template>
  <div class="fm-view">
    <div class="toolbar">
      <el-select v-model="targetNode" placeholder="选择目标客户端" style="width: 200px">
        <el-option
          v-for="c in clients"
          :key="c.node_id"
          :label="`${c.node_id} (${c.address})`"
          :value="c.node_id"
        />
      </el-select>
      <el-breadcrumb separator="/">
        <el-breadcrumb-item
          v-for="(seg, i) in pathSegments"
          :key="i"
          @click="navigateTo(i)"
          style="cursor: pointer"
        >
          {{ seg || '/' }}
        </el-breadcrumb-item>
      </el-breadcrumb>
      <el-button size="small" @click="refresh">
        <el-icon><Refresh /></el-icon>
      </el-button>
      <el-button size="small" type="primary" @click="showNewFolder = true">
        <el-icon><FolderAdd /></el-icon>
        新建文件夹
      </el-button>
    </div>

    <el-table :data="entries" stripe @row-dblclick="handleDoubleClick" v-loading="loading">
      <el-table-column label="名称" prop="name" sortable>
        <template #default="{ row }">
          <el-icon v-if="row.is_dir" style="color: #e6a23c"><Folder /></el-icon>
          <el-icon v-else style="color: #909399"><Document /></el-icon>
          <span style="margin-left: 6px">{{ row.name }}</span>
        </template>
      </el-table-column>
      <el-table-column label="大小" width="120">
        <template #default="{ row }">
          {{ row.is_file ? formatSize(row.size) : '-' }}
        </template>
      </el-table-column>
      <el-table-column label="修改时间" width="180">
        <template #default="{ row }">
          {{ formatTime(row.modified) }}
        </template>
      </el-table-column>
      <el-table-column label="操作" width="180">
        <template #default="{ row }">
          <el-button size="small" v-if="row.is_file" @click="downloadFile(row.name)">
            下载
          </el-button>
          <el-popconfirm title="确认删除？" @confirm="deleteEntry(row.name)">
            <template #reference>
              <el-button size="small" type="danger">删除</el-button>
            </template>
          </el-popconfirm>
        </template>
      </el-table-column>
    </el-table>

    <el-dialog v-model="showNewFolder" title="新建文件夹" width="360px">
      <el-input v-model="newFolderName" placeholder="文件夹名称" />
      <template #footer>
        <el-button @click="showNewFolder = false">取消</el-button>
        <el-button type="primary" @click="createFolder">创建</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, watch } from 'vue'
import { ElMessage } from 'element-plus'

const props = defineProps<{
  pluginName: string
  apiBase: string
}>()

const targetNode = ref('')
const currentPath = ref('/')
const entries = ref<any[]>([])
const clients = ref<any[]>([])
const loading = ref(false)
const showNewFolder = ref(false)
const newFolderName = ref('')

const pathSegments = computed(() => currentPath.value.split('/').filter(Boolean))

async function fetchClients() {
  try {
    const resp = await fetch(`${props.apiBase}/clients`)
    const data = await resp.json()
    clients.value = data.clients || []
  } catch (e) {
    console.error(e)
  }
}

async function listDir(path: string) {
  if (!targetNode.value) return
  loading.value = true
  try {
    const resp = await fetch(`${props.apiBase}/plugins/file-manager/list`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        target_node: targetNode.value,
        path,
      }),
    })
    const data = await resp.json()
    entries.value = data.entries || []
    currentPath.value = path
  } catch (e) {
    ElMessage.error('加载目录失败')
  } finally {
    loading.value = false
  }
}

function refresh() {
  listDir(currentPath.value)
}

function handleDoubleClick(row: any) {
  if (row.is_dir) {
    const newPath = currentPath.value === '/'
      ? `/${row.name}`
      : `${currentPath.value}/${row.name}`
    listDir(newPath)
  }
}

function navigateTo(index: number) {
  const segs = pathSegments.value.slice(0, index + 1)
  listDir('/' + segs.join('/'))
}

async function deleteEntry(name: string) {
  const path = currentPath.value === '/'
    ? `/${name}`
    : `${currentPath.value}/${name}`
  try {
    await fetch(`${props.apiBase}/plugins/file-manager/delete`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ target_node: targetNode.value, path }),
    })
    ElMessage.success('已删除')
    refresh()
  } catch {
    ElMessage.error('删除失败')
  }
}

async function downloadFile(name: string) {
  const path = currentPath.value === '/'
    ? `/${name}`
    : `${currentPath.value}/${name}`
  try {
    const resp = await fetch(`${props.apiBase}/plugins/file-manager/download`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ target_node: targetNode.value, path }),
    })
    const blob = await resp.blob()
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = name
    a.click()
    URL.revokeObjectURL(url)
  } catch {
    ElMessage.error('下载失败')
  }
}

async function createFolder() {
  if (!newFolderName.value) return
  const path = currentPath.value === '/'
    ? `/${newFolderName.value}`
    : `${currentPath.value}/${newFolderName.value}`
  try {
    await fetch(`${props.apiBase}/plugins/file-manager/mkdir`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ target_node: targetNode.value, path }),
    })
    showNewFolder.value = false
    newFolderName.value = ''
    refresh()
  } catch {
    ElMessage.error('创建失败')
  }
}

function formatSize(bytes: number): string {
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB'
  if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + ' MB'
  return (bytes / 1073741824).toFixed(1) + ' GB'
}

function formatTime(ts: number): string {
  if (!ts) return '-'
  return new Date(ts * 1000).toLocaleString()
}

watch(targetNode, (v) => {
  if (v) {
    currentPath.value = '/'
    listDir('/')
  }
})

onMounted(fetchClients)
</script>

<style scoped>
.fm-view .toolbar {
  display: flex;
  gap: 8px;
  align-items: center;
  margin-bottom: 12px;
  flex-wrap: wrap;
}
</style>
