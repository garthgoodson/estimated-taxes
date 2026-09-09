<script setup lang="ts">
import type { Jurisdiction } from '~/types/api'
import type { Recommendation } from '~/types/quarter'

const props = defineProps<{ jurisdiction: Jurisdiction; recommendation: Recommendation }>()
const labels = { payment_recommended: 'Payment recommended', catch_up_recommended: 'Catch-up payment recommended', no_payment_currently_needed: 'No payment currently needed', insufficient_information: 'Insufficient information' }
const statusLabel = computed(() => props.recommendation.calculation_status === 'insufficient_information' ? labels.insufficient_information : labels[props.recommendation.recommendation_status ?? 'no_payment_currently_needed'])
</script>

<template>
  <UCard class="recommendation" :data-jurisdiction="jurisdiction">
    <h3>{{ jurisdiction === 'federal' ? 'Federal' : 'California' }}</h3>
    <p class="status">{{ statusLabel }}</p>
    <div class="current-amount"><span>Amount currently owed</span><MoneyDisplay :value="recommendation.recommended_payment_cents" /></div>
    <dl>
      <div><dt>Cumulative target</dt><dd><MoneyDisplay :value="recommendation.cumulative_target_cents" /></dd></div>
      <div><dt>Payments recorded</dt><dd><MoneyDisplay :value="recommendation.payments_credited_cents" /></dd></div>
      <div><dt>Due date</dt><dd>{{ recommendation.due_date ?? 'Unavailable' }}</dd></div>
      <div><dt>Status</dt><dd>{{ recommendation.due_date_status?.replaceAll('_', ' ') ?? 'Unavailable' }}</dd></div>
      <div v-if="recommendation.projected_overpayment"><dt>Projected overpayment</dt><dd><MoneyDisplay :value="recommendation.projected_overpayment_cents" /></dd></div>
    </dl>
  </UCard>
</template>

<style scoped>
.recommendation { display: grid; gap: .75rem; }.recommendation[data-jurisdiction='federal'] { border-top: 3px solid var(--ui-primary); }.recommendation[data-jurisdiction='california'] { border-top: 3px solid var(--ui-secondary); }h3, .status { margin: 0; }h3 { font-size: 1.125rem; }.status { color: var(--ui-text-muted); font-size: .875rem; padding-top: .25rem; }.current-amount { align-items: baseline; border-bottom: 1px solid var(--ui-border); display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); padding-bottom: 1rem; }.current-amount span { color: var(--ui-text-muted); font-size: .875rem; }.current-amount :deep(.money-display) { font-size: 1.25rem; font-weight: 650; }dl { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); margin: 0; padding-top: 1rem; }dl div { display: grid; gap: .25rem; }dt { color: var(--ui-text-muted); font-size: .75rem; }dd { margin: 0; }@media (max-width: 400px) { .current-amount, dl { grid-template-columns: 1fr; } }
</style>
