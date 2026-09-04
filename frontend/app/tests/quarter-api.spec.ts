import { describe, expect, it, vi } from 'vitest'
import { createApiClient } from '~/utils/api'
import { cloneQuarterForm, isQuarterResource, sameQuarterForm } from '~/utils/quarter'
import type { QuarterResource } from '~/types/quarter'

export function resource(quarter: 1 | 2 | 3 | 4 = 3): QuarterResource {
  const input: QuarterResource['input'] = { paystubs: { spouse_1: { date: '2026-09-01', pay_frequency: 'biweekly', current_period_regular_wages_cents: 12345, current_period_bonus_wages_cents: 0, current_period_federal_withholding_cents: 100, current_period_california_withholding_cents: 50, federal_taxable_wages_ytd_cents: 1234567, california_taxable_wages_ytd_cents: 1234567, federal_withholding_ytd_cents: 10000, california_withholding_ytd_cents: 5000, social_security_withholding_ytd_cents: 1, medicare_withholding_ytd_cents: 2, california_sdi_withholding_ytd_cents: 3 }, spouse_2: null }, investments: { ordinary_dividends_cents: 100, qualified_dividends_cents: 50, short_term_gain_cents: -25, long_term_gain_cents: 0, federal_withholding_cents: 0, california_withholding_cents: 0, notes: null }, payments: { federal: { amount_cents: 0, date: null }, california: { amount_cents: 500, date: '2026-09-01' } } }
  const amount = { actual_cents: 0, projected_remaining_cents: 0, projected_annual_cents: 0 }
  const recommendation: QuarterResource['result']['current_recommendations']['federal'] = { calculation_status: 'available', projected_overpayment_cents: 0, projected_overpayment: false, recommendation_status: 'payment_recommended', due_date_status: 'upcoming', annual_target_cents: 1, cumulative_target_cents: 1, payments_credited_cents: 0, scheduled_installment_cents: 1, scheduled_recommendation_cents: 1, catch_up_recommendation_cents: 0, recommended_payment_cents: 1, remaining_before_recommendation_cents: 1, remaining_after_recommendation_cents: 0, current_quarter: 3, due_date: '2026-09-15', future_outlook: [], rule_revision_id: '1' }
  const tax = { details: { annual_liability_cents: 1, projected_withholding_cents: 0, amount_requiring_estimated_payments_cents: 1 }, warnings: [] }
  return { tax_year: 2026, quarter, input, result: { as_of_date: '2026-09-02', annual_summary: { federal_wages: amount, california_wages: amount, federal_withholding: amount, california_withholding: amount, investments: input.investments!, spouses: [], warnings: [] }, federal: tax, california: tax, current_recommendations: { federal: recommendation, california: recommendation, validation: [] } }, warnings: [] }
}

describe('quarter API boundary', () => {
  it('loads and saves one whole quarter document exactly once', async () => {
    const response = resource()
    const fetcher = vi.fn().mockResolvedValue(response)
    const client = createApiClient('/api', fetcher)
    await expect(client.getQuarter(3)).resolves.toEqual(response)
    await expect(client.saveQuarter(3, response.input)).resolves.toEqual(response)
    expect(fetcher).toHaveBeenLastCalledWith('/api/2026/quarters/3', { method: 'PUT', body: response.input })
    expect(fetcher).toHaveBeenCalledTimes(2)
  })

  it('preserves null, zero, dates, and signed cents in dirty comparisons', () => {
    const form = resource().input
    expect(sameQuarterForm(form, cloneQuarterForm(form))).toBe(true)
    expect(sameQuarterForm(form, { ...form, payments: { ...form.payments, federal: { amount_cents: 0, date: '2026-09-01' } } })).toBe(false)
    expect(sameQuarterForm(form, { ...form, investments: { ...form.investments!, short_term_gain_cents: 25 } })).toBe(false)
  })

  it('rejects malformed complete-quarter responses', () => {
    expect(isQuarterResource(resource())).toBe(true)
    expect(isQuarterResource({ ...resource(), result: {} })).toBe(false)
  })
})
