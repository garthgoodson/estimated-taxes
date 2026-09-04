import { describe, expect, it } from 'vitest'
import { isBootstrapState, normalizeApiError } from '~/utils/api'

describe('API boundaries', () => {
  it('accepts the documented bootstrap minimum shape', () => {
    expect(isBootstrapState({ tax_year: 2026, current_quarter: 3, quarters: [] })).toBe(true)
    expect(isBootstrapState({ tax_year: 2025, current_quarter: 3, quarters: [] })).toBe(false)
    expect(isBootstrapState({ tax_year: 2026, current_quarter: '3', quarters: [] })).toBe(false)
  })

  it('normalizes validation and backend envelopes', () => {
    const validation = normalizeApiError({ status: 422, data: { error: { code: 'validation_failed', message: 'Invalid amount.', fields: [{ path: 'payments.federal', code: 'invalid', message: 'Invalid.' }] } } })
    expect(validation.kind).toBe('validation')
    expect(validation.fields).toHaveLength(1)
    expect(normalizeApiError({ status: 500, data: { error: { code: 'internal_error', message: 'Failed.' } } }).kind).toBe('backend')
  })

  it('handles transport and malformed responses', () => {
    expect(normalizeApiError(new Error('offline')).kind).toBe('transport')
    expect(isBootstrapState({ unexpected: true })).toBe(false)
  })
})
