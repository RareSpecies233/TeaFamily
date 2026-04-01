import { defineStore } from 'pinia'
import { ref } from 'vue'
import { initApi, getStatus } from '@/api'

export const useConnectionStore = defineStore('connection', () => {
  const defaultUrl = 'http://127.0.0.1:9528'
  const serverUrl = ref(localStorage.getItem('tea_server_url') || defaultUrl)
  const connected = ref(false)
  const serverInfo = ref<any>(null)
  const error = ref('')
  const connecting = ref(false)

  function normalizeUrl(url: string) {
    let baseUrl = url.trim()
    if (!baseUrl) {
      baseUrl = defaultUrl
    }
    if (!baseUrl.startsWith('http')) {
      baseUrl = 'http://' + baseUrl
    }
    return baseUrl.replace(/\/+$/, '')
  }

  async function connect(url: string = serverUrl.value) {
    connecting.value = true
    error.value = ''
    try {
      const baseUrl = normalizeUrl(url)
      initApi(baseUrl)
      const resp = await getStatus()
      serverInfo.value = resp.data
      serverUrl.value = baseUrl
      connected.value = true
      localStorage.setItem('tea_server_url', baseUrl)
      return true
    } catch (e: any) {
      error.value = e.message || '连接失败'
      connected.value = false
      return false
    } finally {
      connecting.value = false
    }
  }

  function disconnect(clearSaved = false) {
    connected.value = false
    serverInfo.value = null
    error.value = ''
    if (clearSaved) {
      serverUrl.value = defaultUrl
      localStorage.removeItem('tea_server_url')
    }
  }

  // Auto-reconnect on load
  const savedUrl = localStorage.getItem('tea_server_url')
  initApi(normalizeUrl(savedUrl || defaultUrl))
  if (savedUrl) connect(savedUrl)

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
