import type { Cents } from '~/types/api'

export interface GrossPayDeductions {
  gross_earnings_cents: Cents
  traditional_retirement_cents: Cents
  pre_tax_health_premiums_cents: Cents
  pre_tax_fsa_cents: Cents
  employee_hsa_payroll_cents: Cents
  other_shared_pre_tax_deductions_cents: Cents
}

export interface GrossPayCalculation {
  shared_deductions_cents: Cents
  federal_only_hsa_deduction_cents: Cents
  federal_taxable_wages_cents: Cents
  california_taxable_wages_cents: Cents
}

export type GrossPayCalculationResult =
  | { valid: true; calculation: GrossPayCalculation }
  | { valid: false; message: string }

export function calculateGrossPayTaxableWages(deductions: GrossPayDeductions): GrossPayCalculationResult {
  const amounts = Object.values(deductions)
  if (amounts.some(amount => !Number.isSafeInteger(amount) || amount < 0)) {
    return { valid: false, message: 'All amounts must be nonnegative whole cents.' }
  }

  const shared_deductions_cents = deductions.traditional_retirement_cents +
    deductions.pre_tax_health_premiums_cents + deductions.pre_tax_fsa_cents +
    deductions.other_shared_pre_tax_deductions_cents
  if (shared_deductions_cents > deductions.gross_earnings_cents) {
    return { valid: false, message: 'Shared deductions cannot exceed gross earnings.' }
  }

  const federal_only_hsa_deduction_cents = deductions.employee_hsa_payroll_cents
  if (shared_deductions_cents + federal_only_hsa_deduction_cents > deductions.gross_earnings_cents) {
    return { valid: false, message: 'Shared deductions plus HSA contributions cannot exceed gross earnings.' }
  }

  return {
    valid: true,
    calculation: {
      shared_deductions_cents,
      federal_only_hsa_deduction_cents,
      federal_taxable_wages_cents: deductions.gross_earnings_cents - shared_deductions_cents - federal_only_hsa_deduction_cents,
      california_taxable_wages_cents: deductions.gross_earnings_cents - shared_deductions_cents
    }
  }
}
