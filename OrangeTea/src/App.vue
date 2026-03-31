<template>
  <div class="app-container" v-if="connectionStore.connected">
    <Sidebar />
    <main class="main-content">
      <router-view />
    </main>
  </div>
  <ConnectView v-else />
</template>

<script setup lang="ts">
import { useConnectionStore } from '@/stores/connection'
import Sidebar from '@/components/Sidebar.vue'
import ConnectView from '@/views/ConnectView.vue'

const connectionStore = useConnectionStore()
</script>

<style scoped>
.app-container {
  display: flex;
  height: 100vh;
  overflow: hidden;
}

.main-content {
  flex: 1;
  overflow: auto;
  padding: 20px;
  background: #f5f7fa;
}
</style>
