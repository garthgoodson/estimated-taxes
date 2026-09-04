import { createApiClient } from '~/utils/api'

export function useApi() {
  const config = useRuntimeConfig()
  return createApiClient(config.public.apiBase, (url, options) => $fetch<unknown>(url, options))
}
