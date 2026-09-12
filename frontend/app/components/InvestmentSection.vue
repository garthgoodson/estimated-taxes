<script setup lang="ts">
import { ref } from 'vue'
import type { ApiValidationField, ApiWarning, Cents, QuarterInput } from '~/types/api'

type Investments = NonNullable<QuarterInput['investments']>
type InvestmentField = Exclude<keyof Investments, 'notes'>

const props = defineProps<{ modelValue: Investments | null; warnings: ApiWarning[]; errors: ApiValidationField[] }>()
const emit = defineEmits<{ 'update:modelValue': [value: Investments | null] }>()
const entryModalOpen = ref(false)

function update<K extends keyof Investments>(field: K, value: Investments[K]) {
  if (props.modelValue) emit('update:modelValue', { ...props.modelValue, [field]: value })
}
function add({ field, amount_cents }: { field: InvestmentField; amount_cents: Cents }) {
  if (props.modelValue) emit('update:modelValue', { ...props.modelValue, [field]: props.modelValue[field] + amount_cents })
}
function openEntryModal() {
  if (!props.modelValue) emit('update:modelValue', createInvestments())
  entryModalOpen.value = true
}
function errorFor(field: string) { return props.errors.find(error => error.path === `investments.${field}`)?.message }
function createInvestments(): Investments {
  return { ordinary_dividends_cents: 0, qualified_dividends_cents: 0, short_term_gain_cents: 0, long_term_gain_cents: 0, federal_withholding_cents: 0, california_withholding_cents: 0, notes: null }
}
</script>

<template>
  <UCard>
    <template #header>
      <div class="header"><div><h2>Quarterly investments</h2><p>All values are for this selected quarter, not year to date.</p></div><UButton size="sm" @click="openEntryModal">Add</UButton></div>
    </template>
    <div v-if="modelValue" class="investment-form">
      <section class="investment-group" aria-labelledby="dividend-income-heading">
        <h3 id="dividend-income-heading">Dividend income</h3>
        <div class="investment-grid">
          <MoneyInput :model-value="modelValue.ordinary_dividends_cents" label="Ordinary dividends" :backend-error="errorFor('ordinary_dividends_cents')" @update:model-value="update('ordinary_dividends_cents', $event as never)" />
          <MoneyInput :model-value="modelValue.qualified_dividends_cents" label="Qualified dividends" :backend-error="errorFor('qualified_dividends_cents')" @update:model-value="update('qualified_dividends_cents', $event as never)" />
        </div>
      </section>
      <section class="investment-group" aria-labelledby="gains-losses-heading">
        <h3 id="gains-losses-heading">Gains and losses</h3>
        <div class="investment-grid">
          <MoneyInput :model-value="modelValue.short_term_gain_cents" allow-negative label="Net short-term gain/loss" @update:model-value="update('short_term_gain_cents', $event as never)" />
          <MoneyInput :model-value="modelValue.long_term_gain_cents" allow-negative label="Net long-term gain/loss" @update:model-value="update('long_term_gain_cents', $event as never)" />
        </div>
      </section>
      <section class="investment-group" aria-labelledby="investment-withholding-heading">
        <h3 id="investment-withholding-heading">Investment withholding</h3>
        <div class="investment-grid">
          <MoneyInput :model-value="modelValue.federal_withholding_cents" label="Federal" @update:model-value="update('federal_withholding_cents', $event as never)" />
          <MoneyInput :model-value="modelValue.california_withholding_cents" label="California" @update:model-value="update('california_withholding_cents', $event as never)" />
        </div>
      </section>
      <UFormField class="form-wide" label="Notes (optional)"><UTextarea :model-value="modelValue.notes ?? ''" @update:model-value="update('notes', $event ? String($event) : null)" /></UFormField>
      <WarningList :warnings="warnings" class="form-wide" />
      <UAlert v-for="error in errors" :key="error.path" class="form-wide" color="error" :description="error.message" />
      <div class="form-wide"><UButton color="error" variant="outline" @click="emit('update:modelValue', null)">Remove investments</UButton></div>
    </div>
    <UEmpty v-else :ui="{ root: 'ring-0' }" icon="i-lucide-chart-no-axes-combined" title="No investment activity entered" description="Add dividends, gains/losses, or investment withholding for this quarter.">
      <template #actions><UButton @click="emit('update:modelValue', createInvestments())">Add investments</UButton></template>
    </UEmpty>
  </UCard>
  <InvestmentEntryModal v-if="modelValue" v-model:open="entryModalOpen" :investments="modelValue" @add="add" />
</template>

<style scoped>
h2, h3, p { margin: 0; }p { color: var(--ui-text-muted); font-size: .875rem; margin-top: .25rem; }.header { align-items: start; display: flex; gap: 1rem; justify-content: space-between; }.investment-form { display: grid; gap: 1.5rem; }.investment-group { display: grid; gap: .75rem; }.investment-group + .investment-group { border-top: 1px solid var(--ui-border); padding-top: 1.5rem; }h3 { color: var(--ui-text-muted); font-size: .875rem; font-weight: 650; }.investment-grid { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }.form-wide { grid-column: 1 / -1; }@media (max-width: 640px) { .header, .investment-grid { grid-template-columns: 1fr; }.header { align-items: start; flex-direction: column; } }
</style>
