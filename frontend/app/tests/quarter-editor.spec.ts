import { describe, expect, it, vi } from 'vitest'
import { createQuarterEditor } from '~/composables/useQuarterEditor'
import type { QuarterResource } from '~/types/quarter'

const response = { input: { paystubs: { spouse_1: null, spouse_2: null }, investments: null, payments: { federal: { amount_cents: 0, date: null }, california: { amount_cents: 0, date: null } } } } as QuarterResource
const apiError = { kind: 'transport', code: 'offline', message: 'Offline', fields: [] } as const

describe('quarter editor workflow', () => {
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
    resolve({ ...response, input: { ...response.input, payments: { ...response.input.payments, federal: { amount_cents: 123, date: null } } } }); await Promise.all([first, second])
    expect(editor.dirty.value).toBe(false); expect(editor.saved.value).toBe(true)
  })
  it('retains edits and exposes backend validation after a failed save', async () => {
    const client = { getQuarter: vi.fn().mockResolvedValue(response), saveQuarter: vi.fn().mockRejectedValue({ kind: 'validation', code: 'invalid', message: 'Invalid', fields: [{ path: 'investments.qualified_dividends_cents', code: 'bad', message: 'Too high' }] }) }
    const editor = createQuarterEditor(client, 3); await editor.load(); editor.form.value.payments.federal.amount_cents = 7; await editor.save()
    expect(editor.form.value.payments.federal.amount_cents).toBe(7); expect(editor.validationErrors.value[0]?.path).toBe('investments.qualified_dividends_cents')
  })
})
