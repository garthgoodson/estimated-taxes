const backendOrigin = process.env.NUXT_BACKEND_ORIGIN || 'http://127.0.0.1:8080'
const backendUrl = process.env.NUXT_PUBLIC_API_BASE || `${backendOrigin}/api`

export default defineNuxtConfig({
  modules: ['@nuxt/eslint', '@nuxt/ui'],
  css: ['~/assets/css/main.css'],
  colorMode: {
    preference: 'dark',
    fallback: 'dark',
    classSuffix: ''
  },
  runtimeConfig: {
    public: {
      apiBase: import.meta.dev ? '/api' : backendUrl
    }
  },
  nitro: {
    devProxy: {
      '/api': {
        target: backendOrigin,
        changeOrigin: true
      }
    }
  },
  devtools: { enabled: true }
})
