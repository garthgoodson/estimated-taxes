<script setup lang="ts">
import type { Jurisdiction } from '~/types/api'

const bootstrap = useBootstrap()
const state = computed(() => bootstrap.state.value)
const jurisdictions: Jurisdiction[] = ['federal', 'california']
</script>

<template>
  <div v-if="state" class="page-stack">
    <header>
      <p class="eyebrow">2026 annual estimate</p>
      <h1>Annual tax position</h1>
      <p class="intro">Review current projections and move to the active quarter to update your information.</p>
    </header>

    <section class="jurisdiction-grid" aria-label="Current payment recommendations">
      <JurisdictionSummary v-for="jurisdiction in jurisdictions" :key="jurisdiction" :jurisdiction="jurisdiction" label="Amount currently owed" :amount="state.tax[jurisdiction].current_recommendation_cents" />
    </section>

    <UCard>
      <template #header><h2>Actual earnings and investments so far</h2></template>
      <div class="metric-grid">
        <MetricSummary label="Federal taxable wages" :amount="state.actuals.federal_wages_ytd_cents" meaning="actual" />
        <MetricSummary label="California taxable wages" :amount="state.actuals.california_wages_ytd_cents" meaning="actual" />
        <MetricSummary label="Ordinary dividends" :amount="state.actuals.ordinary_dividends_cents" meaning="actual" />
        <MetricSummary label="Qualified dividends" :amount="state.actuals.qualified_dividends_cents" meaning="actual" />
        <MetricSummary label="Net short-term gain/loss" :amount="state.actuals.short_term_gain_cents" meaning="actual" />
        <MetricSummary label="Net long-term gain/loss" :amount="state.actuals.long_term_gain_cents" meaning="actual" />
      </div>
    </UCard>

    <UCard>
      <template #header><h2>Taxes paid so far</h2></template>
      <div class="metric-grid">
        <MetricSummary label="Federal income-tax withholding" :amount="state.actuals.federal_withholding_ytd_cents" meaning="paid" />
        <MetricSummary label="California income-tax withholding" :amount="state.actuals.california_withholding_ytd_cents" meaning="paid" />
        <MetricSummary label="Federal estimated payments" :amount="state.actuals.federal_estimated_payments_cents" meaning="paid" />
        <MetricSummary label="California estimated payments" :amount="state.actuals.california_estimated_payments_cents" meaning="paid" />
      </div>
    </UCard>

    <UCard>
      <template #header><h2>Annual projection</h2></template>
      <div class="metric-grid">
        <MetricSummary label="Projected annual federal wages" :amount="state.projection.federal_wages_cents" meaning="projected" />
        <MetricSummary label="Projected annual California wages" :amount="state.projection.california_wages_cents" meaning="projected" />
        <MetricSummary label="Projected full-year federal withholding" :amount="state.projection.federal_withholding_cents" meaning="projected" />
        <MetricSummary label="Projected full-year California withholding" :amount="state.projection.california_withholding_cents" meaning="projected" />
        <template v-for="jurisdiction in jurisdictions" :key="jurisdiction">
          <MetricSummary :label="`Projected ${jurisdiction === 'federal' ? 'Federal' : 'California'} liability`" :amount="state.tax[jurisdiction].annual_liability_cents" meaning="projected" />
          <MetricSummary :label="`Remaining projected ${jurisdiction === 'federal' ? 'Federal' : 'California'} obligation`" :amount="state.tax[jurisdiction].remaining_obligation_cents" meaning="projected" />
        </template>
      </div>
    </UCard>

    <UCard>
      <template #header><h2>Quarter status</h2></template>
      <div class="quarter-grid"><QuarterStatus v-for="quarter in state.quarters" :key="quarter.quarter" :quarter="quarter" /></div>
    </UCard>
    <WarningList :warnings="state.warnings" />
  </div>
</template>

<style scoped>
.page-stack { display: grid; gap: 1.5rem; }.eyebrow { color: var(--ui-primary); font-size: .75rem; font-weight: 700; letter-spacing: .08em; margin: 0; text-transform: uppercase; }h1, h2, .intro { margin: 0; }h1 { margin-top: .25rem; }.intro { color: var(--ui-text-muted); margin-top: .5rem; }.jurisdiction-grid { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }.metric-grid { display: grid; gap: 1rem; grid-template-columns: repeat(3, minmax(0, 1fr)); }.quarter-grid { display: grid; gap: .75rem; grid-template-columns: repeat(4, minmax(0, 1fr)); }@media (max-width: 800px) { .metric-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }@media (max-width: 640px) { .jurisdiction-grid, .metric-grid, .quarter-grid { grid-template-columns: 1fr; } }
</style>
