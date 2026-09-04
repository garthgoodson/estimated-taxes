<script setup lang="ts">
import type { Cents } from '~/types/api'

withDefaults(defineProps<{
  label: string
  amount: Cents | null | undefined
  meaning?: 'actual' | 'projected' | 'paid' | 'recommended'
  supportingText?: string
}>(), { meaning: undefined, supportingText: undefined })
</script>

<template>
  <section class="metric-summary">
    <p class="metric-summary__label">{{ label }}</p>
    <MoneyDisplay :value="amount" :meaning="meaning" />
    <p v-if="supportingText" class="metric-summary__support">{{ supportingText }}</p>
  </section>
</template>

<style scoped>
.metric-summary { display: grid; gap: .35rem; }
.metric-summary__label, .metric-summary__support { margin: 0; color: var(--ui-text-muted); font-size: .875rem; }
.metric-summary :deep(.money-display) { font-size: 1.25rem; font-weight: 650; }
</style>
