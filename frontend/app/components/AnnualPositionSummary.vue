<script setup lang="ts">
import type { Jurisdiction } from '~/types/api'
import type { QuarterResource } from '~/types/quarter'

defineProps<{ resource: QuarterResource }>()
</script>

<template>
  <UCard>
    <template #header><h2>Updated annual position</h2><p>Projection as of {{ resource.result.as_of_date }}</p></template>
    <div class="annual-grid">
      <MetricSummary label="Projected annual federal wages" :amount="resource.result.annual_summary.federal_wages.projected_annual_cents" meaning="projected" />
      <MetricSummary label="Projected annual California wages" :amount="resource.result.annual_summary.california_wages.projected_annual_cents" meaning="projected" />
      <MetricSummary label="Recorded ordinary dividends" :amount="resource.result.annual_summary.investments.ordinary_dividends_cents" meaning="actual" />
      <MetricSummary label="Recorded qualified dividends" :amount="resource.result.annual_summary.investments.qualified_dividends_cents" meaning="actual" />
      <MetricSummary label="Projected federal withholding" :amount="resource.result.annual_summary.federal_withholding.projected_annual_cents" meaning="projected" />
      <MetricSummary label="Projected California withholding" :amount="resource.result.annual_summary.california_withholding.projected_annual_cents" meaning="projected" />
      <template v-for="jurisdiction in ['federal', 'california'] as Jurisdiction[]" :key="jurisdiction">
        <MetricSummary :label="`Projected ${jurisdiction === 'federal' ? 'Federal' : 'California'} liability`" :amount="resource.result[jurisdiction].details.annual_liability_cents" meaning="projected" />
        <MetricSummary :label="`${jurisdiction === 'federal' ? 'Federal' : 'California'} estimated payments recorded`" :amount="resource.result.current_recommendations[jurisdiction].payments_credited_cents" meaning="paid" />
        <MetricSummary :label="`Remaining ${jurisdiction === 'federal' ? 'Federal' : 'California'} obligation`" :amount="resource.result.current_recommendations[jurisdiction].remaining_before_recommendation_cents" meaning="projected" />
      </template>
    </div>
    <WarningList :warnings="resource.result.annual_summary.warnings" class="warnings" />
    <div class="tax-warnings"><WarningList :warnings="resource.result.federal.warnings" /><WarningList :warnings="resource.result.california.warnings" /></div>
  </UCard>
</template>

<style scoped>
h2, p { margin: 0; }p { color: var(--ui-text-muted); font-size: .875rem; margin-top: .25rem; }.annual-grid { display: grid; gap: 1rem; grid-template-columns: repeat(3, minmax(0, 1fr)); }.warnings, .tax-warnings { margin-top: 1rem; }.tax-warnings { display: grid; gap: .75rem; }@media (max-width: 800px) { .annual-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }@media (max-width: 640px) { .annual-grid { grid-template-columns: 1fr; } }
</style>
