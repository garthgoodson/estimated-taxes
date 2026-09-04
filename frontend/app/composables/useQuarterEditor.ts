import { computed, ref } from 'vue'
import type { ApiError } from '~/types/api'
import type { QuarterForm, QuarterNumber, QuarterResource } from '~/types/quarter'
import { cloneQuarterForm, emptyQuarterForm, sameQuarterForm } from '~/utils/quarter'

type QuarterClient = { getQuarter: (quarter: QuarterNumber) => Promise<QuarterResource>; saveQuarter: (quarter: QuarterNumber, input: QuarterForm) => Promise<QuarterResource> }

export function createQuarterEditor(client: QuarterClient, quarter: QuarterNumber) {
  const resource = ref<QuarterResource | null>(null)
  const form = ref<QuarterForm>(emptyQuarterForm())
  const baseline = ref<QuarterForm>(emptyQuarterForm())
  const pending = ref(false); const saving = ref(false); const error = ref<ApiError | null>(null); const validationErrors = ref<ApiError['fields']>([]); const saved = ref(false)
  const dirty = computed(() => !sameQuarterForm(form.value, baseline.value))
  function replace(value: QuarterResource) { resource.value = value; form.value = cloneQuarterForm(value.input); baseline.value = cloneQuarterForm(value.input); validationErrors.value = []; saved.value = true }
  async function load() { pending.value = true; error.value = null; saved.value = false; try { replace(await client.getQuarter(quarter)); saved.value = false } catch (cause) { error.value = cause as ApiError } finally { pending.value = false } }
  async function save() { if (saving.value) return; saving.value = true; error.value = null; validationErrors.value = []; saved.value = false; try { replace(await client.saveQuarter(quarter, form.value)) } catch (cause) { error.value = cause as ApiError; if (error.value.kind === 'validation') validationErrors.value = error.value.fields } finally { saving.value = false } }
  return { resource, form, pending, saving, error, validationErrors, saved, dirty, load, save }
}
export function useQuarterEditor(quarter: QuarterNumber) { return createQuarterEditor(useApi(), quarter) }
