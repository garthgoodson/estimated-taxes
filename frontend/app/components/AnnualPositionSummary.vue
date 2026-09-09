<script setup lang="ts">
import type { Jurisdiction } from '~/types/api'
import type { QuarterResource } from '~/types/quarter'

defineProps<{ resource: QuarterResource }>()
</script>

<template>
  <UCard>
    <template #header><h2>Updated annual position</h2><p>Projection as of {{ resource.result.as_of_date }}</p></template>
    <div class="annual-sections">
      <section class="annual-section" aria-labelledby="income-investments-heading">
        <h3 id="income-investments-heading">Income &amp; investments</h3>
        <div class="annual-groups">
          <div class="metric-group">
            <h4>Projected wages</h4>
            <div class="metric-grid">
              <MetricSummary label="Federal wages" :amount="resource.result.annual_summary.federal_wages.projected_annual_cents" />
              <MetricSummary label="California wages" :amount="resource.result.annual_summary.california_wages.projected_annual_cents" />
            </div>
          </div>
          <div class="metric-group">
            <h4>Recorded investment income</h4>
            <div class="metric-grid">
              <MetricSummary label="Ordinary dividends" :amount="resource.result.annual_summary.investments.ordinary_dividends_cents" />
              <MetricSummary label="Qualified dividends" :amount="resource.result.annual_summary.investments.qualified_dividends_cents" />
            </div>
          </div>
        </div>
      </section>
      <section class="annual-section" aria-labelledby="covered-taxes-heading">
        <h3 id="covered-taxes-heading">Covered taxes</h3>
        <div class="annual-groups">
          <div class="metric-group">
            <h4>Projected withholding</h4>
            <div class="metric-grid">
              <MetricSummary label="Federal withholding" :amount="resource.result.annual_summary.federal_withholding.projected_annual_cents" />
              <MetricSummary label="California withholding" :amount="resource.result.annual_summary.california_withholding.projected_annual_cents" />
            </div>
          </div>
          <div class="metric-group">
            <h4>Estimated payments recorded</h4>
            <div class="metric-grid">
              <MetricSummary label="Federal estimated payments" :amount="resource.result.current_recommendations.federal.payments_credited_cents" />
              <MetricSummary label="California estimated payments" :amount="resource.result.current_recommendations.california.payments_credited_cents" />
            </div>
          </div>
        </div>
      </section>
      <section class="annual-section" aria-labelledby="tax-position-heading">
        <h3 id="tax-position-heading">Projected tax position</h3>
        <div class="annual-groups">
          <div v-for="jurisdiction in ['federal', 'california'] as Jurisdiction[]" :key="jurisdiction" class="metric-group">
            <h4>{{ jurisdiction === 'federal' ? 'Federal' : 'California' }}</h4>
            <div class="metric-grid">
              <MetricSummary label="Annual liability" :amount="resource.result[jurisdiction].details.annual_liability_cents" />
              <MetricSummary label="Remaining obligation" :amount="resource.result.current_recommendations[jurisdiction].remaining_before_recommendation_cents" />
            </div>
          </div>
        </div>
      </section>
    </div>
    <WarningList :warnings="resource.result.annual_summary.warnings" class="warnings" />
    <div class="tax-warnings"><WarningList :warnings="resource.result.federal.warnings" /><WarningList :warnings="resource.result.california.warnings" /></div>
  </UCard>
</template>

<style scoped>
h2, h3, h4, p { margin: 0; }
p { color: var(--ui-text-muted); font-size: .875rem; margin-top: .25rem; }
.annual-sections { display: grid; gap: 1.5rem; }
.annual-section + .annual-section { border-top: 1px solid var(--ui-border); padding-top: 1.5rem; }
h3 { font-size: 1rem; }
.annual-groups { display: grid; gap: 1.25rem; grid-template-columns: repeat(2, minmax(0, 1fr)); margin-top: 1rem; }
.metric-group { display: grid; gap: .75rem; }
h4 { color: var(--ui-text-muted); font-size: .875rem; font-weight: 650; }
.metric-grid { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }
.warnings, .tax-warnings { margin-top: 1rem; }
.tax-warnings { display: grid; gap: .75rem; }
@media (max-width: 640px) { .annual-groups, .metric-grid { grid-template-columns: 1fr; } }
</style>
