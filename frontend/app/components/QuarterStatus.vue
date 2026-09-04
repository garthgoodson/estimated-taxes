<script setup lang="ts">
import type { BootstrapQuarter } from '~/types/api'

defineProps<{ quarter: BootstrapQuarter }>()

const labels = { complete: 'Complete', in_progress: 'In progress', not_started: 'Not started' }
const colors = { complete: 'success', in_progress: 'warning', not_started: 'neutral' } as const
</script>

<template>
  <NuxtLink :to="`/quarters/${quarter.quarter}`" class="quarter-status">
    <span>Q{{ quarter.quarter }}</span>
    <UBadge :color="colors[quarter.status]" variant="subtle">{{ labels[quarter.status] }}</UBadge>
  </NuxtLink>
</template>

<style scoped>
.quarter-status { align-items: center; border: 1px solid var(--ui-border); border-radius: var(--ui-radius); color: inherit; display: flex; justify-content: space-between; padding: .75rem; text-decoration: none; }
.quarter-status:focus-visible { outline: 2px solid var(--ui-primary); outline-offset: 2px; }
</style>
