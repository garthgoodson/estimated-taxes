<script setup lang="ts">
const bootstrap = useBootstrap()
</script>

<template>
  <div v-if="bootstrap.state.value" class="page-stack">
    <header>
      <p class="eyebrow">2026 annual estimate</p>
      <h1>Annual tax position</h1>
      <p class="intro">Review current projections and move to the active quarter to update your information.</p>
    </header>
    <section class="jurisdiction-grid" aria-label="Current payment recommendations">
      <JurisdictionSummary jurisdiction="federal" label="Amount currently owed" :amount="bootstrap.state.value.tax.federal.current_recommendation_cents" />
      <JurisdictionSummary jurisdiction="california" label="Amount currently owed" :amount="bootstrap.state.value.tax.california.current_recommendation_cents" />
    </section>
    <UCard>
      <template #header><h2>Quarter status</h2></template>
      <div class="quarter-grid"><QuarterStatus v-for="quarter in bootstrap.state.value.quarters" :key="quarter.quarter" :quarter="quarter" /></div>
    </UCard>
    <WarningList :warnings="bootstrap.state.value.warnings" />
  </div>
</template>

<style scoped>
.page-stack { display: grid; gap: 1.5rem; }
.eyebrow { color: var(--ui-primary); font-size: .75rem; font-weight: 700; letter-spacing: .08em; margin: 0; text-transform: uppercase; }
h1, h2, .intro { margin: 0; } h1 { margin-top: .25rem; } .intro { color: var(--ui-text-muted); margin-top: .5rem; }
.jurisdiction-grid { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }
.quarter-grid { display: grid; gap: .75rem; grid-template-columns: repeat(4, minmax(0, 1fr)); }
@media (max-width: 640px) { .jurisdiction-grid, .quarter-grid { grid-template-columns: 1fr; } }
</style>
