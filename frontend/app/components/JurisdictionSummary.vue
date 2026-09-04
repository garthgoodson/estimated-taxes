<script setup lang="ts">
import type { Cents, Jurisdiction } from '~/types/api'

defineProps<{ jurisdiction: Jurisdiction, label: string, amount: Cents | null, detail?: string }>()
</script>

<template>
  <UCard class="jurisdiction-summary" :data-jurisdiction="jurisdiction">
    <p class="jurisdiction-summary__eyebrow">{{ jurisdiction === 'federal' ? 'Federal' : 'California' }}</p>
    <MetricSummary :label="label" :amount="amount" meaning="recommended" :supporting-text="detail" />
  </UCard>
</template>

<style scoped>
.jurisdiction-summary__eyebrow { margin: 0 0 .75rem; color: var(--ui-text-muted); font-size: .75rem; font-weight: 700; letter-spacing: .08em; text-transform: uppercase; }
.jurisdiction-summary[data-jurisdiction='federal'] { border-top: 2px solid var(--ui-primary); }
.jurisdiction-summary[data-jurisdiction='california'] { border-top: 2px solid var(--ui-secondary); }
</style>
