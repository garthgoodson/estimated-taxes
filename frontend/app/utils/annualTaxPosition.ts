import type { BootstrapState, Cents, Jurisdiction } from '~/types/api'

export type AnnualTaxPosition = { label: string; amount: Cents | null }
type AnnualTax = BootstrapState['tax'][Jurisdiction]

export function annualTaxPosition(tax: AnnualTax | undefined): AnnualTaxPosition {
  if (!tax || tax.remaining_obligation_cents === null) return { label: 'Projected position unavailable', amount: null }
  if (tax.remaining_obligation_cents > 0) {
    return { label: 'Remaining projected obligation', amount: tax.remaining_obligation_cents }
  }
  if (tax.projected_overpayment_cents > 0) {
    return { label: 'Projected overpayment', amount: tax.projected_overpayment_cents }
  }
  return { label: 'No remaining projected obligation', amount: 0 }
}
