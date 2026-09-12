<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import type { Cents, QuarterInput } from '~/types/api'

type Investments = NonNullable<QuarterInput['investments']>
type InvestmentField = Exclude<keyof Investments, 'notes'>

const types: Array<{ label: string; value: InvestmentField }> = [
  { label: 'Ordinary dividends', value: 'ordinary_dividends_cents' },
  { label: 'Qualified dividends', value: 'qualified_dividends_cents' },
  { label: 'Net short-term gain/loss', value: 'short_term_gain_cents' },
  { label: 'Net long-term gain/loss', value: 'long_term_gain_cents' },
  { label: 'Federal withholding', value: 'federal_withholding_cents' },
  { label: 'California withholding', value: 'california_withholding_cents' }
]

const props = defineProps<{ open: boolean; investments: Investments }>()
const emit = defineEmits<{ 'update:open': [open: boolean]; add: [value: { field: InvestmentField; amount_cents: Cents }] }>()
const field = ref<InvestmentField>('ordinary_dividends_cents')
const amount = ref<Cents | null>(null)

const proposed = computed(() => amount.value == null ? null : { ...props.investments, [field.value]: props.investments[field.value] + amount.value })
const validationError = computed(() => {
  if (amount.value == null) return null
  if (amount.value === 0) return 'Enter a nonzero amount.'
  if (!proposed.value) return null
  if (proposed.value.ordinary_dividends_cents < 0 || proposed.value.qualified_dividends_cents < 0 ||
      proposed.value.federal_withholding_cents < 0 || proposed.value.california_withholding_cents < 0) {
    return 'Dividends and withholding cannot be reduced below zero.'
  }
  if (proposed.value.qualified_dividends_cents > proposed.value.ordinary_dividends_cents) {
    return 'Qualified dividends cannot exceed ordinary dividends.'
  }
  return null
})
const canAdd = computed(() => amount.value != null && amount.value !== 0 && validationError.value == null)

function reset() {
  field.value = 'ordinary_dividends_cents'
  amount.value = null
}
function close() {
  reset()
  emit('update:open', false)
}
function add() {
  if (!canAdd.value || amount.value == null) return
  emit('add', { field: field.value, amount_cents: amount.value })
  close()
}

watch(() => props.open, open => {
  if (open) reset()
})
</script>

<template>
  <UModal :open="open" title="Add investment activity" description="Add an amount to this quarter’s investment aggregate." @update:open="open => open ? emit('update:open', true) : close()">
    <template #body>
      <div class="entry-form">
        <UFormField label="Investment type"><USelect v-model="field" :items="types" /></UFormField>
        <MoneyInput v-model="amount" allow-negative label="Amount" help="Use a negative amount to reduce an existing aggregate." />
        <UAlert v-if="validationError" color="error" :description="validationError" />
      </div>
    </template>
    <template #footer><div class="actions"><UButton color="neutral" variant="outline" @click="close">Cancel</UButton><UButton :disabled="!canAdd" @click="add">Add</UButton></div></template>
  </UModal>
</template>

<style scoped>
.entry-form { display: grid; gap: 1rem; }.actions { display: flex; gap: .75rem; justify-content: space-between; }
</style>
