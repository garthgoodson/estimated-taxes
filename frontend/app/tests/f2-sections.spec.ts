import { mount } from '@vue/test-utils'
import { defineComponent, nextTick, ref } from 'vue'
import { describe, expect, it } from 'vitest'
import InvestmentSection from '~/components/InvestmentSection.vue'
import PaystubSection from '~/components/PaystubSection.vue'

const paystub = { date: '2026-06-01', pay_frequency: 'biweekly' as const, projection_end_date: null, current_period_regular_wages_cents: 12345, current_period_bonus_wages_cents: 0, current_period_federal_withholding_cents: 100, current_period_california_withholding_cents: 50, federal_taxable_wages_ytd_cents: 123456, california_taxable_wages_ytd_cents: 123456, federal_withholding_ytd_cents: 1000, california_withholding_ytd_cents: 500, social_security_withholding_ytd_cents: 1, medicare_withholding_ytd_cents: 2, california_sdi_withholding_ytd_cents: 3 }
const global = { stubs: { UCard: { template: '<section><slot name="header" /><slot /></section>' }, UFormField: { template: '<div><slot /></div>' }, UInput: true, USelect: { name: 'USelect', template: '<button />', props: ['modelValue', 'items'] }, UEmpty: { template: '<div><slot name="actions" /></div>' }, UButton: { template: '<button @click="$emit(\'click\')"><slot /></button>' }, UAlert: true, WarningList: true, MoneyInput: { template: '<button class="money" @click="$emit(\'update:modelValue\', 999)">{{ label }}</button>', props: ['label'] }, GrossPayCalculatorModal: { name: 'GrossPayCalculatorModal', template: '<div />', props: ['mode'] }, InvestmentEntryModal: { name: 'InvestmentEntryModal', template: '<div />', props: ['open', 'investments'] } } }

describe('F2 section state flow', () => {
  it('emits an immutable investment replacement and displays a precise backend error', async () => {
    const model = { ordinary_dividends_cents: 100, qualified_dividends_cents: 50, short_term_gain_cents: -1, long_term_gain_cents: 0, federal_withholding_cents: 0, california_withholding_cents: 0, notes: null }
    const wrapper = mount(InvestmentSection, { props: { modelValue: model, warnings: [], errors: [{ path: 'investments.qualified_dividends_cents', code: 'invalid', message: 'Too high.' }] }, global })
    await wrapper.findAll('.money')[1]!.trigger('click')
    const value = wrapper.emitted('update:modelValue')![0]![0] as typeof model
    expect(value).toEqual({ ...model, qualified_dividends_cents: 999 })
    expect(model.qualified_dividends_cents).toBe(50)
  })

  it('creates investment aggregates and opens the modal from the empty header action', async () => {
    const Harness = defineComponent({
      components: { InvestmentSection },
      setup() {
        const investments = ref<null | { ordinary_dividends_cents: number; qualified_dividends_cents: number; short_term_gain_cents: number; long_term_gain_cents: number; federal_withholding_cents: number; california_withholding_cents: number; notes: null }>(null)
        return { investments }
      },
      template: '<InvestmentSection v-model="investments" :warnings="[]" :errors="[]" />'
    })
    const wrapper = mount(Harness, { global })
    await wrapper.findAll('button').find(button => button.text() === 'Add')!.trigger('click')
    await nextTick()

    expect((wrapper.vm as { investments: object | null }).investments).toEqual({ ordinary_dividends_cents: 0, qualified_dividends_cents: 0, short_term_gain_cents: 0, long_term_gain_cents: 0, federal_withholding_cents: 0, california_withholding_cents: 0, notes: null })
    expect(wrapper.findComponent({ name: 'InvestmentEntryModal' }).props('open')).toBe(true)
  })

  it('applies an investment entry delta without changing unrelated aggregates', async () => {
    const model = { ordinary_dividends_cents: 100, qualified_dividends_cents: 50, short_term_gain_cents: -1, long_term_gain_cents: 0, federal_withholding_cents: 0, california_withholding_cents: 0, notes: 'Keep this.' }
    const wrapper = mount(InvestmentSection, { props: { modelValue: model, warnings: [], errors: [] }, global })
    wrapper.findComponent({ name: 'InvestmentEntryModal' }).vm.$emit('add', { field: 'ordinary_dividends_cents', amount_cents: -25 })
    await nextTick()
    expect(wrapper.emitted('update:modelValue')![0]![0]).toEqual({ ...model, ordinary_dividends_cents: 75 })
  })

  it('keeps the second spouse independent when the first spouse section emits an edit', async () => {
    const first = mount(PaystubSection, { props: { modelValue: paystub, spouse: 'spouse_1', label: 'Spouse 1', warnings: [], errors: [] }, global })
    await first.find('.money').trigger('click')
    expect((first.emitted('update:modelValue')![0]![0] as typeof paystub).current_period_regular_wages_cents).toBe(999)
    expect(paystub.current_period_regular_wages_cents).toBe(12345)
    expect(first.text()).toContain('Regular Wages')
    expect(first.text()).toContain('Federal Taxable Wages')
    expect(first.text()).toContain('California SDI Withholding')
    expect(first.text()).not.toContain('Current Period')
  })

  it('maps projection modes, exposes custom dates, and follows the paystub date', async () => {
    const wrapper = mount(PaystubSection, { props: { modelValue: { ...paystub, projection_end_date: null }, spouse: 'spouse_1', label: 'Spouse 1', warnings: [], errors: [{ path: 'paystubs.spouse_1.projection_end_date', code: 'invalid', message: 'Choose a later date.' }] }, global })
    const select = wrapper.findAllComponents({ name: 'USelect' })[1]!
    expect(select.props('modelValue')).toBe('year_end')
    select.vm.$emit('update:modelValue', 'after_paystub')
    await nextTick()
    expect(wrapper.emitted('update:modelValue')![0]![0]).toEqual({ ...paystub, projection_end_date: paystub.date })
    await wrapper.setProps({ modelValue: { ...paystub, projection_end_date: paystub.date } })
    await wrapper.findAllComponents({ name: 'UInput' })[0]!.vm.$emit('update:modelValue', '2026-06-15')
    expect(wrapper.emitted('update:modelValue')![1]![0]).toEqual({ ...paystub, date: '2026-06-15', projection_end_date: '2026-06-15' })
    select.vm.$emit('update:modelValue', 'date')
    await nextTick()
    expect(select.props('modelValue')).toBe('date')
    expect(wrapper.findAllComponents({ name: 'UInput' })).toHaveLength(2)
    await wrapper.setProps({ modelValue: { ...paystub, projection_end_date: '2026-10-31' } })
    expect(select.props('modelValue')).toBe('date')
  })

  it('applies calculator results only to their intended paystub fields', async () => {
    const wrapper = mount(PaystubSection, { props: { modelValue: paystub, spouse: 'spouse_1', label: 'Spouse 1', warnings: [], errors: [] }, global })
    const calculators = wrapper.findAllComponents({ name: 'GrossPayCalculatorModal' })

    calculators[1]!.vm.$emit('apply', { mode: 'year_to_date', federal_taxable_wages_cents: 120_001, california_taxable_wages_cents: 120_002 })
    await nextTick()
    const yearToDate = wrapper.emitted('update:modelValue')![0]![0] as typeof paystub
    expect(yearToDate).toEqual({ ...paystub, federal_taxable_wages_ytd_cents: 120_001, california_taxable_wages_ytd_cents: 120_002 })

    calculators[0]!.vm.$emit('apply', { mode: 'current_period', regular_wages_cents: 12_000 })
    await nextTick()
    const currentPeriod = wrapper.emitted('update:modelValue')![1]![0] as typeof paystub
    expect(currentPeriod).toEqual({ ...paystub, current_period_regular_wages_cents: 12_000 })
  })

  it('does not change the other spouse when a calculator result is applied', async () => {
    const otherPaystub = { ...paystub, current_period_regular_wages_cents: 54_321 }
    const Harness = defineComponent({
      components: { PaystubSection },
      setup() {
        const paystubs = ref({ spouse_1: { ...paystub }, spouse_2: otherPaystub })
        return { paystubs }
      },
      template: '<div><PaystubSection v-model="paystubs.spouse_1" spouse="spouse_1" label="One" :warnings="[]" :errors="[]" /><PaystubSection v-model="paystubs.spouse_2" spouse="spouse_2" label="Two" :warnings="[]" :errors="[]" /></div>'
    })
    const wrapper = mount(Harness, { global })
    const first = wrapper.findAllComponents(PaystubSection)[0]!
    first.findAllComponents({ name: 'GrossPayCalculatorModal' })[1]!.vm.$emit('apply', {
      mode: 'year_to_date', federal_taxable_wages_cents: 120_001, california_taxable_wages_cents: 120_002
    })
    await nextTick()

    expect((wrapper.vm as { paystubs: { spouse_2: typeof paystub } }).paystubs.spouse_2).toEqual(otherPaystub)
  })
})
