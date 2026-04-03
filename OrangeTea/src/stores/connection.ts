import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import { getClients, getStatus, initApi } from '@/api'

interface HoneyClientInfo {
  node_id: string
  address: string
  port: number
  connected: boolean
  last_heartbeat?: number
}

export const useConnectionStore = defineStore('connection', () => {
  const defaultUrl = 'http://127.0.0.1:9528'
  const serverUrl = ref(localStorage.getItem('tea_server_url') || defaultUrl)
  const connected = ref(false)
  const serverInfo = ref<any>(null)
  const error = ref('')
  const connecting = ref(false)
  const honeyClients = ref<HoneyClientInfo[]>([])
  const refreshingHoneyClients = ref(false)
  const selectedHoneyNodeId = ref(localStorage.getItem('tea_selected_honey_node') || '')

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
      await refreshHoneyClients()
      return true
    } catch (e: any) {
      error.value = e.message || '连接失败'
      connected.value = false
      honeyClients.value = []
      return false
    } finally {
      connecting.value = false
    }
  }

  function setSelectedHoneyNodeId(nodeId: string) {
    selectedHoneyNodeId.value = (nodeId || '').trim()
    if (selectedHoneyNodeId.value) {
      localStorage.setItem('tea_selected_honey_node', selectedHoneyNodeId.value)
    } else {
      localStorage.removeItem('tea_selected_honey_node')
    }
  }

  async function refreshHoneyClients() {
    if (!connected.value) {
      honeyClients.value = []
      return []
    }

    refreshingHoneyClients.value = true
    try {
      const resp = await getClients()
      const list = (resp.data?.clients || []) as HoneyClientInfo[]
      honeyClients.value = list

      if (selectedHoneyNodeId.value) {
        const exists = list.some((c) => c.node_id === selectedHoneyNodeId.value)
        if (!exists) {
          setSelectedHoneyNodeId('')
        }
      }

      if (!selectedHoneyNodeId.value) {
        const firstConnected = list.find((c) => c.connected)
        if (firstConnected) {
          setSelectedHoneyNodeId(firstConnected.node_id)
        }
      }

      return list
    } catch {
      honeyClients.value = []
      return []
    } finally {
      refreshingHoneyClients.value = false
    }
  }

  const selectedHoneyClient = computed(() => {
    if (!selectedHoneyNodeId.value) return null
    return honeyClients.value.find((c) => c.node_id === selectedHoneyNodeId.value) || null
  })

  const honeyConnectedCount = computed(() => honeyClients.value.filter((c) => c.connected).length)

  function disconnect(clearSaved = false) {
    connected.value = false
    serverInfo.value = null
    error.value = ''
    honeyClients.value = []
    if (clearSaved) {
      serverUrl.value = defaultUrl
      localStorage.removeItem('tea_server_url')
      setSelectedHoneyNodeId('')
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
    honeyClients,
    refreshingHoneyClients,
    selectedHoneyNodeId,
    selectedHoneyClient,
    honeyConnectedCount,
    connect,
    disconnect,
    refreshHoneyClients,
    setSelectedHoneyNodeId,
  }
})
