import type { ApiWarning, Cents, IsoDate, Jurisdiction, PaystubInput, QuarterInput } from '~/types/api'

export type QuarterNumber = 1 | 2 | 3 | 4
export type ProjectionAmounts = { actual_cents: Cents; projected_remaining_cents: Cents; projected_annual_cents: Cents }
export type AnnualProjection = {
  federal_wages: ProjectionAmounts
  california_wages: ProjectionAmounts
  federal_withholding: ProjectionAmounts
  california_withholding: ProjectionAmounts
  investments: NonNullable<QuarterInput['investments']>
  spouses: Array<{ key: 'spouse_1' | 'spouse_2'; authoritative_quarter: QuarterNumber | null; remaining_pay_periods: number; federal_wages: ProjectionAmounts; california_wages: ProjectionAmounts; federal_withholding: ProjectionAmounts; california_withholding: ProjectionAmounts }>
  warnings: ApiWarning[]
}
export type TaxResult = {
  details: { annual_liability_cents: Cents; projected_withholding_cents: Cents; amount_requiring_estimated_payments_cents: Cents }
  warnings: ApiWarning[]
}
export type Recommendation = {
  calculation_status: 'available' | 'insufficient_information'
  projected_overpayment_cents: Cents
  projected_overpayment: boolean
  recommendation_status: 'payment_recommended' | 'catch_up_recommended' | 'no_payment_currently_needed' | null
  due_date_status: 'upcoming' | 'due_today' | 'past_due' | null
  annual_target_cents: Cents | null
  cumulative_target_cents: Cents | null
  payments_credited_cents: Cents | null
  scheduled_installment_cents: Cents | null
  scheduled_recommendation_cents: Cents | null
  catch_up_recommendation_cents: Cents | null
  recommended_payment_cents: Cents | null
  remaining_before_recommendation_cents: Cents | null
  remaining_after_recommendation_cents: Cents | null
  current_quarter: QuarterNumber
  due_date: IsoDate | null
  future_outlook: Array<{ quarter: QuarterNumber; cumulative_target_cents: Cents; recommended_payment_cents: Cents }>
  rule_revision_id: string
}
export type RecommendationValidation = { severity: 'blocking' | 'caution' | 'informational'; jurisdiction: Jurisdiction | null; code: string; message: string }
export type QuarterResource = {
  tax_year: 2026
  quarter: QuarterNumber
  input: QuarterInput
  result: {
    as_of_date: IsoDate
    annual_summary: AnnualProjection
    federal: TaxResult
    california: TaxResult
    current_recommendations: Record<Jurisdiction, Recommendation> & { validation: RecommendationValidation[] }
  }
  warnings: ApiWarning[]
}

export type QuarterForm = QuarterInput
export type { PaystubInput }
