<script setup lang="ts">
import type { ApiError } from '~/types/api'

const snapshots = useSnapshots()
const label = ref('')
const actionPending = ref(false)

async function saveSnapshot() {
  if (!label.value.trim()) return
  actionPending.value = true
  try {
    await snapshots.create(label.value.trim())
    label.value = ''
  } catch (cause) {
    snapshots.error.value = cause as ApiError
  } finally {
    actionPending.value = false
  }
}

async function renameSnapshot(id: string, currentLabel: string) {
  const label = window.prompt('Snapshot label', currentLabel)?.trim()
  if (!label) return
  actionPending.value = true
  try {
    await snapshots.rename(id, label)
  } catch (cause) {
    snapshots.error.value = cause as ApiError
  } finally {
    actionPending.value = false
  }
}

async function deleteSnapshot(id: string) {
  if (!window.confirm('Delete this saved calculation? This cannot be undone.')) return
  actionPending.value = true
  try {
    await snapshots.remove(id)
  } catch (cause) {
    snapshots.error.value = cause as ApiError
  } finally {
    actionPending.value = false
  }
}

onMounted(() => { void snapshots.load() })
</script>

<template>
  <div class="page-stack">
    <header><p class="eyebrow">History</p><h1>Saved calculations</h1><p>Snapshots preserve the inputs, rules, and recommendations captured when they were saved.</p></header>

    <UCard>
      <template #header><h2>Save current calculation</h2></template>
      <form class="save-form" @submit.prevent="saveSnapshot"><UFormField label="Snapshot label"><UInput v-model="label" placeholder="After Q3 update" /></UFormField><UButton type="submit" :disabled="!label.trim() || actionPending" :loading="actionPending">Save snapshot</UButton></form>
    </UCard>

    <ApplicationState :pending="snapshots.pending.value" :error="snapshots.error.value" :retry="snapshots.load">
      <UCard>
        <template #header><h2>Snapshots</h2></template>
        <p v-if="!snapshots.summaries.value.length" class="muted">No saved calculations yet.</p>
        <div v-else class="snapshot-list">
          <article v-for="snapshot in snapshots.summaries.value" :key="snapshot.id" class="snapshot-row">
            <button type="button" class="snapshot-select" :aria-pressed="snapshots.selected.value?.id === snapshot.id" @click="snapshots.select(snapshot.id)"><strong>{{ snapshot.label }}</strong><span>Saved as of {{ snapshot.as_of_date }}</span></button>
            <div class="row-actions"><UButton color="neutral" variant="ghost" size="sm" :disabled="actionPending" @click="renameSnapshot(snapshot.id, snapshot.label)">Rename</UButton><UButton color="error" variant="ghost" size="sm" :disabled="actionPending" @click="deleteSnapshot(snapshot.id)">Delete</UButton></div>
          </article>
        </div>
      </UCard>

      <UCard v-if="snapshots.detailPending.value" aria-live="polite"><p>Loading saved calculation details…</p></UCard>
      <UCard v-else-if="snapshots.selected.value">
        <template #header><h2>{{ snapshots.selected.value.label }}</h2><p>Stored recommendations as of {{ snapshots.selected.value.as_of_date }}</p></template>
        <div class="recommendation-grid"><JurisdictionSummary jurisdiction="federal" label="Stored federal recommendation" :amount="snapshots.selected.value.current_result.recommendations.federal.recommended_payment_cents" /><JurisdictionSummary jurisdiction="california" label="Stored California recommendation" :amount="snapshots.selected.value.current_result.recommendations.california.recommended_payment_cents" /></div>
      </UCard>
    </ApplicationState>
  </div>
</template>

<style scoped>
.page-stack { display:grid; gap:1.5rem; }.eyebrow { color:var(--ui-primary); font-size:.75rem; font-weight:700; letter-spacing:.08em; margin:0; text-transform:uppercase; }h1,h2,p { margin:0; }header > p:not(.eyebrow), .muted, .snapshot-select span, .snapshot-row + .snapshot-row { color:var(--ui-text-muted); }header > p:not(.eyebrow) { margin-top:.5rem; }.save-form { align-items:end; display:flex; gap:1rem; }.save-form :deep(.u-form-field) { flex:1; }.snapshot-list { display:grid; }.snapshot-row { align-items:center; display:flex; gap:1rem; justify-content:space-between; padding:.75rem 0; }.snapshot-row + .snapshot-row { border-top:1px solid var(--ui-border); }.snapshot-select { background:none; border:0; color:inherit; cursor:pointer; display:grid; gap:.25rem; padding:0; text-align:left; }.snapshot-select:focus-visible { outline:2px solid var(--ui-primary); outline-offset:3px; }.row-actions,.recommendation-grid { display:flex; gap:.5rem; }.recommendation-grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); }@media(max-width:640px) { .save-form,.snapshot-row { align-items:stretch; flex-direction:column; }.recommendation-grid { grid-template-columns:1fr; } }
</style>
