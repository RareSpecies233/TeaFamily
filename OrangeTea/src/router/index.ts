import { createRouter, createWebHistory } from 'vue-router'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      redirect: '/plugins',
    },
    {
      path: '/plugins',
      name: 'plugins',
      component: () => import('@/views/PluginsView.vue'),
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
    {
      path: '/plugin-window/:name',
      name: 'plugin-window',
      component: () => import('@/views/PluginDetailView.vue'),
      props: true,
      meta: {
        hideSidebar: true,
        standalone: true,
      },
    },
  ],
})

export default router
