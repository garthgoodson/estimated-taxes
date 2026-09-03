# Backend 5: Quarterly Payment Recommendations

## Responsibility

Convert projected annual federal and California liabilities into the amounts currently owed for the selected installment.

The application targets projected current-year liability only. It does not calculate safe harbor, annualized installments, penalties, or interest.

## Annual target

For each jurisdiction:

```text
projected annual liability
- projected full-year withholding
= annual estimated-payment target
```

The target is floored at zero and represents 100% of the projected balance after withholding.

## Default installment schedules

### Federal

| Quarter | Period | Cumulative |
| --- | ---: | ---: |
| Q1 | 25% | 25% |
| Q2 | 25% | 50% |
| Q3 | 25% | 75% |
| Q4 | 25% | 100% |

### California

| Quarter | Period | Cumulative |
| --- | ---: | ---: |
| Q1 | 30% | 30% |
| Q2 | 40% | 70% |
| Q3 | 0% | 70% |
| Q4 | 30% | 100% |

The active tax-rule revisions provide the actual dates and percentages.

## As-of date

The application orchestration layer resolves the as-of date through an injectable narrow clock abstraction. In normal use it returns the host operating system's current local calendar date, using the operating system's configured local timezone; tests supply a fixed implementation. Projection, recommendation, validation, result-composition, and snapshot functions continue to receive this date explicitly and must never read the system clock directly.

Each application operation resolves the date once at its start and passes that one value through validation, projection, tax calculation, recommendations, persistence checks, and response composition. It must not resolve the clock again during that operation.

There is no persisted current application date and no editable API field or endpoint for it. Saved snapshots retain their captured date; reading a snapshot later does not recalculate it.

## Current recommendation

The current installment is the calendar quarter containing the as-of date. The configured due date is displayed separately and determines whether the result is upcoming, due today, or past due. Viewing a historic quarter does not replace the current installment used by the current annual recommendation.

For installment `i`, let:

- `A` be the annual estimated-payment target
- `C_i` be its cumulative schedule percentage
- `T_i = round(A x C_i)` be its cumulative target
- `T_0 = 0`
- `M_i` be actual payments assigned to quarters 1 through `i` whose payment date is on or before the as-of date

```text
scheduled current-installment portion = T_i - T_(i-1)
recommended payment = max(0, T_i - M_i)
catch-up portion = max(0, recommended payment - scheduled current-installment portion)
scheduled portion of recommendation = recommended payment - catch-up portion
```

Percent multiplication uses the common cent-rounding rule. Defining scheduled portions as differences between rounded cumulative targets makes all four installments reconcile exactly to `A`; any remainder lands in the final installment.

## Payment behavior

- Federal and California are independent.
- All payments recorded through the as-of date reduce the relevant cumulative target.
- A payment is credited according to its quarter assignment only when its date is on or before the as-of date.
- A payment date after the as-of date is invalid because payments represent completed facts, not plans.
- A prior-year overpayment applied to 2026 may be represented as a Q1 payment.
- Excess payment reduces later recommendations.
- A rising projection produces a current catch-up amount.
- A falling projection may reduce the recommendation to zero.
- California Q3 can show catch-up even though its scheduled period percentage is zero.

## Status model

A recommendation result has separate status dimensions. There is no global precedence between them.

### Calculation status

- `available`
- `insufficient_information`

`insufficient_information` represents a blocking validation outcome. When it applies, the application does not produce a recommendation status or valid recommendation amounts, and does not substitute zero for unknown results. The due-date status may still be present only when the active installment and its date can be determined independently from valid rule data. Any due-date status shown does not imply that a payment amount has been calculated.

### Recommendation status

This status is present only when calculation status is `available`. It is mutually exclusive and selected as follows:

1. `catch_up_recommended` when recommended payment is greater than zero and catch-up portion is greater than zero.
2. `payment_recommended` when recommended payment is greater than zero and catch-up portion is zero.
3. `no_payment_currently_needed` when recommended payment is zero.

`catch_up_recommended` is the more specific form of a positive recommendation; it does not also emit `payment_recommended`.

### Due-date status

This status is independent of recommendation status:

- `upcoming` when the as-of date is before the installment due date
- `due_today` when the dates are equal
- `past_due` when the as-of date is after the installment due date

Past due is explanatory and does not calculate penalties or imply that payment is necessarily owed.

### Projected overpayment

Projected overpayment is an independent annual-position amount, not a replacement for recommendation status:

```text
max(0, projected annual withholding + all actual jurisdiction payments through the as-of date
       - projected annual liability)
```

The corresponding projected-overpayment condition is true only when this amount is greater than zero. It may coexist with `no_payment_currently_needed`, or with another recommendation status when quarter assignment causes the cumulative-installment calculation to differ from the overall projected annual position. Preserve both facts rather than hiding one with precedence.

All actual payments for projected annual position include every completed payment dated on or before the as-of date. Quarter assignment continues to control which payments reduce the current cumulative installment target.

## Output

For each jurisdiction:

- Projected annual liability
- Projected annual withholding
- Annual estimated-payment target
- Current installment and due date
- Cumulative percentage and target
- Payments already made
- Scheduled current-installment portion
- Catch-up portion
- Recommended payment now
- Remaining projected annual amount
- Simple future-installment outlook
- Rule revision used

## Reconciliation

The unpaid annual target before a recommendation is `max(0, A - all actual payments through the as-of date)`. The remaining amount after the recommendation is `max(0, A - all actual payments through the as-of date - recommended payment)`.

A simple future-installment outlook applies the same cumulative formula in date order, assumes the current recommendation is paid, and assumes no further change in liability, withholding, or rules. It is explanatory only and is recalculated whenever inputs change.

## Exclusions

- Prior-year safe harbor
- Current-year safe-harbor testing
- Higher-income safe-harbor rules
- Annualized-income installment method
- Underpayment penalties and interest
- Payment fees and transmission
- Farmer, fisherman, or fiscal-year rules
