import type { ApiError, BootstrapState } from '~/types/api'

export function useBootstrap() {
  const state = useState<BootstrapState | null>('bootstrap:data', () => null)
  const pending = useState('bootstrap:pending', () => false)
  const error = useState<ApiError | null>('bootstrap:error', () => null)

  async function load() {
    pending.value = true
    error.value = null
    try {
      state.value = await useApi().getBootstrap()
    } catch (cause) {
      error.value = cause as ApiError
    } finally {
      pending.value = false
    }
  }

  return { state, pending, error, load }
}
