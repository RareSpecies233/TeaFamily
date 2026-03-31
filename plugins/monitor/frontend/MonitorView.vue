<template>
  <div class="monitor-view">
    <div class="toolbar">
      <el-select v-model="targetNode" placeholder="选择目标客户端" style="width: 200px">
        <el-option
          v-for="c in clients"
          :key="c.node_id"
          :label="`${c.node_id} (${c.address})`"
          :value="c.node_id"
        />
      </el-select>
      <el-button type="primary" @click="startMonitor" :disabled="!targetNode || monitoring">
        开始监控
      </el-button>
      <el-button type="danger" @click="stopMonitor" :disabled="!monitoring">
        停止监控
      </el-button>
    </div>

    <el-row :gutter="16" v-if="latestMetrics">
      <el-col :span="6">
        <el-statistic title="CPU 使用率" :value="latestMetrics.cpu_percent?.toFixed(1)" suffix="%" />
      </el-col>
      <el-col :span="6">
        <el-statistic
          title="内存使用"
          :value="latestMetrics.memory?.used_mb"
          :suffix="'/ ' + latestMetrics.memory?.total_mb + ' MB'"
        />
      </el-col>
      <el-col :span="6">
        <el-statistic
          title="磁盘使用"
          :value="latestMetrics.disks?.[0]?.used_gb?.toFixed(1)"
          :suffix="'/ ' + latestMetrics.disks?.[0]?.total_gb?.toFixed(1) + ' GB'"
        />
      </el-col>
      <el-col :span="6">
        <el-statistic
          title="温度"
          :value="latestMetrics.temperature >= 0 ? latestMetrics.temperature?.toFixed(1) : 'N/A'"
          :suffix="latestMetrics.temperature >= 0 ? '°C' : ''"
        />
      </el-col>
    </el-row>

    <el-row :gutter="16" style="margin-top: 20px">
      <el-col :span="12">
        <el-card>
          <template #header>CPU 使用率</template>
          <div ref="cpuChartRef" style="height: 250px" />
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card>
          <template #header>内存使用</template>
          <div ref="memChartRef" style="height: 250px" />
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, nextTick } from 'vue'
import * as echarts from 'echarts'

const props = defineProps<{
  pluginName: string
  apiBase: string
}>()

const targetNode = ref('')
const clients = ref<any[]>([])
const monitoring = ref(false)
const latestMetrics = ref<any>(null)
const cpuChartRef = ref<HTMLElement>()
const memChartRef = ref<HTMLElement>()

let cpuChart: echarts.ECharts | null = null
let memChart: echarts.ECharts | null = null
let pollTimer: ReturnType<typeof setInterval> | null = null

const cpuHistory = ref<{ time: string; value: number }[]>([])
const memHistory = ref<{ time: string; value: number }[]>([])

async function fetchClients() {
  try {
    const resp = await fetch(`${props.apiBase}/clients`)
    const data = await resp.json()
    clients.value = data.clients || []
  } catch (e) {
    console.error(e)
  }
}

function initCharts() {
  if (cpuChartRef.value) {
    cpuChart = echarts.init(cpuChartRef.value)
    cpuChart.setOption({
      xAxis: { type: 'category', data: [] },
      yAxis: { type: 'value', min: 0, max: 100, axisLabel: { formatter: '{value}%' } },
      series: [{ type: 'line', data: [], smooth: true, areaStyle: { opacity: 0.3 } }],
      tooltip: { trigger: 'axis' },
      grid: { left: 50, right: 20, bottom: 30, top: 20 },
    })
  }
  if (memChartRef.value) {
    memChart = echarts.init(memChartRef.value)
    memChart.setOption({
      xAxis: { type: 'category', data: [] },
      yAxis: { type: 'value', min: 0, axisLabel: { formatter: '{value} MB' } },
      series: [{ type: 'line', data: [], smooth: true, areaStyle: { opacity: 0.3 } }],
      tooltip: { trigger: 'axis' },
      grid: { left: 60, right: 20, bottom: 30, top: 20 },
    })
  }
}

function updateCharts() {
  const maxPoints = 60
  if (cpuChart) {
    const data = cpuHistory.value.slice(-maxPoints)
    cpuChart.setOption({
      xAxis: { data: data.map(d => d.time) },
      series: [{ data: data.map(d => d.value) }],
    })
  }
  if (memChart) {
    const data = memHistory.value.slice(-maxPoints)
    memChart.setOption({
      xAxis: { data: data.map(d => d.time) },
      series: [{ data: data.map(d => d.value) }],
    })
  }
}

async function pollMetrics() {
  if (!targetNode.value) return
  try {
    const resp = await fetch(`${props.apiBase}/plugins/monitor/metrics?node_id=${targetNode.value}`)
    const data = await resp.json()
    if (data && data.cpu_percent !== undefined) {
      latestMetrics.value = data
      const time = new Date().toLocaleTimeString()
      cpuHistory.value.push({ time, value: data.cpu_percent })
      memHistory.value.push({ time, value: data.memory?.used_mb || 0 })
      updateCharts()
    }
  } catch {
    // ignore poll errors
  }
}

async function startMonitor() {
  if (!targetNode.value) return
  try {
    await fetch(`${props.apiBase}/plugins/monitor/start`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        target_node: targetNode.value,
        interval_ms: 1000,
      }),
    })
    monitoring.value = true
    cpuHistory.value = []
    memHistory.value = []
    pollTimer = setInterval(pollMetrics, 1000)
  } catch (e) {
    console.error('Start monitor failed:', e)
  }
}

function stopMonitor() {
  monitoring.value = false
  if (pollTimer) {
    clearInterval(pollTimer)
    pollTimer = null
  }
  if (targetNode.value) {
    fetch(`${props.apiBase}/plugins/monitor/stop`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ target_node: targetNode.value }),
    }).catch(console.error)
  }
}

onMounted(async () => {
  await fetchClients()
  await nextTick()
  initCharts()
  window.addEventListener('resize', () => {
    cpuChart?.resize()
    memChart?.resize()
  })
})

onUnmounted(() => {
  stopMonitor()
  cpuChart?.dispose()
  memChart?.dispose()
})
</script>

<style scoped>
.monitor-view .toolbar {
  display: flex;
  gap: 8px;
  align-items: center;
  margin-bottom: 16px;
}
</style>
