import { ref } from 'vue'
import type { ApiError, SnapshotDetail, SnapshotSummary } from '~/types/api'

type SnapshotClient = {
  listSnapshots: () => Promise<SnapshotSummary[]>
  createSnapshot: (label: string) => Promise<SnapshotSummary>
  getSnapshot: (id: string) => Promise<SnapshotDetail>
  renameSnapshot: (id: string, label: string) => Promise<SnapshotSummary>
  deleteSnapshot: (id: string) => Promise<unknown>
}

export function createSnapshots(client: SnapshotClient) {
  const summaries = ref<SnapshotSummary[]>([])
  const selected = ref<SnapshotDetail | null>(null)
  const pending = ref(false)
  const detailPending = ref(false)
  const error = ref<ApiError | null>(null)

  async function load() {
    pending.value = true
    error.value = null
    try {
      summaries.value = await client.listSnapshots()
    } catch (cause) {
      error.value = cause as ApiError
    } finally {
      pending.value = false
    }
  }

  async function select(id: string) {
    detailPending.value = true
    error.value = null
    selected.value = null
    try {
      selected.value = await client.getSnapshot(id)
    } catch (cause) {
      error.value = cause as ApiError
    } finally {
      detailPending.value = false
    }
  }

  async function create(label: string) {
    const summary = await client.createSnapshot(label)
    summaries.value = [summary, ...summaries.value]
  }

  async function rename(id: string, label: string) {
    const updated = await client.renameSnapshot(id, label)
    summaries.value = summaries.value.map(summary => summary.id === id ? updated : summary)
    selected.value = null
  }

  async function remove(id: string) {
    await client.deleteSnapshot(id)
    summaries.value = summaries.value.filter(summary => summary.id !== id)
    if (selected.value?.id === id) selected.value = null
  }

  return { summaries, selected, pending, detailPending, error, load, select, create, rename, remove }
}

export function useSnapshots() {
  return createSnapshots(useApi())
}
