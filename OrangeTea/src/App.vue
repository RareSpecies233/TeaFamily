<template>
  <div class="app-container" :class="{ 'app-container--standalone': hideSidebar }">
    <Sidebar v-if="!hideSidebar" />
    <main class="main-content" :class="{ 'main-content--standalone': hideSidebar }">
      <router-view />
    </main>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import Sidebar from '@/components/Sidebar.vue'

const route = useRoute()
const hideSidebar = computed(() => Boolean(route.meta.hideSidebar))
</script>

<style scoped>
.app-container {
  display: flex;
  height: 100vh;
  overflow: hidden;
  min-width: 0;
}

.app-container--standalone {
  display: block;
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

.main-content--standalone {
  height: 100vh;
  padding: 14px;
}
</style>
