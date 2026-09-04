import type { QuarterForm, QuarterNumber, QuarterResource } from '~/types/quarter'

const quarters = [1, 2, 3, 4]
const frequencies = ['weekly', 'biweekly', 'semimonthly', 'monthly']

function record(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}
function cents(value: unknown): boolean { return typeof value === 'number' && Number.isSafeInteger(value) }
function optionalCents(value: unknown): boolean { return value === null || cents(value) }
function quarter(value: unknown): value is QuarterNumber { return quarters.includes(value as number) }
function warnings(value: unknown): boolean {
  return Array.isArray(value) && value.every(item => record(item) && ['blocking', 'caution', 'information'].includes(item.severity as string) && typeof item.code === 'string' && typeof item.message === 'string' && (item.path === undefined || typeof item.path === 'string'))
}
function projectionAmounts(value: unknown): boolean { return record(value) && cents(value.actual_cents) && cents(value.projected_remaining_cents) && cents(value.projected_annual_cents) }
function paystub(value: unknown): boolean {
  if (value === null) return true
  if (!record(value) || typeof value.date !== 'string' || !frequencies.includes(value.pay_frequency as string)) return false
  return ['current_period_regular_wages_cents', 'current_period_bonus_wages_cents', 'current_period_federal_withholding_cents', 'current_period_california_withholding_cents', 'federal_taxable_wages_ytd_cents', 'california_taxable_wages_ytd_cents', 'federal_withholding_ytd_cents', 'california_withholding_ytd_cents', 'social_security_withholding_ytd_cents', 'medicare_withholding_ytd_cents', 'california_sdi_withholding_ytd_cents'].every(field => cents(value[field]))
}
function input(value: unknown): boolean {
  if (!record(value) || !record(value.paystubs) || !paystub(value.paystubs.spouse_1) || !paystub(value.paystubs.spouse_2) || !record(value.payments)) return false
  const investments = value.investments
  const payments = value.payments
  return (investments === null || (record(investments) && ['ordinary_dividends_cents', 'qualified_dividends_cents', 'short_term_gain_cents', 'long_term_gain_cents', 'federal_withholding_cents', 'california_withholding_cents'].every(field => cents(investments[field])) && (investments.notes === null || typeof investments.notes === 'string'))) &&
    ['federal', 'california'].every(name => record(payments[name]) && cents(payments[name].amount_cents) && (payments[name].date === null || typeof payments[name].date === 'string'))
}
function recommendation(value: unknown): boolean {
  return record(value) && ['available', 'insufficient_information'].includes(value.calculation_status as string) &&
    (value.recommendation_status === null || ['payment_recommended', 'catch_up_recommended', 'no_payment_currently_needed'].includes(value.recommendation_status as string)) &&
    (value.due_date_status === null || ['upcoming', 'due_today', 'past_due'].includes(value.due_date_status as string)) && quarter(value.current_quarter) && (value.due_date === null || typeof value.due_date === 'string') &&
    cents(value.projected_overpayment_cents) && typeof value.projected_overpayment === 'boolean' && typeof value.rule_revision_id === 'string' && Array.isArray(value.future_outlook) &&
    value.future_outlook.every(item => record(item) && quarter(item.quarter) && cents(item.cumulative_target_cents) && cents(item.recommended_payment_cents)) &&
    ['annual_target_cents', 'cumulative_target_cents', 'payments_credited_cents', 'scheduled_installment_cents', 'scheduled_recommendation_cents', 'catch_up_recommendation_cents', 'recommended_payment_cents', 'remaining_before_recommendation_cents', 'remaining_after_recommendation_cents'].every(field => optionalCents(value[field]))
}
function result(value: unknown): boolean {
  if (!record(value) || typeof value.as_of_date !== 'string' || !record(value.annual_summary) || !record(value.federal) || !record(value.california) || !record(value.current_recommendations)) return false
  const projection = value.annual_summary
  const investments = projection.investments
  return ['federal_wages', 'california_wages', 'federal_withholding', 'california_withholding'].every(field => projectionAmounts(projection[field])) &&
    record(investments) && ['ordinary_dividends_cents', 'qualified_dividends_cents', 'short_term_gain_cents', 'long_term_gain_cents', 'federal_withholding_cents', 'california_withholding_cents'].every(field => cents(investments[field])) &&
    Array.isArray(projection.spouses) && warnings(projection.warnings) &&
    ['federal', 'california'].every(name => record(value[name]) && record(value[name].details) && cents(value[name].details.annual_liability_cents) && cents(value[name].details.projected_withholding_cents) && cents(value[name].details.amount_requiring_estimated_payments_cents) && warnings(value[name].warnings)) &&
    recommendation(value.current_recommendations.federal) && recommendation(value.current_recommendations.california) && Array.isArray(value.current_recommendations.validation) &&
    value.current_recommendations.validation.every(item => record(item) && ['blocking', 'caution', 'informational'].includes(item.severity as string) && (item.jurisdiction === null || ['federal', 'california'].includes(item.jurisdiction as string)) && typeof item.code === 'string' && typeof item.message === 'string')
}

export function isQuarterResource(value: unknown): value is QuarterResource {
  return record(value) && value.tax_year === 2026 && quarter(value.quarter) && input(value.input) && result(value.result) && warnings(value.warnings)
}

export function cloneQuarterForm(value: QuarterForm): QuarterForm { return structuredClone(value) }
function sameValue(left: unknown, right: unknown): boolean {
  if (Object.is(left, right)) return true
  if (Array.isArray(left) && Array.isArray(right)) return left.length === right.length && left.every((value, index) => sameValue(value, right[index]))
  if (record(left) && record(right)) {
    const leftKeys = Object.keys(left)
    return leftKeys.length === Object.keys(right).length && leftKeys.every(key => key in right && sameValue(left[key], right[key]))
  }
  return false
}
export function sameQuarterForm(left: QuarterForm, right: QuarterForm): boolean { return sameValue(left, right) }
export function emptyQuarterForm(): QuarterForm {
  return { paystubs: { spouse_1: null, spouse_2: null }, investments: null, payments: { federal: { amount_cents: 0, date: null }, california: { amount_cents: 0, date: null } } }
}
