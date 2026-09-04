<script setup lang="ts">
import type { ApiWarning } from '~/types/api'

defineProps<{ warnings: ApiWarning[] }>()

const icon = { blocking: 'i-lucide-circle-x', caution: 'i-lucide-triangle-alert', information: 'i-lucide-info', informational: 'i-lucide-info' }
const color = { blocking: 'error', caution: 'warning', information: 'info', informational: 'info' } as const
</script>

<template>
  <ul v-if="warnings.length" class="warning-list" aria-label="Estimate notices">
    <li v-for="warning in warnings" :key="`${warning.code}-${warning.path}`">
      <UAlert :icon="icon[warning.severity]" :color="color[warning.severity]" :title="warning.code" :description="warning.message" />
    </li>
  </ul>
</template>

<style scoped>
.warning-list { display: grid; gap: .75rem; margin: 0; padding: 0; list-style: none; }
</style>
