# Paystub sanity checks

## Scope

Keep internal single-paystub contradictions blocking and add advisory plausibility warnings only. Payroll corrections and job transitions remain supported as cautions, not new job or pay-source models.

## Behavior

- Block negative money, invalid dates, an end date before its paystub, current regular-plus-bonus wages above either corresponding YTD taxable wage, and current jurisdiction withholding above its YTD withholding. Return the precise spouse field path.
- Warn when jurisdiction withholding exceeds corresponding taxable wages or 50% of them, when Federal and California taxable wages differ by more than `max($10,000, 10% of the larger amount)`, and when a chronologically later paystub has lower YTD wages or withholding.
- Compare saved paystubs by date, skip missing data, and explain that payroll corrections or job transitions may be legitimate but the single-pay-source model does not fully represent job transitions.
- In the frontend-only gross-pay calculator, warn without blocking Apply when its Federal taxable result is below half of gross. Existing deductions-over-gross blocking behavior remains unchanged.

## Verification

Test threshold boundaries, date-ordered comparisons, absent paystubs, API warning serialization, field paths, calculator warnings, and the full backend/frontend suites.
