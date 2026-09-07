const loopbackHosts = new Set(['127.0.0.1', '::1', 'localhost'])

export function validatedBackendOrigin(value: string): string {
  let origin: URL
  try {
    origin = new URL(value)
  } catch {
    throw new Error('NUXT_BACKEND_ORIGIN must be a valid HTTP loopback origin.')
  }

  if (origin.protocol !== 'http:' || !loopbackHosts.has(origin.hostname) || origin.username || origin.password ||
    origin.pathname !== '/' || origin.search || origin.hash) {
    throw new Error('NUXT_BACKEND_ORIGIN must be a valid HTTP loopback origin.')
  }
  return origin.origin
}
