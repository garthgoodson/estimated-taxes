import { describe, expect, it } from 'vitest'
import { calculateGrossPayTaxableWages } from '~/utils/grossPayCalculator'

const deductions = {
  gross_earnings_cents: 100_001,
  traditional_retirement_cents: 10_001,
  pre_tax_health_premiums_cents: 2_002,
  pre_tax_fsa_cents: 1_003,
  employee_hsa_payroll_cents: 4_004,
  other_shared_pre_tax_deductions_cents: 5_005
}

describe('gross-pay taxable-wage calculator', () => {
  it('preserves cents and distinguishes federal HSA treatment', () => {
    expect(calculateGrossPayTaxableWages(deductions)).toEqual({
      valid: true,
      calculation: {
        shared_deductions_cents: 18_011,
        federal_only_hsa_deduction_cents: 4_004,
        federal_taxable_wages_cents: 77_986,
        california_taxable_wages_cents: 81_990
      }
    })
  })

  it('does not subtract employer HSA contributions that are absent from gross payroll deductions', () => {
    expect(calculateGrossPayTaxableWages({ ...deductions, employee_hsa_payroll_cents: 0 })).toEqual({
      valid: true,
      calculation: expect.objectContaining({ federal_taxable_wages_cents: 81_990, california_taxable_wages_cents: 81_990 })
    })
  })

  it('rejects shared deductions exceeding gross earnings', () => {
    expect(calculateGrossPayTaxableWages({ ...deductions, gross_earnings_cents: 18_010, employee_hsa_payroll_cents: 0 })).toEqual({
      valid: false,
      message: 'Shared deductions cannot exceed gross earnings.'
    })
  })

  it('rejects combined shared deductions and HSA exceeding gross earnings', () => {
    expect(calculateGrossPayTaxableWages({ ...deductions, gross_earnings_cents: 20_000 })).toEqual({
      valid: false,
      message: 'Shared deductions plus HSA contributions cannot exceed gross earnings.'
    })
  })

  it('rejects negative inputs', () => {
    expect(calculateGrossPayTaxableWages({ ...deductions, pre_tax_fsa_cents: -1 })).toEqual({
      valid: false,
      message: 'All amounts must be nonnegative whole cents.'
    })
  })
})
