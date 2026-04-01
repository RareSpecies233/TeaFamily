import { defineStore } from 'pinia'
import { ref } from 'vue'
import * as api from '@/api'

export interface PluginInfo {
  name: string
  version: string
  description: string
  plugin_type?: string
  state: string
  pid: number
  has_frontend: boolean
  auto_start: boolean
}

export interface FrontendPlugin {
  name: string
  version: string
  description: string
  entry: string
  route: string
  icon: string
  title: string
}

export const usePluginStore = defineStore('plugins', () => {
  const localPlugins = ref<PluginInfo[]>([])
  const frontendPlugins = ref<FrontendPlugin[]>([])
  const loading = ref(false)

  async function fetchPlugins() {
    loading.value = true
    try {
      const resp = await api.getPlugins()
      localPlugins.value = resp.data.plugins || []
    } catch (e) {
      console.error('Failed to fetch plugins:', e)
    } finally {
      loading.value = false
    }
  }

  async function fetchFrontendPlugins() {
    try {
      const resp = await api.getFrontendPlugins()
      frontendPlugins.value = resp.data.plugins || []
    } catch (e) {
      console.error('Failed to fetch frontend plugins:', e)
    }
  }

  async function startPlugin(name: string) {
    await api.startPlugin(name)
    await fetchPlugins()
  }

  async function stopPlugin(name: string) {
    await api.stopPlugin(name)
    await fetchPlugins()
  }

  async function installPlugin(file: File, name?: string) {
    await api.installPlugin(file, name)
    await fetchPlugins()
  }

  async function deletePlugin(name: string) {
    await api.deletePlugin(name)
    await fetchPlugins()
  }

  return {
    localPlugins,
    frontendPlugins,
    loading,
    fetchPlugins,
    fetchFrontendPlugins,
    startPlugin,
    stopPlugin,
    installPlugin,
    deletePlugin,
  }
})
