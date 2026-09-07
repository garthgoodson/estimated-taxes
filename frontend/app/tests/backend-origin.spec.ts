import { describe, expect, it } from 'vitest'
import { validatedBackendOrigin } from '../../server/utils/backendOrigin'

describe('backend origin configuration', () => {
  it('accepts default and nondefault HTTP loopback origins', () => {
    expect(validatedBackendOrigin('http://127.0.0.1:8080')).toBe('http://127.0.0.1:8080')
    expect(validatedBackendOrigin('http://localhost:9090')).toBe('http://localhost:9090')
  })

  it('rejects non-loopback, non-HTTP, and path-bearing origins', () => {
    expect(() => validatedBackendOrigin('https://127.0.0.1:8080')).toThrow('valid HTTP loopback origin')
    expect(() => validatedBackendOrigin('http://example.com:8080')).toThrow('valid HTTP loopback origin')
    expect(() => validatedBackendOrigin('http://127.0.0.1:8080/api')).toThrow('valid HTTP loopback origin')
  })
})
