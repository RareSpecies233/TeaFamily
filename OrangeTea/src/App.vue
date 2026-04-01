<template>
  <div class="app-container">
    <Sidebar />
    <main class="main-content">
      <router-view />
    </main>
  </div>
</template>

<script setup lang="ts">
import Sidebar from '@/components/Sidebar.vue'
</script>

<style scoped>
.app-container {
  display: flex;
  height: 100vh;
  overflow: hidden;
  min-width: 0;
}

.main-content {
  flex: 1;
  min-width: 0;
  overflow: auto;
  padding: 20px;
  background:
    radial-gradient(circle at 100% 0, rgba(255, 213, 138, 0.35), transparent 30%),
    linear-gradient(180deg, #fffaf0 0%, #f7f2e7 100%);
}
</style>
