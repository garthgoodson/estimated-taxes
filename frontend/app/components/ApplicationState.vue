<script setup lang="ts">
import type { ApiError } from '~/types/api'

defineProps<{ pending?: boolean, error?: ApiError | null, empty?: boolean }>()
</script>

<template>
  <div v-if="pending" class="application-state" role="status" aria-live="polite">
    <UIcon name="i-lucide-loader-circle" class="application-state__spinner" />
    <span>Loading 2026 tax estimate…</span>
  </div>
  <UAlert v-else-if="error" icon="i-lucide-circle-x" color="error" title="Unable to load the estimate" :description="error.message">
    <template v-if="error.fields.length" #description>
      <p>{{ error.message }}</p>
      <ul><li v-for="field in error.fields" :key="field.path">{{ field.path }}: {{ field.message }}</li></ul>
    </template>
  </UAlert>
  <UEmpty v-else-if="empty" icon="i-lucide-inbox" title="Nothing to show yet" description="Enter quarter information to begin your estimate." />
  <slot v-else />
</template>

<style scoped>
.application-state { align-items: center; color: var(--ui-text-muted); display: flex; gap: .75rem; justify-content: center; min-height: 12rem; }
.application-state__spinner { animation: spin 1s linear infinite; }
@keyframes spin { to { transform: rotate(360deg); } }
</style>
