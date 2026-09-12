import { describe, expect, it, vi } from 'vitest'
import { createApiClient, isBootstrapState, normalizeApiError } from '~/utils/api'

const bootstrap = {
  tax_year: 2026,
  as_of_date: '2026-09-02',
  current_quarter: 3,
  household: { spouses: [{ key: 'spouse_1', label: 'Spouse 1' }, { key: 'spouse_2', label: 'Spouse 2' }] },
  actuals: { federal_wages_ytd_cents: 0, california_wages_ytd_cents: 0, ordinary_dividends_cents: 0, qualified_dividends_cents: 0, short_term_gain_cents: 0, long_term_gain_cents: 0, federal_withholding_ytd_cents: 0, california_withholding_ytd_cents: 0, federal_estimated_payments_cents: 0, california_estimated_payments_cents: 0 },
  projection: { federal_wages_cents: 0, california_wages_cents: 0, federal_withholding_cents: 0, california_withholding_cents: 0 },
  tax: { federal: { annual_liability_cents: 0, remaining_obligation_cents: null, projected_overpayment_cents: 0, current_recommendation_cents: null }, california: { annual_liability_cents: 0, remaining_obligation_cents: null, projected_overpayment_cents: 0, current_recommendation_cents: null } },
  quarters: [{ quarter: 3, status: 'in_progress' }],
  warnings: []
}

describe('API boundaries', () => {
  it('decodes a complete documented bootstrap response', async () => {
    const fetcher = vi.fn().mockResolvedValue(bootstrap)
    await expect(createApiClient('/api', fetcher).getBootstrap()).resolves.toEqual(bootstrap)
    expect(fetcher).toHaveBeenCalledWith('/api/2026', undefined)
  })

  it('rejects incomplete successful responses as malformed', async () => {
    await expect(createApiClient('/api', vi.fn().mockResolvedValue({ tax_year: 2026, current_quarter: 3, quarters: [] })).getBootstrap())
      .rejects.toMatchObject({ kind: 'malformed_response' })
  })

  it('normalizes validation, backend, and transport errors', async () => {
    const validation = { status: 422, data: { error: { code: 'validation_failed', message: 'Invalid amount.', fields: [{ path: 'payments.federal', code: 'invalid', message: 'Invalid.' }] } } }
    await expect(createApiClient('/api', vi.fn().mockRejectedValue(validation)).getBootstrap()).rejects.toMatchObject({ kind: 'validation', fields: [expect.any(Object)] })
    expect(normalizeApiError({ status: 500, data: { error: { code: 'internal_error', message: 'Failed.' } } }).kind).toBe('backend')
    expect(normalizeApiError(new Error('offline')).kind).toBe('transport')
  })

  it('rejects malformed nested bootstrap values', () => {
    expect(isBootstrapState({ ...bootstrap, tax: {} })).toBe(false)
    expect(isBootstrapState({ ...bootstrap, warnings: [{ severity: 'informational', code: 'x', message: 'x' }] })).toBe(false)
  })

  it('uses canonical Settings write responses and rejects malformed ones', async () => {
    const household = { tax_year: 2026 as const, filing_status: 'married_filing_jointly' as const, residency: 'california_full_year' as const, spouses: [{ key: 'spouse_1' as const, label: 'One', age_65_or_older: false, blind: false }, { key: 'spouse_2' as const, label: 'Two', age_65_or_older: false, blind: false }] }
    const current_result = { as_of_date: '2026-09-02', projection: {}, tax: {}, recommendations: {} }
    const client = createApiClient('/api', vi.fn().mockResolvedValue({ household, current_result }))
    await expect(client.saveHousehold(household)).resolves.toMatchObject({ household })
    await expect(createApiClient('/api', vi.fn().mockResolvedValue({ household })).saveHousehold(household)).rejects.toMatchObject({ kind: 'malformed_response' })

    const rules = { federal: {}, california: {} }
    await expect(createApiClient('/api', vi.fn().mockResolvedValue({ rules, current_result })).saveTaxRules(rules)).resolves.toMatchObject({ rules })
    await expect(createApiClient('/api', vi.fn().mockResolvedValue({ current_result })).restoreTaxRules({ jurisdiction: 'federal', source: 'official' })).rejects.toMatchObject({ kind: 'malformed_response' })
  })
})
