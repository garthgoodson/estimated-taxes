<script setup lang="ts">
import type { ApiError, HouseholdResource, TaxRulesResource } from '~/types/api'
import { sameValue } from '~/utils/quarter'
import { confirmDiscard, protectBeforeUnload } from '~/utils/unsavedChanges'

const api = useApi()
const bootstrap = useBootstrap()
const config = useRuntimeConfig()
const household = ref<HouseholdResource | null>(null)
const rules = ref<TaxRulesResource | null>(null)
const rulesText = ref('')
const pending = ref(true)
const saving = ref(false)
const error = ref<ApiError | null>(null)
const restoreFile = ref<File | null>(null)
const householdBaseline = ref<HouseholdResource | null>(null)
const rulesBaseline = ref<TaxRulesResource | null>(null)
const dirty = computed(() => {
  if (household.value !== null && !sameValue(household.value, householdBaseline.value)) return true
  try {
    return rulesBaseline.value !== null && !sameValue(JSON.parse(rulesText.value), editableRules(rulesBaseline.value))
  } catch {
    return rulesText.value.trim().length > 0
  }
})
const beforeUnload = (event: BeforeUnloadEvent) => protectBeforeUnload(dirty.value, event)

function editableRules(value: TaxRulesResource) {
  const withoutMetadata = (rule: Record<string, unknown>) => {
    const { revision_id: _revisionId, official: _official, sources: _sources, archived_revisions: _archived, ...editable } = rule
    return editable
  }
  return { federal: withoutMetadata(value.federal), california: withoutMetadata(value.california) }
}

function refreshRulesText() {
  if (rules.value) rulesText.value = JSON.stringify(editableRules(rules.value), null, 2)
}

function sourcesFor(jurisdiction: 'federal' | 'california'): Array<Record<string, unknown>> {
  const sources = rules.value?.[jurisdiction].sources
  return Array.isArray(sources) ? sources as Array<Record<string, unknown>> : []
}

async function load() {
  pending.value = true
  error.value = null
  try {
    const [loadedHousehold, loadedRules] = await Promise.all([api.getHousehold(), api.getTaxRules()])
    household.value = loadedHousehold
    rules.value = loadedRules
    refreshRulesText()
    householdBaseline.value = structuredClone(loadedHousehold)
    rulesBaseline.value = structuredClone(loadedRules)
  } catch (cause) {
    error.value = cause as ApiError
  } finally {
    pending.value = false
  }
}

async function saveHousehold() {
  if (!household.value) return
  saving.value = true
  error.value = null
  try {
    const response = await api.saveHousehold(household.value)
    household.value = response.household
    householdBaseline.value = structuredClone(response.household)
    await bootstrap.load()
  } catch (cause) {
    error.value = cause as ApiError
  } finally {
    saving.value = false
  }
}

async function saveRules() {
  saving.value = true
  error.value = null
  try {
    const parsed = JSON.parse(rulesText.value) as TaxRulesResource
    const response = await api.saveTaxRules(parsed)
    rules.value = response.rules
    refreshRulesText()
    rulesBaseline.value = structuredClone(response.rules)
    await bootstrap.load()
  } catch (cause) {
    error.value = cause instanceof SyntaxError
      ? { kind: 'validation', code: 'invalid_json', message: 'Tax rules must be valid JSON.', fields: [] }
      : cause as ApiError
  } finally {
    saving.value = false
  }
}

async function restoreRules(jurisdiction: 'federal' | 'california', source: 'official' | 'revision', revisionId?: string) {
  const description = source === 'official' ? 'official defaults' : 'this archived revision'
  if (!window.confirm(`Restore ${description} for ${jurisdiction === 'federal' ? 'Federal' : 'California'} rules?`)) return
  saving.value = true
  error.value = null
  try {
    const response = await api.restoreTaxRules({ jurisdiction, source, ...(revisionId ? { revision_id: revisionId } : {}) })
    rules.value = response.rules
    refreshRulesText()
    rulesBaseline.value = structuredClone(response.rules)
    await bootstrap.load()
  } catch (cause) {
    error.value = cause as ApiError
  } finally {
    saving.value = false
  }
}

async function downloadBackup() {
  try {
    const backup = await $fetch<ArrayBuffer>(`${config.public.apiBase}/backup`, { responseType: 'arrayBuffer' })
    const url = URL.createObjectURL(new Blob([backup], { type: 'application/vnd.sqlite3' }))
    const link = document.createElement('a')
    link.href = url
    link.download = 'estimated-taxes-2026.sqlite'
    link.click()
    URL.revokeObjectURL(url)
  } catch (cause) {
    error.value = cause as ApiError
  }
}

async function restoreDatabase() {
  if (!restoreFile.value || !window.confirm('Restore this SQLite backup? It will replace all current data.')) return
  saving.value = true
  error.value = null
  try {
    bootstrap.state.value = await api.restoreDatabase(await restoreFile.value.arrayBuffer())
    await load()
    restoreFile.value = null
  } catch (cause) {
    error.value = cause as ApiError
  } finally {
    saving.value = false
  }
}

onBeforeRouteLeave(() => confirmDiscard(dirty.value, () => window.confirm('Discard unsaved settings changes?')))
onMounted(() => {
  void load()
  window.addEventListener('beforeunload', beforeUnload)
})
onBeforeUnmount(() => window.removeEventListener('beforeunload', beforeUnload))
</script>

<template>
  <div class="page-stack">
    <header><p class="eyebrow">Settings</p><h1>Household and tax rules</h1><p>Changes update the current 2026 estimate immediately.</p></header>
    <ApplicationState :pending="pending" :error="!household && !rules ? error : null" :retry="load">
      <UAlert v-if="error" color="error" title="Settings were not saved" :description="error.message" />
      <UCard v-if="household">
        <template #header><h2>Household</h2><p>2026 · Married filing jointly · Full-year California residents</p></template>
        <div class="spouse-grid"><section v-for="spouse in household.spouses" :key="spouse.key" class="spouse"><h3>{{ spouse.key === 'spouse_1' ? 'Spouse 1' : 'Spouse 2' }}</h3><UFormField label="Label"><UInput v-model="spouse.label" /></UFormField><UCheckbox v-model="spouse.age_65_or_older" label="Age 65 or older" /><UCheckbox v-model="spouse.blind" label="Blind" /></section></div>
        <UButton :loading="saving" :disabled="saving" class="save-button" @click="saveHousehold">Save household</UButton>
      </UCard>

      <UCard v-if="rules">
        <template #header><h2>Tax rules</h2><p>Edit both complete jurisdiction rule documents. Values use integer cents and parts per million.</p></template>
        <div class="sources"><p v-for="jurisdiction in ['federal', 'california'] as const" :key="jurisdiction"><strong>{{ jurisdiction === 'federal' ? 'Federal' : 'California' }}:</strong> {{ rules[jurisdiction].official ? 'Official active values.' : 'Customized active values.' }} <template v-for="source in sourcesFor(jurisdiction)" :key="String(source.url)"><a v-if="typeof source.url === 'string'" :href="source.url" target="_blank" rel="noopener">{{ typeof source.title === 'string' ? source.title : 'Source' }}</a> </template></p></div>
        <UFormField label="Federal and California tax rules"><UTextarea v-model="rulesText" :rows="24" class="rules-editor" /></UFormField>
        <UButton :loading="saving" :disabled="saving" class="save-button" @click="saveRules">Save tax rules</UButton>
        <div class="restore-grid"><section v-for="jurisdiction in ['federal', 'california'] as const" :key="jurisdiction"><h3>{{ jurisdiction === 'federal' ? 'Federal' : 'California' }} revisions</h3><UButton color="warning" variant="outline" :disabled="saving" @click="restoreRules(jurisdiction, 'official')">Restore official defaults</UButton><ul v-if="Array.isArray(rules[jurisdiction].archived_revisions) && rules[jurisdiction].archived_revisions.length"><li v-for="revision in rules[jurisdiction].archived_revisions as Array<Record<string, unknown>>" :key="String(revision.revision_id)"><span>Revision {{ revision.revision_id }}</span><UButton size="xs" color="neutral" variant="ghost" :disabled="saving" @click="restoreRules(jurisdiction, 'revision', String(revision.revision_id))">Restore</UButton></li></ul><p v-else class="muted">No archived revisions.</p></section></div>
      </UCard>

      <UCard>
        <template #header><h2>Database</h2></template>
        <div class="data-actions"><p>Backups and restores include the complete local SQLite database.</p><UButton color="neutral" variant="outline" @click="downloadBackup">Download SQLite backup</UButton><UFormField label="SQLite backup to restore"><UInput type="file" accept=".sqlite,application/vnd.sqlite3,application/octet-stream" @change="restoreFile = ($event.target as HTMLInputElement).files?.[0] ?? null" /></UFormField><UButton color="error" :loading="saving" :disabled="saving || !restoreFile" @click="restoreDatabase">Restore backup</UButton></div>
      </UCard>
    </ApplicationState>
  </div>
</template>

<style scoped>
.page-stack { display:grid; gap:1.5rem; }.eyebrow { color:var(--ui-primary); font-size:.75rem; font-weight:700; letter-spacing:.08em; margin:0; text-transform:uppercase; }h1,h2,h3,p { margin:0; }header > p:not(.eyebrow), :deep(.u-card-header) p, .muted { color:var(--ui-text-muted); }header > p:not(.eyebrow) { margin-top:.5rem; }.spouse-grid,.restore-grid { display:grid; gap:1rem; grid-template-columns:repeat(2,minmax(0,1fr)); }.spouse,.restore-grid section,.data-actions { display:grid; gap:1rem; }.spouse { border:1px solid var(--ui-border); border-radius:var(--ui-radius); padding:1rem; }.save-button { margin-top:1rem; }.sources { display:grid; gap:.5rem; margin-bottom:1rem; }.sources a { color:var(--ui-primary); }.rules-editor { font-family:ui-monospace,SFMono-Regular,Menlo,monospace; width:100%; }.restore-grid { border-top:1px solid var(--ui-border); margin-top:1rem; padding-top:1rem; }.restore-grid ul { display:grid; gap:.5rem; list-style:none; margin:0; padding:0; }.restore-grid li { align-items:center; display:flex; gap:.5rem; justify-content:space-between; }.data-actions { max-width:32rem; }@media(max-width:640px) { .spouse-grid,.restore-grid { grid-template-columns:1fr; } }
</style>
