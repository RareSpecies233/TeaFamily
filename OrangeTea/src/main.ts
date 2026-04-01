import { createApp } from 'vue'
import { createPinia } from 'pinia'
import ElementPlus from 'element-plus'
import 'element-plus/dist/index.css'
import * as ElementPlusIconsVue from '@element-plus/icons-vue'
import { Terminal } from '@xterm/xterm'
import { FitAddon } from '@xterm/addon-fit'
import '@xterm/xterm/css/xterm.css'
import App from './App.vue'
import router from './router'
import './styles/main.css'

const app = createApp(App)

// Allow Vue devtools during local development.
if (import.meta.env.DEV) {
  ;(app.config as any).devtools = true
}

;(window as any).__TeaXterm = {
  Terminal,
  FitAddon,
}

app.use(createPinia())
app.use(router)
app.use(ElementPlus)

// Register all Element Plus icons
for (const [key, component] of Object.entries(ElementPlusIconsVue)) {
  app.component(key, component)
}

app.mount('#app')
