import { describe, expect, it, vi } from 'vitest'
import { createSnapshots } from '~/composables/useSnapshots'
import { createApiClient } from '~/utils/api'

const summary = { id: '7', label: 'After Q3 update', as_of_date: '2026-09-02' }
const detail = {
  ...summary,
  current_result: {
    recommendations: {
      federal: { recommended_payment_cents: 12300 },
      california: { recommended_payment_cents: 45600 }
    }
  }
}

describe('snapshot history', () => {
  it('renders list metadata without recommendation fields', async () => {
    const fetcher = vi.fn().mockResolvedValue([summary])
    await expect(createApiClient('/api', fetcher).listSnapshots()).resolves.toEqual([summary])
    expect(fetcher).toHaveBeenCalledWith('/api/2026/snapshots', undefined)
  })

  it('fetches snapshot details only when selected', async () => {
    const client = {
      listSnapshots: vi.fn().mockResolvedValue([summary]),
      createSnapshot: vi.fn(),
      getSnapshot: vi.fn().mockResolvedValue(detail),
      renameSnapshot: vi.fn(),
      deleteSnapshot: vi.fn()
    }
    const snapshots = createSnapshots(client)

    await snapshots.load()
    expect(client.getSnapshot).not.toHaveBeenCalled()

    await snapshots.select(summary.id)
    expect(client.getSnapshot).toHaveBeenCalledOnce()
    expect(client.getSnapshot).toHaveBeenCalledWith(summary.id)
    expect(snapshots.selected.value?.current_result.recommendations.federal.recommended_payment_cents).toBe(12300)
  })
})
