<script setup lang="ts">
import type { ApiValidationField, ApiWarning } from '~/types/api'
import { validQuarter } from '~/utils/routes'
import { quarterDateRange } from '~/utils/quarterDates'
import { confirmDiscard, protectBeforeUnload } from '~/utils/unsavedChanges'

const route = useRoute()
const quarter = validQuarter(route.params.quarter)
if (quarter == null) await navigateTo('/')

const editor = useQuarterEditor(quarter ?? 1)
const bootstrap = useBootstrap()
const quarterSummary = computed(() => bootstrap.state.value?.quarters.find(item => item.quarter === quarter))
const currentQuarter = computed(() => editor.resource.value?.result.current_recommendations.federal.current_quarter)
const periodLabel = computed(() => quarter === currentQuarter.value ? 'Current quarter' : (quarter ?? 1) < (currentQuarter.value ?? 1) ? 'Historic quarter' : 'Upcoming quarter')

function relatedWarnings(prefix: string): ApiWarning[] {
  return (editor.resource.value?.warnings ?? []).filter(warning => warning.path?.startsWith(prefix))
}
function relatedErrors(prefix: string): ApiValidationField[] {
  return editor.validationErrors.value.filter(error => error.path.startsWith(prefix))
}

const beforeUnload = (event: BeforeUnloadEvent) => protectBeforeUnload(editor.dirty.value, event)

onBeforeRouteLeave(() => confirmDiscard(editor.dirty.value, () => window.confirm('Discard unsaved quarter changes?')))
onMounted(() => {
  void editor.load()
  window.addEventListener('beforeunload', beforeUnload)
})
onBeforeUnmount(() => window.removeEventListener('beforeunload', beforeUnload))
</script>

<template>
  <ApplicationState :pending="editor.pending.value" :error="!editor.resource.value && editor.error.value && editor.error.value.kind !== 'validation' ? editor.error.value : null" :retry="editor.load">
    <div v-if="editor.resource.value" class="quarter-page">
      <header class="quarter-header">
        <div><p class="eyebrow">{{ periodLabel }}</p><h1>Quarter {{ quarter }}</h1><p>{{ quarterDateRange(quarter ?? 1) }} · As of {{ editor.resource.value.result.as_of_date }} · {{ quarterSummary?.status.replace('_', ' ') ?? 'Loading status' }}</p></div>
        <div class="save-state"><span v-if="editor.saved.value">Saved</span><span v-else-if="editor.dirty.value">Unsaved changes</span><span v-else>Saved version</span><UButton :loading="editor.saving.value" :disabled="editor.saving.value || !editor.dirty.value" @click="editor.save">Save quarter</UButton></div>
      </header>

      <section class="recommendations" aria-label="Estimated taxes owed"><QuarterRecommendationSummary jurisdiction="federal" :recommendation="editor.resource.value.result.current_recommendations.federal" /><QuarterRecommendationSummary jurisdiction="california" :recommendation="editor.resource.value.result.current_recommendations.california" /></section>
      <UAlert v-for="outcome in editor.resource.value.result.current_recommendations.validation" :key="outcome.code" :color="outcome.severity === 'blocking' ? 'error' : outcome.severity === 'caution' ? 'warning' : 'info'" :title="outcome.jurisdiction ? `${outcome.jurisdiction === 'federal' ? 'Federal' : 'California'} recommendation` : 'Recommendation notice'" :description="outcome.message" />
      <UAlert v-if="editor.validationErrors.value.length" color="error" title="Review the highlighted quarter entries"><template #description><ul><li v-for="error in editor.validationErrors.value" :key="error.path">{{ error.message }}</li></ul></template></UAlert>
      <UAlert v-else-if="editor.error.value" color="error" title="Quarter was not saved" :description="editor.error.value.message"><template #actions><UButton color="neutral" variant="outline" @click="editor.save">Retry save</UButton></template></UAlert>

      <PaystubSection v-model="editor.form.value.paystubs.spouse_1" spouse="spouse_1" label="Spouse 1" :warnings="relatedWarnings('paystubs.spouse_1')" :errors="relatedErrors('paystubs.spouse_1')" />
      <PaystubSection v-model="editor.form.value.paystubs.spouse_2" spouse="spouse_2" label="Spouse 2" :warnings="relatedWarnings('paystubs.spouse_2')" :errors="relatedErrors('paystubs.spouse_2')" />
      <InvestmentSection v-model="editor.form.value.investments" :warnings="relatedWarnings('investments')" :errors="relatedErrors('investments')" />
      <EstimatedPaymentsSection v-model="editor.form.value.payments" :recommendations="editor.resource.value.result.current_recommendations" :errors="relatedErrors('payments')" />
      <AnnualPositionSummary :resource="editor.resource.value" />

    </div>
  </ApplicationState>
</template>

<style scoped>
.quarter-page { display: grid; gap: 1.5rem; }.quarter-header { align-items: end; display: flex; gap: 1rem; justify-content: space-between; }.quarter-header h1, .quarter-header p { margin: 0; }.quarter-header > div > p:not(.eyebrow) { color: var(--ui-text-muted); margin-top: .5rem; }.eyebrow { color: var(--ui-primary); font-size: .75rem; font-weight: 700; letter-spacing: .08em; text-transform: uppercase; }.save-state { align-items: end; display: flex; flex-direction: column; gap: .5rem; }.save-state > span { color: var(--ui-text-muted); font-size: .875rem; }.recommendations { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }ul { margin: .5rem 0 0; padding-left: 1.25rem; }@media (max-width: 640px) { .quarter-header { align-items: start; flex-direction: column; }.save-state { align-items: start; }.recommendations { grid-template-columns: 1fr; } }
</style>
