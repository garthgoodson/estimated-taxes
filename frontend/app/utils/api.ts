import type { ApiError, ApiErrorEnvelope, ApiValidationField, ApiWarning, BootstrapState, Cents } from '~/types/api'

type JsonRecord = Record<string, unknown>

function isRecord(value: unknown): value is JsonRecord {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

function isCents(value: unknown): value is Cents {
  return typeof value === 'number' && Number.isSafeInteger(value)
}

function isOptionalCents(value: unknown): value is Cents | null {
  return value === null || isCents(value)
}

function hasCents(record: JsonRecord, fields: string[]): boolean {
  return fields.every(field => isCents(record[field]))
}

function isWarning(value: unknown): value is ApiWarning {
  if (!isRecord(value) || typeof value.code !== 'string' || typeof value.message !== 'string') return false
  return ['blocking', 'caution', 'information'].includes(value.severity as string) &&
    (value.path === undefined || typeof value.path === 'string')
}

function isJurisdictionTax(value: unknown): boolean {
  return isRecord(value) && isCents(value.annual_liability_cents) &&
    isOptionalCents(value.remaining_obligation_cents) &&
    isOptionalCents(value.current_recommendation_cents)
}

export function isBootstrapState(value: unknown): value is BootstrapState {
  if (!isRecord(value) || value.tax_year !== 2026 || typeof value.as_of_date !== 'string' ||
    ![1, 2, 3, 4].includes(value.current_quarter as number) || !isRecord(value.household) ||
    !isRecord(value.actuals) || !isRecord(value.projection) || !isRecord(value.tax) ||
    !Array.isArray(value.quarters) || !Array.isArray(value.warnings)) return false

  const actuals = ['federal_wages_ytd_cents', 'california_wages_ytd_cents', 'ordinary_dividends_cents', 'qualified_dividends_cents', 'short_term_gain_cents', 'long_term_gain_cents', 'federal_withholding_ytd_cents', 'california_withholding_ytd_cents', 'federal_estimated_payments_cents', 'california_estimated_payments_cents']
  const projection = ['federal_wages_cents', 'california_wages_cents', 'federal_withholding_cents', 'california_withholding_cents']
  const spouses = value.household.spouses

  return Array.isArray(spouses) && spouses.every(spouse => isRecord(spouse) &&
    ['spouse_1', 'spouse_2'].includes(spouse.key as string) && typeof spouse.label === 'string') &&
    hasCents(value.actuals, actuals) && hasCents(value.projection, projection) &&
    isJurisdictionTax(value.tax.federal) && isJurisdictionTax(value.tax.california) &&
    value.quarters.every(quarter => isRecord(quarter) && [1, 2, 3, 4].includes(quarter.quarter as number) &&
      ['complete', 'in_progress', 'not_started'].includes(quarter.status as string)) &&
    value.warnings.every(isWarning)
}

function isErrorEnvelope(value: unknown): value is ApiErrorEnvelope {
  if (!isRecord(value) || !isRecord(value.error) || typeof value.error.code !== 'string' || typeof value.error.message !== 'string') return false
  return value.error.fields === undefined || (Array.isArray(value.error.fields) && value.error.fields.every(isValidationField))
}

function isValidationField(value: unknown): value is ApiValidationField {
  return isRecord(value) && typeof value.path === 'string' && typeof value.code === 'string' && typeof value.message === 'string'
}

export function isApiError(value: unknown): value is ApiError {
  return isRecord(value) && ['validation', 'backend', 'transport', 'malformed_response'].includes(value.kind as string) &&
    typeof value.code === 'string' && typeof value.message === 'string' && Array.isArray(value.fields) && value.fields.every(isValidationField)
}

export function normalizeApiError(error: unknown): ApiError {
  if (isRecord(error) && typeof error.status === 'number') {
    if (isErrorEnvelope(error.data)) {
      return { kind: error.status === 422 ? 'validation' : 'backend', status: error.status, code: error.data.error.code, message: error.data.error.message, fields: error.data.error.fields ?? [] }
    }
    return { kind: 'backend', status: error.status, code: 'http_error', message: typeof error.message === 'string' ? error.message : 'The request failed.', fields: [] }
  }
  return { kind: 'transport', code: 'transport_error', message: 'The backend could not be reached.', fields: [] }
}

export type ApiFetcher = (url: string, options?: object) => Promise<unknown>

export function createApiClient(apiBase: string, fetcher: ApiFetcher) {
  async function request<T>(path: string, validate: (value: unknown) => value is T, options?: object): Promise<T> {
    try {
      const response = await fetcher(`${apiBase}${path}`, options)
      if (!validate(response)) {
        throw { kind: 'malformed_response', code: 'malformed_response', message: 'The backend returned an unexpected response.', fields: [] } satisfies ApiError
      }
      return response
    } catch (error) {
      if (isApiError(error)) throw error
      throw normalizeApiError(error)
    }
  }

  return { getBootstrap: () => request('/2026', isBootstrapState), request }
}
