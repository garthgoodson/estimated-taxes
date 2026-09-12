<script setup lang="ts">
import type { Jurisdiction } from '~/types/api'
import { annualTaxPosition } from '~/utils/annualTaxPosition'

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
      <div class="earnings-groups">
        <section class="metric-group" aria-labelledby="actual-wages-heading">
          <div class="help-heading"><h3 id="actual-wages-heading">Taxable wages</h3><HelpTooltip label="Taxable wages" text="Year-to-date taxable wages from the latest paystub entered for each spouse." /></div>
          <div class="metric-grid">
            <MetricSummary label="Federal" :amount="state.actuals.federal_wages_ytd_cents" />
            <MetricSummary label="California" :amount="state.actuals.california_wages_ytd_cents" />
          </div>
        </section>
        <section class="metric-group investment-group" aria-labelledby="investment-activity-heading">
          <div class="help-heading"><h3 id="investment-activity-heading">Investment activity</h3><HelpTooltip label="Investment activity" text="Quarterly investment amounts recorded so far, combined across completed quarters." /></div>
          <div class="investment-grid">
            <MetricSummary label="Ordinary dividends" :amount="state.actuals.ordinary_dividends_cents" />
            <MetricSummary label="Qualified dividends" :amount="state.actuals.qualified_dividends_cents" />
            <MetricSummary label="Net short-term gain/loss" :amount="state.actuals.short_term_gain_cents" />
            <MetricSummary label="Net long-term gain/loss" :amount="state.actuals.long_term_gain_cents" />
          </div>
        </section>
      </div>
    </UCard>

    <UCard>
      <template #header><h2>Taxes paid so far</h2></template>
      <div class="metric-groups">
        <section class="metric-group" aria-labelledby="withholding-heading">
          <div class="help-heading"><h3 id="withholding-heading">Tax withholding</h3><HelpTooltip label="Tax withholding" text="Year-to-date payroll and investment tax withholding recorded so far." /></div>
          <div class="metric-grid">
            <MetricSummary label="Federal" :amount="state.actuals.federal_withholding_ytd_cents" />
            <MetricSummary label="California" :amount="state.actuals.california_withholding_ytd_cents" />
          </div>
        </section>
        <section class="metric-group" aria-labelledby="estimated-payments-heading">
          <div class="help-heading"><h3 id="estimated-payments-heading">Estimated payments</h3><HelpTooltip label="Estimated payments" text="Estimated tax payments recorded as paid; these amounts are not projected." /></div>
          <div class="metric-grid">
            <MetricSummary label="Federal" :amount="state.actuals.federal_estimated_payments_cents" />
            <MetricSummary label="California" :amount="state.actuals.california_estimated_payments_cents" />
          </div>
        </section>
      </div>
    </UCard>

    <UCard>
      <template #header><h2>Annual projection</h2></template>
      <div class="projection-groups">
        <div class="metric-groups">
          <section class="metric-group" aria-labelledby="projected-wages-heading">
            <div class="help-heading"><h3 id="projected-wages-heading">Projected wages</h3><HelpTooltip label="Projected wages" text="Estimated full-year taxable wages based on recorded paystubs and remaining pay periods." /></div>
            <div class="metric-grid">
              <MetricSummary label="Federal" :amount="state.projection.federal_wages_cents" />
              <MetricSummary label="California" :amount="state.projection.california_wages_cents" />
            </div>
          </section>
          <section class="metric-group" aria-labelledby="projected-withholding-heading">
            <div class="help-heading"><h3 id="projected-withholding-heading">Projected withholding</h3><HelpTooltip label="Projected withholding" text="Estimated full-year withholding based on recorded withholding and remaining pay periods." /></div>
            <div class="metric-grid">
              <MetricSummary label="Federal" :amount="state.projection.federal_withholding_cents" />
              <MetricSummary label="California" :amount="state.projection.california_withholding_cents" />
            </div>
          </section>
        </div>
        <section class="tax-position" aria-labelledby="projected-tax-position-heading">
          <div class="help-heading"><h3 id="projected-tax-position-heading">Projected tax position</h3><HelpTooltip label="Projected tax position" text="Estimated full-year tax liability and the amount remaining after projected withholding and recorded estimated payments." /></div>
          <div class="metric-groups">
            <section class="metric-group" aria-labelledby="annual-liability-heading">
              <h4 id="annual-liability-heading">Annual liability</h4>
              <div class="metric-grid">
                <MetricSummary label="Federal" :amount="state.tax.federal.annual_liability_cents" />
                <MetricSummary label="California" :amount="state.tax.california.annual_liability_cents" />
              </div>
            </section>
            <section class="metric-group" aria-labelledby="annual-position-heading">
              <h4 id="annual-position-heading">Annual position</h4>
              <div class="metric-grid">
                <MetricSummary v-for="jurisdiction in jurisdictions" :key="jurisdiction" :label="jurisdiction === 'federal' ? 'Federal' : 'California'" :amount="annualTaxPosition(state.tax[jurisdiction]).amount" :supporting-text="annualTaxPosition(state.tax[jurisdiction]).label" />
              </div>
            </section>
          </div>
        </section>
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
.page-stack { display: grid; gap: 1.5rem; }.help-heading { align-items: center; display: flex; gap: .25rem; min-width: 0; }.eyebrow { color: var(--ui-primary); font-size: .75rem; font-weight: 700; letter-spacing: .08em; margin: 0; text-transform: uppercase; }h1, h2, h3, h4, .intro { margin: 0; }h1 { margin-top: .25rem; }.intro { color: var(--ui-text-muted); margin-top: .5rem; }.jurisdiction-grid, .metric-groups { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }.earnings-groups { display: grid; gap: 1.5rem; }.metric-group { display: grid; gap: .75rem; }h3, h4 { color: var(--ui-text-muted); font-size: .875rem; font-weight: 650; }.metric-grid { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }.investment-group { border-top: 1px solid var(--ui-border); padding-top: 1.5rem; }.investment-grid { display: grid; gap: 1rem; grid-template-columns: repeat(4, minmax(0, 1fr)); }.projection-groups { display: grid; gap: 1.5rem; }.tax-position { border-top: 1px solid var(--ui-border); display: grid; gap: 1rem; padding-top: 1.5rem; }.quarter-grid { display: grid; gap: .75rem; grid-template-columns: repeat(4, minmax(0, 1fr)); }@media (max-width: 800px) { .investment-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }@media (max-width: 640px) { .jurisdiction-grid, .metric-groups, .metric-grid, .investment-grid, .quarter-grid { grid-template-columns: 1fr; } }
</style>
