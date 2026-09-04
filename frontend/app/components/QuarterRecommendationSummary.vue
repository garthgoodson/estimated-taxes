<script setup lang="ts">
import type { Jurisdiction } from '~/types/api'
import type { Recommendation } from '~/types/quarter'

const props = defineProps<{ jurisdiction: Jurisdiction; recommendation: Recommendation }>()
const labels = { payment_recommended: 'Payment recommended', catch_up_recommended: 'Catch-up payment recommended', no_payment_currently_needed: 'No payment currently needed', insufficient_information: 'Insufficient information' }
const statusLabel = computed(() => props.recommendation.calculation_status === 'insufficient_information' ? labels.insufficient_information : labels[props.recommendation.recommendation_status ?? 'no_payment_currently_needed'])
</script>

<template>
  <UCard class="recommendation" :data-jurisdiction="jurisdiction">
    <p class="eyebrow">{{ jurisdiction === 'federal' ? 'Federal' : 'California' }}</p>
    <h2>Estimated taxes owed</h2>
    <p class="status">{{ statusLabel }}</p>
    <MetricSummary label="Amount currently owed" :amount="recommendation.recommended_payment_cents" meaning="recommended" />
    <dl>
      <div><dt>Cumulative target</dt><dd><MoneyDisplay :value="recommendation.cumulative_target_cents" /></dd></div>
      <div><dt>Payments already recorded</dt><dd><MoneyDisplay :value="recommendation.payments_credited_cents" meaning="paid" /></dd></div>
      <div><dt>Due date</dt><dd>{{ recommendation.due_date ?? 'Unavailable' }}</dd></div>
      <div><dt>Due-date status</dt><dd>{{ recommendation.due_date_status?.replaceAll('_', ' ') ?? 'Unavailable' }}</dd></div>
      <div v-if="recommendation.projected_overpayment"><dt>Projected overpayment</dt><dd><MoneyDisplay :value="recommendation.projected_overpayment_cents" meaning="projected" /></dd></div>
    </dl>
  </UCard>
</template>

<style scoped>
.recommendation { display: grid; gap: .75rem; }.recommendation[data-jurisdiction='federal'] { border-top: 3px solid var(--ui-primary); }.recommendation[data-jurisdiction='california'] { border-top: 3px solid var(--ui-secondary); }.eyebrow, .status { color: var(--ui-text-muted); font-size: .875rem; margin: 0; }h2 { margin: 0; }dl { display: grid; gap: .5rem; margin: 0; }dl div { display: flex; justify-content: space-between; gap: 1rem; }dt { color: var(--ui-text-muted); }dd { margin: 0; text-align: right; }
</style>
