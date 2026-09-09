# Gross-pay calculator

## Scope

Add a frontend-only paystub-entry convenience. It normalizes user-entered gross pay and known payroll deductions into existing paystub fields; it does not persist deduction details, call the API, or implement tax-liability rules.

## Calculation

The calculator accepts nonnegative integer-cent amounts for gross pay, traditional pre-tax retirement, pre-tax health premiums, pre-tax FSA, employee HSA payroll deductions, and other shared pre-tax deductions.

- Shared deductions are retirement, health premiums, FSA, and other shared deductions.
- Federal taxable wages are gross minus shared deductions minus HSA.
- California taxable wages are gross minus shared deductions.
- Reject shared deductions greater than gross and combined shared deductions plus HSA greater than gross.
- HSA means only employee payroll deductions included in entered gross; employer contributions are excluded.

YTD gross includes all earnings, including bonuses and holiday pay, and Apply updates both existing taxable-wage fields atomically. Current-period gross includes only recurring regular gross wages, excluding bonus and other nonrecurring pay; Apply uses the federal result for the existing shared regular-wage field and visibly explains the California limitation before Apply.

## Implementation

1. Add a tested pure calculator utility.
2. Add a reusable modal with local, reset-on-cancel/apply/reopen state and an explicit arithmetic breakdown.
3. Add optional MoneyInput header actions and integrate the YTD and current-period calculators into PaystubSection.
4. Document the narrow input-normalization exception and add unit/component coverage.

## Verification

Test formulas, cents, all validation boundaries, mode-specific apply payloads, reset behavior, atomic YTD apply, current-only regular-wage apply, and unchanged unrelated spouse data. Run typecheck, tests, lint, production build, and diff check.
