export type Cents = number
export type IsoDate = string
export type Jurisdiction = 'federal' | 'california'
export type WarningSeverity = 'blocking' | 'caution' | 'information' | 'informational'

export interface ApiWarning {
  severity: WarningSeverity
  code: string
  path?: string
  message: string
}

export interface ApiValidationField {
  path: string
  code: string
  message: string
}

export interface ApiErrorEnvelope {
  error: {
    code: string
    message: string
    fields?: ApiValidationField[]
  }
}

export interface BootstrapQuarter {
  quarter: 1 | 2 | 3 | 4
  status: 'complete' | 'in_progress' | 'not_started'
}

export interface BootstrapState {
  tax_year: 2026
  as_of_date: IsoDate
  current_quarter: 1 | 2 | 3 | 4
  household: {
    spouses: Array<{ key: 'spouse_1' | 'spouse_2'; label: string }>
  }
  actuals: {
    federal_wages_ytd_cents: Cents
    california_wages_ytd_cents: Cents
    ordinary_dividends_cents: Cents
    qualified_dividends_cents: Cents
    short_term_gain_cents: Cents
    long_term_gain_cents: Cents
    federal_withholding_ytd_cents: Cents
    california_withholding_ytd_cents: Cents
    federal_estimated_payments_cents: Cents
    california_estimated_payments_cents: Cents
  }
  projection: {
    federal_wages_cents: Cents
    california_wages_cents: Cents
    federal_withholding_cents: Cents
    california_withholding_cents: Cents
  }
  tax: Record<Jurisdiction, {
    annual_liability_cents: Cents
    remaining_obligation_cents: Cents | null
    current_recommendation_cents: Cents | null
  }>
  quarters: BootstrapQuarter[]
  warnings: ApiWarning[]
}

export interface ProjectionAmounts { actual_cents: Cents, projected_remaining_cents: Cents, projected_annual_cents: Cents }
export interface PaystubInput { date: IsoDate, pay_frequency: 'weekly' | 'biweekly' | 'semimonthly' | 'monthly', current_period_regular_wages_cents: Cents, current_period_bonus_wages_cents: Cents, current_period_federal_withholding_cents: Cents, current_period_california_withholding_cents: Cents, federal_taxable_wages_ytd_cents: Cents, california_taxable_wages_ytd_cents: Cents, federal_withholding_ytd_cents: Cents, california_withholding_ytd_cents: Cents, social_security_withholding_ytd_cents: Cents, medicare_withholding_ytd_cents: Cents, california_sdi_withholding_ytd_cents: Cents }
export interface QuarterInput { paystubs: { spouse_1: PaystubInput | null, spouse_2: PaystubInput | null }, investments: { ordinary_dividends_cents: Cents, qualified_dividends_cents: Cents, short_term_gain_cents: Cents, long_term_gain_cents: Cents, federal_withholding_cents: Cents, california_withholding_cents: Cents, notes: string | null } | null, payments: Record<Jurisdiction, { amount_cents: Cents, date: IsoDate | null }> }
export interface RuleBracket { lower_bound_cents: Cents, upper_bound_cents: Cents | null, rate_ppm: number }
export interface Installment { quarter: 1 | 2 | 3 | 4, due_date: IsoDate, period_ppm: number, cumulative_ppm: number }
export interface SnapshotSummary { id: string, label: string, as_of_date: IsoDate }
export interface Household { tax_year: 2026, filing_status: 'married_filing_jointly', residency: 'california_full_year', spouses: Array<{ key: 'spouse_1' | 'spouse_2', label: string, age_65_or_older: boolean, blind: boolean }> }
export interface ApiError {
  kind: 'validation' | 'backend' | 'transport' | 'malformed_response'
  status?: number
  code: string
  message: string
  fields: ApiValidationField[]
}
