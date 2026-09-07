import type { ApiError, ApiErrorEnvelope, ApiValidationField, ApiWarning, BootstrapState, Cents, HouseholdResource, HouseholdWriteResponse, QuarterInput, SnapshotDetail, SnapshotSummary, TaxRulesResource, TaxRulesWriteResponse } from '~/types/api'
import type { QuarterNumber } from '~/types/quarter'
import { isQuarterResource } from '~/utils/quarter'

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

function isSnapshotSummary(value: unknown): value is SnapshotSummary {
  return isRecord(value) && typeof value.id === 'string' && typeof value.label === 'string' && typeof value.as_of_date === 'string'
}

function isSnapshotDetail(value: unknown): value is SnapshotDetail {
  if (!isRecord(value)) return false
  const currentResult = value.current_result
  if (!isSnapshotSummary(value) || !isRecord(currentResult)) return false
  const recommendations = currentResult.recommendations
  if (!isRecord(recommendations)) return false
  return ['federal', 'california'].every(jurisdiction => {
    const recommendation = recommendations[jurisdiction]
    return isRecord(recommendation) && isOptionalCents(recommendation.recommended_payment_cents)
  })
}

function isSnapshotList(value: unknown): value is SnapshotSummary[] {
  return Array.isArray(value) && value.every(isSnapshotSummary)
}

function isHousehold(value: unknown): value is HouseholdResource {
  return isRecord(value) && value.tax_year === 2026 && value.filing_status === 'married_filing_jointly' &&
    value.residency === 'california_full_year' && Array.isArray(value.spouses) && value.spouses.length === 2 &&
    value.spouses.every(spouse => isRecord(spouse) && ['spouse_1', 'spouse_2'].includes(spouse.key as string) &&
      typeof spouse.label === 'string' && typeof spouse.age_65_or_older === 'boolean' && typeof spouse.blind === 'boolean')
}

function isTaxRules(value: unknown): value is TaxRulesResource {
  return isRecord(value) && ['federal', 'california'].every(jurisdiction => isRecord(value[jurisdiction]))
}

function isCurrentResult(value: unknown): value is JsonRecord {
  return isRecord(value) && typeof value.as_of_date === 'string' && isRecord(value.projection) &&
    isRecord(value.tax) && isRecord(value.recommendations)
}

function isHouseholdWriteResponse(value: unknown): value is HouseholdWriteResponse {
  return isRecord(value) && isHousehold(value.household) && isCurrentResult(value.current_result)
}

function isTaxRulesWriteResponse(value: unknown): value is TaxRulesWriteResponse {
  return isRecord(value) && isTaxRules(value.rules) && isCurrentResult(value.current_result)
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

  return {
    getBootstrap: () => request('/2026', isBootstrapState),
    getQuarter: (quarter: QuarterNumber) => request(`/2026/quarters/${quarter}`, isQuarterResource),
    saveQuarter: (quarter: QuarterNumber, input: QuarterInput) => request(`/2026/quarters/${quarter}`, isQuarterResource, { method: 'PUT', body: input }),
    listSnapshots: () => request('/2026/snapshots', isSnapshotList),
    createSnapshot: (label: string) => request('/2026/snapshots', isSnapshotSummary, { method: 'POST', body: { label } }),
    getSnapshot: (id: string) => request(`/2026/snapshots/${id}`, isSnapshotDetail),
    renameSnapshot: (id: string, label: string) => request(`/2026/snapshots/${id}/metadata`, isSnapshotSummary, { method: 'PUT', body: { label } }),
    deleteSnapshot: (id: string) => request(`/2026/snapshots/${id}`, (_: unknown): _ is unknown => true, { method: 'DELETE' }),
    getHousehold: () => request('/2026/household', isHousehold),
    saveHousehold: (household: HouseholdResource) => request('/2026/household', isHouseholdWriteResponse, { method: 'PUT', body: household }),
    getTaxRules: () => request('/2026/tax-rules', isTaxRules),
    saveTaxRules: (rules: TaxRulesResource) => request('/2026/tax-rules', isTaxRulesWriteResponse, { method: 'PUT', body: rules }),
    restoreTaxRules: (body: { jurisdiction: 'federal' | 'california'; source: 'official' | 'revision'; revision_id?: string }) => request('/2026/tax-rules/restore', isTaxRulesWriteResponse, { method: 'POST', body }),
    restoreDatabase: (database: ArrayBuffer) => request('/restore', isBootstrapState, { method: 'POST', body: database, headers: { 'Content-Type': 'application/vnd.sqlite3' } }),
    request
  }
}
