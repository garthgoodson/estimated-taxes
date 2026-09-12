import { describe, expect, it } from 'vitest'
import { annualTaxPosition } from '~/utils/annualTaxPosition'
import type { BootstrapState } from '~/types/api'

type AnnualTax = BootstrapState['tax']['federal']

function tax(remaining_obligation_cents: number | null, projected_overpayment_cents: number): AnnualTax {
  return { annual_liability_cents: 10_000, remaining_obligation_cents, projected_overpayment_cents, current_recommendation_cents: 0 }
}

describe('annual tax position', () => {
  it('shows remaining projected obligation when the annual balance is positive', () => {
    expect(annualTaxPosition(tax(600, 0))).toEqual({ label: 'Remaining projected obligation', amount: 600 })
  })

  it('shows projected overpayment when the annual balance is overpaid', () => {
    expect(annualTaxPosition(tax(0, 200))).toEqual({ label: 'Projected overpayment', amount: 200 })
  })

  it('shows no remaining projected obligation when exactly balanced', () => {
    expect(annualTaxPosition(tax(0, 0))).toEqual({ label: 'No remaining projected obligation', amount: 0 })
  })

  it('does not mistake an unavailable position for an exact balance', () => {
    expect(annualTaxPosition(tax(null, 0))).toEqual({ label: 'Projected position unavailable', amount: null })
  })

  it('keeps jurisdiction positions independent and reconciles their signed difference', () => {
    const federal = tax(600, 0)
    const california = tax(0, 200)
    expect((federal.remaining_obligation_cents ?? 0) - federal.projected_overpayment_cents).toBe(600)
    expect((california.remaining_obligation_cents ?? 0) - california.projected_overpayment_cents).toBe(-200)
  })
})
