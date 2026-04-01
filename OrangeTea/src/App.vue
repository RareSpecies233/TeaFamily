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
  background:
    radial-gradient(circle at 100% 0, rgba(255, 213, 138, 0.35), transparent 30%),
    linear-gradient(180deg, #fffaf0 0%, #f7f2e7 100%);
}
</style>
