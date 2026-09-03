# Backend 2: Quarterly Data and Projection

## Responsibility

Convert the latest quarterly inputs into one annual projection while keeping actual and projected values distinguishable.

The projection does not calculate tax.

## As-of date

Every projection receives its as-of date explicitly; it must not read the system clock. The as-of date determines:

- The current quarter
- Which paystub snapshot is latest
- How many pay periods remain
- Which estimated payments have occurred
- Which installment is current

For each spouse, the authoritative paystub is the paystub with the latest date on or before the as-of date. A future-dated paystub is invalid. If two saved quarters contain the same paystub date for a spouse, the later quarter is authoritative and a duplicate-date warning is returned.

## Paystub semantics

Paystub snapshots are cumulative. The latest snapshot for a spouse is authoritative for YTD wages and withholding.

Previous-quarter paystub snapshots are historical inputs and must not be summed when their values are already included in the latest YTD totals.

## Investment semantics

Investment summaries are periodic. Q1-Q4 values are added together.

They are not YTD snapshots and must never replace or subsume earlier quarters.

Only elapsed and current quarters may contain facts. A future calendar quarter must remain empty; the application does not treat future-quarter entries as expectations. An elapsed investment quarter is missing only when no investment summary was entered; an explicitly entered summary containing zero values is not missing.

## Wage projection

### Remaining-pay-period rule

The supported pay frequencies and nominal annual period counts are:

| Frequency | Periods per year |
| --- | ---: |
| Weekly | 52 |
| Biweekly | 26 |
| Semimonthly | 24 |
| Monthly | 12 |

The MVP intentionally does not collect an employer payroll calendar. It uses this deterministic approximation:

```text
completed periods = ceiling(day of year(paystub date) x periods per year / days in 2026)
remaining periods = max(0, periods per year - completed periods)
```

The paystub's current period is already included in YTD totals and is not counted again. The approximation must be shown in calculation detail and may differ from an employer calendar by one pay period.

For each spouse:

```text
latest YTD taxable wages
+ latest regular current-period wages x remaining pay periods
= projected annual taxable wages
```

Federal and California taxable wages are projected separately when their YTD values differ.

Bonus compensation is included in actual YTD wages but is not repeated over remaining pay periods.

Household projected wages are the sum of the two spouse projections. A missing spouse paystub contributes zero.

## Withholding projection

For each jurisdiction and spouse:

```text
latest YTD income-tax withholding
+ regular current-period withholding x remaining pay periods
= projected annual withholding
```

If the latest paystub contains bonus-related withholding, use the latest available non-bonus regular pattern when possible. If no suitable pattern exists, use the available information and report that withholding may be distorted.

The regular withholding amount to repeat is selected in this order:

1. If total current-period wages are zero, repeat zero and warn that withholding could not be projected. A zero-wage paystub may contain withholding adjustments, but those adjustments are not projected as recurring withholding.
2. When current-period bonus wages are zero, use the current-period withholding from the authoritative paystub.
3. Otherwise, use current-period withholding from the most recent earlier saved paystub for that spouse whose bonus wages are zero.
4. If none exists, multiply current-period withholding by `regular wages / (regular wages + bonus wages)` and warn that proportional allocation was used.

This rule is applied independently to federal and California income-tax withholding.

Only federal and California income-tax withholding reduce the corresponding annual payment target. Ordinary Social Security withholding, Medicare withholding, and California SDI may be retained for display but do not reduce the target. Separately identified Additional Medicare Tax withholding is not collected in the MVP and is therefore not projected as a credit.

## Investment totals

Annual recorded investment income is:

```text
Q1 + Q2 + Q3 + Q4
```

This is calculated independently for ordinary dividends, qualified dividends, short-term gains/losses, long-term gains/losses, and investment withholding.

The MVP does not project future dividends or stock activity. Earlier-year estimates may therefore understate annual investment income, which is an accepted product limitation.

## Missing data defaults

- Missing spouse paystub: zero for that spouse. A missing-current-quarter-paystub warning applies only when that spouse has a paystub in an earlier quarter.
- Missing investment quarter: zero
- Missing investment withholding: zero
- Missing estimated payment: zero
- Missing notes: no effect

The backend should calculate whenever the available inputs are internally consistent.

## Projection output

The projection provides:

- Actual federal and California wages YTD
- Projected remaining federal and California wages
- Projected annual federal and California wages
- Actual federal and California withholding YTD
- Projected remaining withholding
- Projected annual withholding
- Remaining-pay-period count and the regular pay/withholding pattern used for each spouse
- Recorded annual dividends
- Recorded annual short-term gain/loss
- Recorded annual long-term gain/loss

Actual plus projected wage and withholding components must reconcile with their annual totals.

## Warnings

Relevant projection warnings include:

- Missing current-quarter paystub
- Paystub appears stale when its age exceeds two nominal pay periods: 14 days for weekly, 28 days for biweekly, 31 days for semimonthly, and 62 days for monthly.
- Pay frequency is missing or inconsistent
- Bonus paystub may distort withholding projection
- Pay-period count is an approximation rather than an employer calendar
- Proportional bonus-withholding allocation was used
- Qualified dividends exceed ordinary dividends
- Elapsed investment quarter has no entered data
- Investment income is not projected into future quarters

## Exclusions

- Jobs and job lifecycle
- User-entered future wage, bonus, withholding, dividend, or gain expectations
- Raises or seasonal models
- Market or dividend forecasts
- Multiple projection scenarios
- Confidence scoring
- Capital-loss carryovers
