import { createRouter, createWebHistory } from 'vue-router'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      name: 'dashboard',
      component: () => import('@/views/DashboardView.vue'),
    },
    {
      path: '/plugins',
      name: 'plugins',
      component: () => import('@/views/PluginsView.vue'),
    },
    {
      path: '/clients',
      name: 'clients',
      component: () => import('@/views/ClientsView.vue'),
    },
    {
      path: '/update',
      name: 'update',
      component: () => import('@/views/UpdateView.vue'),
    },
    {
      path: '/plugin/:name',
      name: 'plugin-detail',
      component: () => import('@/views/PluginDetailView.vue'),
      props: true,
    },
  ],
})

export default router
