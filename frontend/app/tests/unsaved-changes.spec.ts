import { describe, expect, it, vi } from 'vitest'
import { confirmDiscard, protectBeforeUnload } from '~/utils/unsavedChanges'

describe('quarter unsaved-change guards', () => {
  it('allows clean navigation without prompting and respects discard confirmation', () => {
    const confirm = vi.fn(() => false)
    expect(confirmDiscard(false, confirm)).toBe(true)
    expect(confirm).not.toHaveBeenCalled()
    expect(confirmDiscard(true, confirm)).toBe(false)
    expect(confirmDiscard(true, () => true)).toBe(true)
  })

  it('protects browser unload only while dirty', () => {
    const clean = { preventDefault: vi.fn(), returnValue: undefined } as unknown as BeforeUnloadEvent
    protectBeforeUnload(false, clean)
    expect(clean.preventDefault).not.toHaveBeenCalled()

    const dirty = { preventDefault: vi.fn(), returnValue: undefined } as unknown as BeforeUnloadEvent
    protectBeforeUnload(true, dirty)
    expect(dirty.preventDefault).toHaveBeenCalledOnce()
    expect(dirty.returnValue).toBe('')
  })
})
