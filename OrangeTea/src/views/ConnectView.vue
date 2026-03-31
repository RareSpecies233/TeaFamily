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
  background: linear-gradient(135deg, #667eea 0%, #e6a23c 100%);
}

.connect-card {
  background: white;
  border-radius: 16px;
  padding: 48px;
  width: 440px;
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.15);
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
  font-size: 28px;
  color: #333;
  margin: 0;
}

.subtitle {
  color: #999;
  font-size: 14px;
  margin-top: 4px;
}
</style>
