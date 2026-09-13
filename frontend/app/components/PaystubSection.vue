<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import type { ApiValidationField, ApiWarning, PaystubInput } from '~/types/api'

type GrossPayApply =
  | { mode: 'current_period'; regular_wages_cents: number }
  | { mode: 'year_to_date'; federal_taxable_wages_cents: number; california_taxable_wages_cents: number }

const props = defineProps<{ modelValue: PaystubInput | null; spouse: 'spouse_1' | 'spouse_2'; label: string; warnings: ApiWarning[]; errors: ApiValidationField[] }>()
const emit = defineEmits<{ 'update:modelValue': [value: PaystubInput | null] }>()
const frequencies = [{ label: 'Weekly', value: 'weekly' }, { label: 'Biweekly', value: 'biweekly' }, { label: 'Semimonthly', value: 'semimonthly' }, { label: 'Monthly', value: 'monthly' }]
const projectionModes = [{ label: 'Continue through year end', value: 'year_end' }, { label: 'End after this paystub', value: 'after_paystub' }, { label: 'End on date', value: 'date' }]
const selectedProjectionMode = ref<'year_end' | 'after_paystub' | 'date'>('year_end')
const projectionMode = computed(() => selectedProjectionMode.value)
watch(() => props.modelValue?.projection_end_date, () => {
  selectedProjectionMode.value = !props.modelValue?.projection_end_date ? 'year_end'
    : props.modelValue.projection_end_date === props.modelValue.date ? 'after_paystub' : 'date'
}, { immediate: true })
const currentPeriodCalculatorOpen = ref(false)
const yearToDateCalculatorOpen = ref(false)

function update<K extends keyof PaystubInput>(field: K, value: PaystubInput[K]) {
  if (props.modelValue) emit('update:modelValue', { ...props.modelValue, [field]: value })
}
function updatePaystubDate(date: string) {
  if (!props.modelValue) return
  emit('update:modelValue', { ...props.modelValue, date, projection_end_date: props.modelValue.projection_end_date === props.modelValue.date ? date : props.modelValue.projection_end_date })
}
function updateProjectionMode(mode: 'year_end' | 'after_paystub' | 'date') {
  if (!props.modelValue) return
  selectedProjectionMode.value = mode
  const projection_end_date = mode === 'year_end' ? null
    : mode === 'after_paystub' ? props.modelValue.date
      : props.modelValue.projection_end_date ?? '2026-12-31'
  emit('update:modelValue', { ...props.modelValue, projection_end_date })
}
function applyGrossPayCalculation(value: GrossPayApply) {
  if (!props.modelValue) return
  if (value.mode === 'current_period') {
    emit('update:modelValue', { ...props.modelValue, current_period_regular_wages_cents: value.regular_wages_cents })
    return
  }
  emit('update:modelValue', {
    ...props.modelValue,
    federal_taxable_wages_ytd_cents: value.federal_taxable_wages_cents,
    california_taxable_wages_ytd_cents: value.california_taxable_wages_cents
  })
}
function errorFor(field: string) { return props.errors.find(error => error.path === `paystubs.${props.spouse}.${field}`)?.message }
function fieldLabel(field: string, prefix = '', suffix = ''): string {
  return field.replace(prefix, '').replace(suffix, '').replace('_cents', '').split('_').map(word => word === 'sdi' ? 'SDI' : `${word[0]?.toUpperCase()}${word.slice(1)}`).join(' ')
}
function createPaystub(): PaystubInput {
  return { date: '', pay_frequency: 'biweekly', projection_end_date: null, current_period_regular_wages_cents: 0, current_period_bonus_wages_cents: 0, current_period_federal_withholding_cents: 0, current_period_california_withholding_cents: 0, federal_taxable_wages_ytd_cents: 0, california_taxable_wages_ytd_cents: 0, federal_withholding_ytd_cents: 0, california_withholding_ytd_cents: 0, social_security_withholding_ytd_cents: 0, medicare_withholding_ytd_cents: 0, california_sdi_withholding_ytd_cents: 0 }
}
</script>

<template>
  <UCard>
    <template #header><h2>{{ label }} paystub</h2></template>
    <div v-if="modelValue" class="form-grid">
      <UFormField label="Paystub date"><UInput :model-value="modelValue.date" type="date" @update:model-value="updatePaystubDate(String($event))" /></UFormField>
      <UFormField label="Pay frequency"><USelect :model-value="modelValue.pay_frequency" :items="frequencies" @update:model-value="update('pay_frequency', $event as PaystubInput['pay_frequency'])" /></UFormField>
      <UFormField class="form-wide" label="Project this pay pattern through" :error="errorFor('projection_end_date')">
        <template #label="{ label: projectionLabel }"><span class="projection-label">{{ projectionLabel }}<HelpTooltip :label="projectionLabel ?? 'pay-pattern projection'" text="Limits how long this paystub’s regular wages and withholding are repeated. It does not change wages or withholding already included in YTD totals. This estimator models one consolidated pay pattern per spouse and assumes no additional wages after the selected date." /></span></template>
        <div class="projection-controls">
          <USelect :model-value="projectionMode" :items="projectionModes" @update:model-value="updateProjectionMode($event as 'year_end' | 'after_paystub' | 'date')" />
          <UInput v-if="projectionMode === 'date'" :model-value="modelValue.projection_end_date ?? undefined" type="date" @update:model-value="update('projection_end_date', String($event))" />
        </div>
      </UFormField>

      <p class="form-heading">Current pay period</p>
      <MoneyInput :model-value="modelValue.current_period_regular_wages_cents" label="Regular Wages" :backend-error="errorFor('current_period_regular_wages_cents')" @update:model-value="update('current_period_regular_wages_cents', $event as number)">
        <template #label-action><UButton size="xs" color="neutral" variant="outline" @click="currentPeriodCalculatorOpen = true">Calculate</UButton></template>
      </MoneyInput>
      <MoneyInput v-for="field in ['current_period_bonus_wages_cents', 'current_period_federal_withholding_cents', 'current_period_california_withholding_cents']" :key="field" :model-value="modelValue[field as keyof PaystubInput] as number" :label="fieldLabel(field, 'current_period_')" :backend-error="errorFor(field)" @update:model-value="update(field as keyof PaystubInput, $event as never)" />

      <p class="form-heading">Year to date</p>
      <MoneyInput :model-value="modelValue.federal_taxable_wages_ytd_cents" label="Federal Taxable Wages" :backend-error="errorFor('federal_taxable_wages_ytd_cents')" @update:model-value="update('federal_taxable_wages_ytd_cents', $event as number)">
        <template #label-action><UButton size="xs" color="neutral" variant="outline" @click="yearToDateCalculatorOpen = true">Calculate</UButton></template>
      </MoneyInput>
      <MoneyInput v-for="field in ['california_taxable_wages_ytd_cents', 'federal_withholding_ytd_cents', 'california_withholding_ytd_cents', 'social_security_withholding_ytd_cents', 'medicare_withholding_ytd_cents', 'california_sdi_withholding_ytd_cents']" :key="field" :model-value="modelValue[field as keyof PaystubInput] as number" :label="fieldLabel(field, '', '_ytd')" :backend-error="errorFor(field)" @update:model-value="update(field as keyof PaystubInput, $event as never)" />

      <WarningList :warnings="warnings" class="form-wide" />
      <UAlert v-for="error in errors" :key="error.path" class="form-wide" color="error" :description="error.message" />
      <div class="form-wide"><UButton color="error" variant="outline" @click="emit('update:modelValue', null)">Remove paystub</UButton></div>
    </div>
    <UEmpty v-else :ui="{ root: 'ring-0' }" icon="i-lucide-receipt" title="No paystub entered" description="Add the latest paystub snapshot for this spouse.">
      <template #actions><UButton @click="emit('update:modelValue', createPaystub())">Add paystub</UButton></template>
    </UEmpty>
  </UCard>
  <GrossPayCalculatorModal v-model:open="currentPeriodCalculatorOpen" mode="current_period" @apply="applyGrossPayCalculation" />
  <GrossPayCalculatorModal v-model:open="yearToDateCalculatorOpen" mode="year_to_date" @apply="applyGrossPayCalculation" />
</template>

<style scoped>
h2, .form-heading { margin: 0; }
.projection-label { align-items: center; display: inline-flex; gap: .25rem; }
.projection-controls { align-items: center; display: flex; gap: .5rem; }
.form-grid { display: grid; gap: 1rem; grid-template-columns: repeat(2, minmax(0, 1fr)); }
.form-heading, .form-wide { grid-column: 1 / -1; }
.form-heading { border-top: 1px solid var(--ui-border); color: var(--ui-text-muted); font-size: .875rem; font-weight: 700; padding-top: 1rem; }
@media (max-width: 640px) { .form-grid { grid-template-columns: 1fr; } }
</style>
