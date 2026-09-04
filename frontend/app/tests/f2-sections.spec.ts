import { mount } from '@vue/test-utils'
import { describe, expect, it } from 'vitest'
import InvestmentSection from '~/components/InvestmentSection.vue'
import PaystubSection from '~/components/PaystubSection.vue'

const paystub = { date: '2026-06-01', pay_frequency: 'biweekly' as const, current_period_regular_wages_cents: 12345, current_period_bonus_wages_cents: 0, current_period_federal_withholding_cents: 100, current_period_california_withholding_cents: 50, federal_taxable_wages_ytd_cents: 123456, california_taxable_wages_ytd_cents: 123456, federal_withholding_ytd_cents: 1000, california_withholding_ytd_cents: 500, social_security_withholding_ytd_cents: 1, medicare_withholding_ytd_cents: 2, california_sdi_withholding_ytd_cents: 3 }
const global = { stubs: { UCard: { template: '<section><slot name="header" /><slot /></section>' }, UFormField: { template: '<div><slot /></div>' }, UInput: true, USelect: true, UEmpty: { template: '<div><slot name="actions" /></div>' }, UButton: { template: '<button @click="$emit(\'click\')"><slot /></button>' }, UAlert: true, WarningList: true, MoneyInput: { template: '<button class="money" @click="$emit(\'update:modelValue\', 999)"></button>' } } }

describe('F2 section state flow', () => {
  it('emits an immutable investment replacement and displays a precise backend error', async () => {
    const model = { ordinary_dividends_cents: 100, qualified_dividends_cents: 50, short_term_gain_cents: -1, long_term_gain_cents: 0, federal_withholding_cents: 0, california_withholding_cents: 0, notes: null }
    const wrapper = mount(InvestmentSection, { props: { modelValue: model, warnings: [], errors: [{ path: 'investments.qualified_dividends_cents', code: 'invalid', message: 'Too high.' }] }, global })
    await wrapper.findAll('.money')[1]!.trigger('click')
    const value = wrapper.emitted('update:modelValue')![0]![0] as typeof model
    expect(value).toEqual({ ...model, qualified_dividends_cents: 999 })
    expect(model.qualified_dividends_cents).toBe(50)
  })

  it('keeps the second spouse independent when the first spouse section emits an edit', async () => {
    const first = mount(PaystubSection, { props: { modelValue: paystub, spouse: 'spouse_1', label: 'Spouse 1', warnings: [], errors: [] }, global })
    await first.find('.money').trigger('click')
    expect((first.emitted('update:modelValue')![0]![0] as typeof paystub).current_period_regular_wages_cents).toBe(999)
    expect(paystub.current_period_regular_wages_cents).toBe(12345)
  })
})
