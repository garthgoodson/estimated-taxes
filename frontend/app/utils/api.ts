import type { ApiError, ApiErrorEnvelope, BootstrapState } from '~/types/api'

export function isBootstrapState(value: unknown): value is BootstrapState {
  return typeof value === 'object' && value !== null &&
    (value as BootstrapState).tax_year === 2026 &&
    Number.isInteger((value as BootstrapState).current_quarter) &&
    Array.isArray((value as BootstrapState).quarters)
}

function isErrorEnvelope(value: unknown): value is ApiErrorEnvelope {
  return typeof value === 'object' && value !== null && 'error' in value &&
    typeof (value as ApiErrorEnvelope).error?.code === 'string' &&
    typeof (value as ApiErrorEnvelope).error?.message === 'string'
}

export function normalizeApiError(error: unknown): ApiError {
  if (error && typeof error === 'object' && 'status' in error) {
    const response = error as { status?: number, data?: unknown, message?: string }
    if (isErrorEnvelope(response.data)) {
      return { kind: response.status === 422 ? 'validation' : 'backend', status: response.status, code: response.data.error.code, message: response.data.error.message, fields: response.data.error.fields ?? [] }
    }
    return { kind: 'backend', status: response.status, code: 'http_error', message: response.message ?? 'The request failed.', fields: [] }
  }
  return { kind: 'transport', code: 'transport_error', message: 'The backend could not be reached.', fields: [] }
}
