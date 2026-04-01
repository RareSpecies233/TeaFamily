import axios, { type AxiosInstance } from 'axios'

let apiClient: AxiosInstance | null = null

export function initApi(baseUrl: string) {
  apiClient = axios.create({
    baseURL: baseUrl,
    timeout: 30000,
    headers: { 'Content-Type': 'application/json' },
  })
}

function client(): AxiosInstance {
  if (!apiClient) throw new Error('API not initialized')
  return apiClient
}

// Status
export const getStatus = () => client().get('/api/status')

// Clients (HoneyTea instances)
export const getClients = () => client().get('/api/clients')
export const getClientPlugins = (nodeId: string) =>
  client().get(`/api/clients/${nodeId}/plugins`)

// Local plugins
export const getPlugins = () => client().get('/api/plugins')
export const startPlugin = (name: string) =>
  client().post(`/api/plugins/${name}/start`)
export const stopPlugin = (name: string) =>
  client().post(`/api/plugins/${name}/stop`)
export const deletePlugin = (name: string) =>
  client().delete(`/api/plugins/${name}`)

export const installPlugin = (file: File, name?: string) => {
  const fd = new FormData()
  fd.append('plugin', file)
  if (name) fd.append('name', name)
  return client().post('/api/plugins/install', fd, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}

export const inspectUnifiedPlugin = (file: File) => {
  const fd = new FormData()
  fd.append('plugin', file)
  return client().post('/api/plugins/inspect', fd, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}

export const installUnifiedPlugin = (file: File) => {
  const fd = new FormData()
  fd.append('plugin', file)
  return client().post('/api/plugins/install-unified', fd, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}

// Remote plugin operations
export const startRemotePlugin = (nodeId: string, name: string) =>
  client().post(`/api/clients/${nodeId}/plugins/${name}/start`)
export const stopRemotePlugin = (nodeId: string, name: string) =>
  client().post(`/api/clients/${nodeId}/plugins/${name}/stop`)

export const deleteRemotePlugin = (nodeId: string, name: string) =>
  client().delete(`/api/clients/${nodeId}/plugins/${name}`)

export const installRemotePlugin = (nodeId: string, file: File, name?: string) => {
  const fd = new FormData()
  fd.append('plugin', file)
  if (name) fd.append('name', name)
  return client().post(`/api/clients/${nodeId}/plugins/install`, fd, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}

// Plugin messaging
export const sendPluginMessage = (nodeId: string, plugin: string, data: any) =>
  client().post('/api/plugin-message', { node_id: nodeId, plugin, data })

// Binary updates
export const updateLemonTea = (file: File) => {
  const fd = new FormData()
  fd.append('binary', file)
  return client().post('/api/update/lemon-tea', fd, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}

export const updateHoneyTea = (nodeId: string, file: File) => {
  const fd = new FormData()
  fd.append('binary', file)
  return client().post(`/api/update/honey-tea/${nodeId}`, fd, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}

// Frontend plugins
export const getFrontendPlugins = () => client().get('/api/frontend-plugins')

export const getFrontendPluginFile = (name: string, file: string) =>
  client().get(`/api/frontend-plugins/${name}/${file}`, { responseType: 'text' })

export const installFrontendPlugin = (file: File, name?: string) => {
  const fd = new FormData()
  fd.append('plugin', file)
  if (name) fd.append('name', name)
  return client().post('/api/frontend-plugins/install', fd, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}

export const deleteFrontendPlugin = (name: string) =>
  client().delete(`/api/frontend-plugins/${name}`)
