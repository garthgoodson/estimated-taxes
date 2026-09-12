import { reactive } from 'vue'
import { describe, expect, it, vi } from 'vitest'
import { createQuarterEditor } from '~/composables/useQuarterEditor'
import { cloneQuarterForm } from '~/utils/quarter'
import type { QuarterResource } from '~/types/quarter'

const response = { input: { paystubs: { spouse_1: null, spouse_2: null }, investments: null, payments: { federal: { amount_cents: 0, date: null }, california: { amount_cents: 0, date: null } } } } as QuarterResource
const apiError = { kind: 'transport', code: 'offline', message: 'Offline', fields: [] } as const

describe('quarter editor workflow', () => {
  it('serializes a payment-only form with nested reactive state', () => {
    const form = { paystubs: { spouse_1: null, spouse_2: null }, investments: null, payments: reactive({ federal: { amount_cents: 500_000, date: '2026-04-15' }, california: { amount_cents: 0, date: null } }) }

    expect(() => structuredClone(form)).toThrow()
    expect(cloneQuarterForm(form)).toEqual({
      paystubs: { spouse_1: null, spouse_2: null },
      investments: null,
      payments: { federal: { amount_cents: 500_000, date: '2026-04-15' }, california: { amount_cents: 0, date: null } }
    })
  })

  it('retries loading and replaces the saved baseline', async () => {
    const client = { getQuarter: vi.fn().mockRejectedValueOnce(apiError).mockResolvedValue(response), saveQuarter: vi.fn() }
    const editor = createQuarterEditor(client, 3)
    await editor.load(); expect(editor.error.value?.kind).toBe('transport')
    await editor.load(); expect(editor.resource.value).toEqual(response); expect(editor.dirty.value).toBe(false)
  })
  it('preserves edits after failure, prevents duplicate save, and clears dirty after success', async () => {
    let resolve!: (value: QuarterResource) => void
    const client = { getQuarter: vi.fn().mockResolvedValue(response), saveQuarter: vi.fn().mockImplementation(() => new Promise<QuarterResource>(done => { resolve = done })) }
    const editor = createQuarterEditor(client, 3); await editor.load()
    editor.form.value.payments.federal.amount_cents = 123; expect(editor.dirty.value).toBe(true)
    const first = editor.save(); const second = editor.save(); expect(client.saveQuarter).toHaveBeenCalledTimes(1)
    resolve({ ...response, input: { ...response.input, payments: { ...response.input.payments, federal: { amount_cents: 123, date: null } } } }); const [saved] = await Promise.all([first, second])
    expect(saved).toBe(true); expect(editor.dirty.value).toBe(false); expect(editor.saved.value).toBe(true)
  })
  it('ignores a stale quarter load after navigation', async () => {
    let resolveStale!: (value: QuarterResource) => void
    const stale = new Promise<QuarterResource>(resolve => { resolveStale = resolve })
    const current = { ...response, quarter: 1 } as QuarterResource
    const client = { getQuarter: vi.fn().mockReturnValueOnce(stale).mockResolvedValueOnce(current), saveQuarter: vi.fn() }
    const editor = createQuarterEditor(client, 3)
    const first = editor.load(3)
    await editor.load(1)
    resolveStale(response)
    await first
    expect(editor.resource.value).toEqual(current)
  })

  it('loads and saves a selected historic quarter through the same editor', async () => {
    const client = { getQuarter: vi.fn().mockResolvedValue(response), saveQuarter: vi.fn().mockResolvedValue(response) }
    const editor = createQuarterEditor(client, 3)
    await editor.load(1)
    editor.form.value.payments.federal.amount_cents = 456
    await editor.save()
    expect(client.getQuarter).toHaveBeenCalledWith(1)
    expect(client.saveQuarter).toHaveBeenCalledWith(1, expect.objectContaining({ payments: expect.objectContaining({ federal: { amount_cents: 456, date: null } }) }))
  })

  it('retains edits and exposes backend validation after a failed save', async () => {
    const client = { getQuarter: vi.fn().mockResolvedValue(response), saveQuarter: vi.fn().mockRejectedValue({ kind: 'validation', code: 'invalid', message: 'Invalid', fields: [{ path: 'investments.qualified_dividends_cents', code: 'bad', message: 'Too high' }] }) }
    const editor = createQuarterEditor(client, 3); await editor.load(); editor.form.value.payments.federal.amount_cents = 7; await editor.save()
    expect(editor.form.value.payments.federal.amount_cents).toBe(7); expect(editor.validationErrors.value[0]?.path).toBe('investments.qualified_dividends_cents')
  })
})
