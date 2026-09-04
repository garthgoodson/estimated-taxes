import { isBootstrapState, normalizeApiError } from '~/utils/api'
import type { ApiError } from '~/types/api'

export function useApi() {
  const config = useRuntimeConfig()

  async function request<T>(path: string, validate: (value: unknown) => value is T, options?: Parameters<typeof $fetch>[1]): Promise<T> {
    try {
      const response = await $fetch<unknown>(`${config.public.apiBase}${path}`, options)
      if (!validate(response)) {
        throw { kind: 'malformed_response', code: 'malformed_response', message: 'The backend returned an unexpected response.', fields: [] } satisfies ApiError
      }
      return response
    } catch (error) {
      if (error && typeof error === 'object' && 'kind' in error) throw error
      throw normalizeApiError(error)
    }
  }

  return { getBootstrap: () => request('/2026', isBootstrapState), request }
}
