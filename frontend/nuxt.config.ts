export default defineNuxtConfig({
  compatibilityDate: '2026-09-07',
  modules: ['@nuxt/eslint', '@nuxt/ui'],
  css: ['~/assets/css/main.css'],
  colorMode: {
    preference: 'dark',
    fallback: 'dark',
    classSuffix: ''
  },
  runtimeConfig: {
    backendOrigin: 'http://127.0.0.1:8080',
    public: {
      apiBase: '/api'
    }
  },
  devtools: { enabled: true }
})
