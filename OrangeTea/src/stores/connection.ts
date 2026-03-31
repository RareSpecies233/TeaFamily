import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { initApi, getStatus } from '@/api'

export const useConnectionStore = defineStore('connection', () => {
  const serverUrl = ref(localStorage.getItem('tea_server_url') || '')
  const connected = ref(false)
  const serverInfo = ref<any>(null)
  const error = ref('')
  const connecting = ref(false)

  async function connect(url: string) {
    connecting.value = true
    error.value = ''
    try {
      // Normalize URL
      let baseUrl = url.trim()
      if (!baseUrl.startsWith('http')) {
        baseUrl = 'http://' + baseUrl
      }
      // Remove trailing slash
      baseUrl = baseUrl.replace(/\/+$/, '')

      initApi(baseUrl)
      const resp = await getStatus()
      serverInfo.value = resp.data
      serverUrl.value = baseUrl
      connected.value = true
      localStorage.setItem('tea_server_url', baseUrl)
    } catch (e: any) {
      error.value = e.message || '连接失败'
      connected.value = false
    } finally {
      connecting.value = false
    }
  }

  function disconnect() {
    connected.value = false
    serverInfo.value = null
    serverUrl.value = ''
    localStorage.removeItem('tea_server_url')
  }

  // Auto-reconnect on load
  const savedUrl = localStorage.getItem('tea_server_url')
  if (savedUrl) {
    connect(savedUrl)
  }

  return {
    serverUrl,
    connected,
    serverInfo,
    error,
    connecting,
    connect,
    disconnect,
  }
})
