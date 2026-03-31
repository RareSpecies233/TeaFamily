<template>
  <div class="plugin-loader">
    <div ref="containerRef" class="plugin-container" />
    <el-alert
      v-if="loadError"
      :title="loadError"
      type="error"
      show-icon
    />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, defineAsyncComponent, h, render as vueRender } from 'vue'

const props = defineProps<{
  url: string
  pluginName: string
}>()

const containerRef = ref<HTMLElement>()
const loadError = ref('')
let cleanup: (() => void) | null = null

async function loadPlugin() {
  if (!containerRef.value) return
  loadError.value = ''

  try {
    const module = await import(/* @vite-ignore */ props.url)

    if (module.mount && typeof module.mount === 'function') {
      // Plugin exports a mount function: mount(container, context)
      const ctx = {
        pluginName: props.pluginName,
        apiBase: '/api',
      }
      const unmount = await module.mount(containerRef.value, ctx)
      if (typeof unmount === 'function') {
        cleanup = unmount
      }
    } else if (module.default) {
      // Plugin exports a Vue component
      const vnode = h(module.default, {
        pluginName: props.pluginName,
        apiBase: '/api',
      })
      vueRender(vnode, containerRef.value)
      cleanup = () => vueRender(null, containerRef.value!)
    } else {
      loadError.value = '插件模块格式无效：需要导出 mount 函数或默认 Vue 组件'
    }
  } catch (e: any) {
    loadError.value = '加载插件失败: ' + (e.message || '未知错误')
  }
}

onMounted(loadPlugin)
onUnmounted(() => {
  if (cleanup) cleanup()
})
</script>

<style scoped>
.plugin-loader {
  width: 100%;
}

.plugin-container {
  width: 100%;
  min-height: 400px;
}
</style>
