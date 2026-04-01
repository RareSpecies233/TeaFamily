<template>
  <div class="connect-page">
    <div class="connect-card">
      <div class="logo">
        <span class="logo-icon">🍊</span>
        <h1>OrangeTea</h1>
        <p class="subtitle">分布式进程管理系统</p>
      </div>

      <el-form @submit.prevent="handleConnect" class="connect-form">
        <el-form-item label="LemonTea 服务器地址">
          <el-input
            v-model="serverAddress"
            placeholder="例如: 127.0.0.1:9528"
            size="large"
            :disabled="connectionStore.connecting"
          >
            <template #prepend>http://</template>
          </el-input>
        </el-form-item>

        <el-alert
          v-if="connectionStore.error"
          :title="connectionStore.error"
          type="error"
          :closable="false"
          style="margin-bottom: 16px"
        />

        <el-button
          type="primary"
          size="large"
          :loading="connectionStore.connecting"
          @click="handleConnect"
          style="width: 100%"
        >
          {{ connectionStore.connecting ? '连接中...' : '连接' }}
        </el-button>
      </el-form>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useConnectionStore } from '@/stores/connection'

const connectionStore = useConnectionStore()
const serverAddress = ref('127.0.0.1:9528')

async function handleConnect() {
  if (!serverAddress.value.trim()) return
  await connectionStore.connect(serverAddress.value)
}
</script>

<style scoped>
.connect-page {
  height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  background:
    radial-gradient(circle at 15% 20%, rgba(255, 206, 126, 0.55), transparent 35%),
    radial-gradient(circle at 85% 80%, rgba(139, 230, 214, 0.45), transparent 38%),
    linear-gradient(135deg, #fff6e7 0%, #f2efe8 60%, #e9f8f5 100%);
}

.connect-card {
  background: rgba(255, 255, 255, 0.86);
  border-radius: 20px;
  padding: 48px;
  width: 440px;
  border: 1px solid #eadcbc;
  backdrop-filter: blur(4px);
  box-shadow: 0 26px 60px rgba(116, 86, 36, 0.2);
}

.logo {
  text-align: center;
  margin-bottom: 32px;
}

.logo-icon {
  font-size: 64px;
  display: block;
  margin-bottom: 8px;
}

.logo h1 {
  font-size: 30px;
  color: #2f2310;
  margin: 0;
}

.subtitle {
  color: #8a7553;
  font-size: 14px;
  margin-top: 4px;
}
</style>
