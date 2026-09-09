import { computed, ref, toRaw } from 'vue'
import type { ApiError } from '~/types/api'
import type { QuarterForm, QuarterNumber, QuarterResource } from '~/types/quarter'
import { cloneQuarterForm, emptyQuarterForm, sameQuarterForm } from '~/utils/quarter'

type QuarterClient = { getQuarter: (quarter: QuarterNumber) => Promise<QuarterResource>; saveQuarter: (quarter: QuarterNumber, input: QuarterForm) => Promise<QuarterResource> }

export function createQuarterEditor(client: QuarterClient, initialQuarter: QuarterNumber) {
  let quarter = initialQuarter
  let loadRequest = 0
  const resource = ref<QuarterResource | null>(null)
  const form = ref<QuarterForm>(emptyQuarterForm())
  const baseline = ref<QuarterForm>(emptyQuarterForm())
  const pending = ref(false); const saving = ref(false); const error = ref<ApiError | null>(null); const validationErrors = ref<ApiError['fields']>([]); const saved = ref(false)
  const dirty = computed(() => !sameQuarterForm(form.value, baseline.value))
  function replace(value: QuarterResource) { resource.value = value; form.value = cloneQuarterForm(value.input); baseline.value = cloneQuarterForm(value.input); validationErrors.value = []; saved.value = true }
  async function load(selectedQuarter = quarter) {
    quarter = selectedQuarter
    const request = ++loadRequest
    pending.value = true
    error.value = null
    saved.value = false
    resource.value = null
    try {
      const value = await client.getQuarter(selectedQuarter)
      if (request === loadRequest) replace(value)
    } catch (cause) {
      if (request === loadRequest) error.value = cause as ApiError
    } finally {
      if (request === loadRequest) pending.value = false
    }
  }
  async function save(): Promise<boolean> {
    if (saving.value) return false
    const savedQuarter = quarter
    const input = cloneQuarterForm(toRaw(form.value))
    saving.value = true
    error.value = null
    validationErrors.value = []
    saved.value = false
    try {
      const value = await client.saveQuarter(savedQuarter, input)
      if (quarter === savedQuarter) replace(value)
      return true
    } catch (cause) {
      if (quarter === savedQuarter) {
        error.value = cause as ApiError
        if (error.value.kind === 'validation') validationErrors.value = error.value.fields
      }
      return false
    } finally {
      saving.value = false
    }
  }
  return { resource, form, pending, saving, error, validationErrors, saved, dirty, load, save }
}
export function useQuarterEditor(quarter: QuarterNumber) { return createQuarterEditor(useApi(), quarter) }
