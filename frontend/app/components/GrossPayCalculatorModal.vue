<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import type { Cents } from '~/types/api'
import { calculateGrossPayTaxableWages } from '~/utils/grossPayCalculator'

type CalculatorMode = 'current_period' | 'year_to_date'
type ApplyPayload =
  | { mode: 'current_period'; regular_wages_cents: Cents }
  | { mode: 'year_to_date'; federal_taxable_wages_cents: Cents; california_taxable_wages_cents: Cents }

const props = defineProps<{ open: boolean; mode: CalculatorMode }>()
const emit = defineEmits<{ 'update:open': [open: boolean]; apply: [payload: ApplyPayload] }>()

const grossEarnings = ref<Cents | null>(null)
const traditionalRetirement = ref<Cents | null>(null)
const healthPremiums = ref<Cents | null>(null)
const fsa = ref<Cents | null>(null)
const employeeHsa = ref<Cents | null>(null)
const otherSharedDeductions = ref<Cents | null>(null)

const modeTitle = computed(() => props.mode === 'year_to_date' ? 'Calculate taxable wages from gross pay' : 'Calculate regular wages from gross pay')
const modeDescription = computed(() => props.mode === 'year_to_date'
  ? 'Derive Federal and California taxable wages from gross pay and payroll deductions.'
  : 'Derive recurring regular wages from gross pay and payroll deductions.')
const grossHelp = computed(() => props.mode === 'year_to_date'
  ? 'Include all year-to-date earnings, including bonuses and holiday pay.'
  : 'Include recurring regular gross wages only. Exclude bonuses and other nonrecurring pay.')
const calculation = computed(() => {
  if (grossEarnings.value == null) return null
  return calculateGrossPayTaxableWages({
    gross_earnings_cents: grossEarnings.value,
    traditional_retirement_cents: traditionalRetirement.value ?? 0,
    pre_tax_health_premiums_cents: healthPremiums.value ?? 0,
    pre_tax_fsa_cents: fsa.value ?? 0,
    employee_hsa_payroll_cents: employeeHsa.value ?? 0,
    other_shared_pre_tax_deductions_cents: otherSharedDeductions.value ?? 0
  })
})
const validationError = computed(() => calculation.value?.valid === false ? calculation.value.message : null)
const result = computed(() => calculation.value?.valid ? calculation.value.calculation : null)
const taxableWagesSubstantiallyBelowGross = computed(() => result.value !== null && grossEarnings.value !== null &&
  result.value.federal_taxable_wages_cents < grossEarnings.value / 2)

function reset() {
  grossEarnings.value = null
  traditionalRetirement.value = null
  healthPremiums.value = null
  fsa.value = null
  employeeHsa.value = null
  otherSharedDeductions.value = null
}
function close() {
  reset()
  emit('update:open', false)
}
function apply() {
  if (!result.value) return
  if (props.mode === 'year_to_date') {
    emit('apply', {
      mode: 'year_to_date',
      federal_taxable_wages_cents: result.value.federal_taxable_wages_cents,
      california_taxable_wages_cents: result.value.california_taxable_wages_cents
    })
  } else {
    emit('apply', { mode: 'current_period', regular_wages_cents: result.value.federal_taxable_wages_cents })
  }
  close()
}

watch(() => props.open, open => {
  if (open) reset()
})
</script>

<template>
  <UModal :open="open" :title="modeTitle" :description="modeDescription" scrollable @update:open="open => open ? emit('update:open', true) : close()">
    <template #body>
      <div class="calculator">
        <UAlert color="warning" title="Payroll treatment can vary" description="Use only employee payroll deductions included in the gross earnings entered here. Do not subtract employer HSA contributions, Roth 401(k)/403(b) contributions, or post-tax insurance deductions." />
        <p class="hsa-note">HSA payroll contributions reduce the Federal result but generally do not reduce the California result because California does not conform to the Federal HSA deduction.</p>
        <div class="input-grid">
          <div class="gross-earnings wide"><p>{{ grossHelp }}</p><MoneyInput v-model="grossEarnings" label="Gross earnings" /></div>
          <p class="deduction-heading wide">Pre-tax deductions</p>
          <MoneyInput v-model="traditionalRetirement" label="Traditional 401(k)/403(b) (excluding Roth)" />
          <MoneyInput v-model="healthPremiums" label="Medical, dental, and vision premiums" />
          <MoneyInput v-model="fsa" label="FSA contributions" />
          <MoneyInput v-model="employeeHsa" label="HSA payroll contributions" />
          <MoneyInput v-model="otherSharedDeductions" class="wide" label="Other shared deductions" />
        </div>
        <UAlert v-if="validationError" color="error" :description="validationError" />
        <UAlert v-if="taxableWagesSubstantiallyBelowGross" color="warning" title="Check this value" description="Taxable wages are substantially below gross earnings. Verify that Roth contributions, taxes, and post-tax deductions were not subtracted." />
        <section v-if="result" class="breakdown" aria-labelledby="arithmetic-heading">
          <h3 id="arithmetic-heading">Arithmetic</h3>
          <dl>
            <div><dt>Gross earnings</dt><dd><MoneyDisplay :value="grossEarnings" /></dd></div>
            <div><dt>Shared deductions</dt><dd>−<MoneyDisplay :value="result.shared_deductions_cents" /></dd></div>
            <div><dt>Federal-only HSA subtraction</dt><dd>−<MoneyDisplay :value="result.federal_only_hsa_deduction_cents" /></dd></div>
            <div><dt>Federal result</dt><dd><MoneyDisplay :value="result.federal_taxable_wages_cents" /></dd></div>
            <div><dt>California result</dt><dd><MoneyDisplay :value="result.california_taxable_wages_cents" /></dd></div>
          </dl>
        </section>
        <UAlert v-if="mode === 'current_period'" color="warning" title="Federal result will be applied" description="The paystub form has one regular-wage field for both jurisdictions. Applying uses the Federal result; an HSA deduction can make California current-period taxable wages higher." />
      </div>
    </template>
    <template #footer>
      <div class="actions"><UButton color="neutral" variant="outline" @click="close">Cancel</UButton><UButton :disabled="!result" @click="apply">Apply values</UButton></div>
    </template>
  </UModal>
</template>

<style scoped>
.calculator { display: grid; gap: 1rem; }.hsa-note, .gross-earnings p { color: var(--ui-text-muted); font-size: .875rem; margin: 0; }.input-grid { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }.wide { grid-column: 1 / -1; }.gross-earnings { display: grid; gap: .5rem; }.deduction-heading { border-top: 1px solid var(--ui-border); font-weight: 650; margin: 0; padding-top: 1rem; }.breakdown { border-top: 1px solid var(--ui-border); display: grid; gap: .75rem; padding-top: 1rem; }.breakdown h3 { margin: 0; }.breakdown dl { display: grid; gap: .5rem; margin: 0; }.breakdown dl div { display: flex; gap: 1rem; justify-content: space-between; }.breakdown dt { color: var(--ui-text-muted); }.breakdown dd { display: flex; margin: 0; }.actions { display: flex; gap: .75rem; justify-content: space-between; }@media (max-width: 640px) { .input-grid { grid-template-columns: 1fr; } }
</style>
