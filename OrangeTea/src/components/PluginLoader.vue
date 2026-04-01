<template>
  <div class="plugin-loader">
    <el-skeleton v-if="loading" :rows="6" animated class="plugin-loading" />
    <div ref="containerRef" class="plugin-container" />
    <el-alert
      v-if="loadError"
      :title="loadError"
      type="error"
      show-icon
    />
    <el-empty v-else-if="!loading && !rendered" description="插件页面暂无可展示内容" />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, h, nextTick, render as vueRender } from 'vue'

const props = defineProps<{
  url: string
  pluginName: string
  apiBase?: string
}>()

const containerRef = ref<HTMLElement | null>(null)
const loadError = ref('')
const loading = ref(false)
const rendered = ref(false)
let cleanup: (() => void) | null = null
let loadToken = 0

async function loadPlugin() {
  const token = ++loadToken
  loading.value = true
  loadError.value = ''
  rendered.value = false

  await nextTick()

  if (token !== loadToken) return

  const container = containerRef.value
  if (!container) {
    loadError.value = '插件容器初始化失败'
    loading.value = false
    return
  }

  if (cleanup) {
    cleanup()
    cleanup = null
  }

  container.innerHTML = ''

  try {
    const module = await import(/* @vite-ignore */ props.url)

    if (token !== loadToken) return

    if (module.mount && typeof module.mount === 'function') {
      // Plugin exports a mount function: mount(container, context)
      const ctx = {
        pluginName: props.pluginName,
        apiBase: props.apiBase || '/api',
      }
      const unmount = await module.mount(container, ctx)
      if (typeof unmount === 'function') {
        cleanup = unmount
      }
      rendered.value = true
    } else if (module.default) {
      // Plugin exports a Vue component
      const vnode = h(module.default, {
        pluginName: props.pluginName,
        apiBase: props.apiBase || '/api',
      })
      vueRender(vnode, container)
      cleanup = () => {
        if (containerRef.value) {
          vueRender(null, containerRef.value)
        }
      }
      rendered.value = true
    } else {
      loadError.value = '插件模块格式无效：需要导出 mount 函数或默认 Vue 组件'
    }
  } catch (e: any) {
    loadError.value = '加载插件失败: ' + (e.message || '未知错误')
  } finally {
    if (token === loadToken) {
      loading.value = false
    }
  }
}

watch(
  () => [props.url, props.pluginName, props.apiBase],
  () => {
    if (!props.url) {
      loadError.value = '插件入口地址为空'
      loading.value = false
      rendered.value = false
      return
    }
    loadPlugin()
  },
  { immediate: true }
)

onMounted(() => {
  if (props.url && !rendered.value && !loading.value) {
    loadPlugin()
  }
})

onUnmounted(() => {
  loadToken++
  if (cleanup) cleanup()
})
</script>

<style scoped>
.plugin-loader {
  width: 100%;
}

.plugin-loading {
  margin-bottom: 12px;
}

.plugin-container {
  width: 100%;
  min-height: 400px;
}
</style>
