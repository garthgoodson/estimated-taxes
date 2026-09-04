<script setup lang="ts">
const bootstrap = useBootstrap()
const quarterLink = computed(() => `/quarters/${bootstrap.state.value?.current_quarter ?? 1}`)

onMounted(() => {
  if (!bootstrap.state.value && !bootstrap.pending.value) void bootstrap.load()
})

const items = computed(() => [
  { label: 'Home', icon: 'i-lucide-house', to: '/' },
  { label: 'Quarters', icon: 'i-lucide-calendar-days', to: quarterLink.value },
  { label: 'History', icon: 'i-lucide-history', to: '/history' },
  { label: 'Settings', icon: 'i-lucide-settings', to: '/settings' }
])
</script>

<template>
  <UDashboardGroup>
    <UDashboardSidebar :ui="{ root: 'bg-elevated/70' }">
      <template #header>
        <NuxtLink to="/" class="brand">Estimated Taxes</NuxtLink>
      </template>
      <UNavigationMenu orientation="vertical" :items="items" />
      <template #footer>
        <p class="sidebar-note">2026 · California MFJ</p>
      </template>
    </UDashboardSidebar>

    <UDashboardPanel>
      <UDashboardNavbar title="Estimated Taxes">
        <template #leading><UDashboardSidebarToggle /></template>
      </UDashboardNavbar>
      <main class="page-content">
        <ApplicationState :pending="bootstrap.pending.value" :error="bootstrap.error.value">
          <slot />
        </ApplicationState>
      </main>
    </UDashboardPanel>
  </UDashboardGroup>
</template>

<style scoped>
.brand { color: var(--ui-text-highlighted); font-weight: 700; text-decoration: none; }
.sidebar-note { color: var(--ui-text-muted); font-size: .75rem; margin: 0; }
.page-content { margin: 0 auto; max-width: 76rem; padding: 1.5rem; width: 100%; }
</style>
