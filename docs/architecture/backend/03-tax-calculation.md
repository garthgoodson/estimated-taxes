# Backend 3: Tax Calculation

## Responsibility

Convert the annual projection into separate federal and California annual tax results. Quarterly payment recommendations are handled in the next stage.

## Inputs

- Projected federal taxable wages
- Projected California taxable wages
- Recorded ordinary dividends
- Qualified portion of ordinary dividends
- Current-year net short-term gain/loss
- Current-year net long-term gain/loss
- Projected federal and California withholding
- Supported household age information
- Active federal and California rule revisions

## Income classification

| Income | Federal | California |
| --- | --- | --- |
| Federal taxable wages | Ordinary income | Not used when California wages are available |
| California taxable wages | Not used | Ordinary income |
| Ordinary dividends | Included in income | Ordinary income |
| Qualified dividends | Preferential portion of ordinary dividends | Ordinary income |
| Short-term gain/loss | Ordinary treatment | Ordinary income |
| Long-term gain/loss | Preferential treatment | Ordinary income |

Qualified dividends are included within ordinary dividends and must not be counted twice.

## Current-year capital netting

Let `S` be the summed 2026 net short-term result and `L` the summed 2026 net long-term result. Produce three nonnegative values: `ordinary capital gain`, `preferential capital gain`, and `deductible capital loss`.

| Condition | Ordinary capital gain | Preferential capital gain | Deductible capital loss |
| --- | ---: | ---: | ---: |
| `S >= 0` and `L >= 0` | `S` | `L` | `0` |
| `S < 0` and `L < 0` | `0` | `0` | `min(-(S + L), loss limit)` |
| `S >= 0` and `L < 0`, with `S + L >= 0` | `S + L` | `0` | `0` |
| `S >= 0` and `L < 0`, with `S + L < 0` | `0` | `0` | `min(-(S + L), loss limit)` |
| `S < 0` and `L >= 0`, with `S + L >= 0` | `0` | `S + L` | `0` |
| `S < 0` and `L >= 0`, with `S + L < 0` | `0` | `0` | `min(-(S + L), loss limit)` |

The federal and California rule sets each supply their loss limit. Any loss beyond the applicable limit is reported as unused and discarded; there is no carryover.

Special capital-gain categories and carryovers are outside scope.

## Shared calculation primitives

### Progressive bracket tax

A bracket has a lower bound, an optional upper bound, and a rate. Brackets cover `[lower, upper)`; the final bracket has no upper bound.

For nonnegative amount `x`:

```text
bracket tax(x) = sum over brackets(
  tax rate x width of the overlap between [0, x) and [lower, upper)
)
```

### Rounding

- All inputs and outputs are integer cents.
- Rates are integer parts per million.
- Each bracket-slice or separately stated tax multiplication is rounded to the nearest cent, with a half cent rounded away from zero.
- Addition, subtraction, minimum, maximum, and gain/loss netting use exact integer cents.
- Liability, withholding, and payment targets are never rounded to whole dollars.

## Federal stages

1. Compute supported federal AGI:

   ```text
   projected federal wages
   + ordinary dividends
   + ordinary capital gain
   + preferential capital gain
   - deductible federal capital loss
   ```

2. Compute the deduction as the MFJ standard deduction plus one age addition for each spouse age 65 or older and one blindness addition for each blind spouse.
3. Compute `taxable income = max(0, AGI - deduction)`.
4. Compute `preferential pool = min(taxable income, qualified dividends + preferential capital gain)` and `ordinary taxable income = taxable income - preferential pool`. Both values are retained as calculation-detail stages.
5. Compute ordinary tax on ordinary taxable income using the ordinary brackets, and retain the ordinary tax on all taxable income for the lesser-of comparison.
6. Stack the preferential pool immediately above ordinary taxable income. For each preferential-rate bracket, tax the overlap between `[ordinary taxable income, taxable income)` and that bracket. Qualified dividends and preferential capital gain share the same pool.
7. Set regular federal tax to the lesser of:
   - ordinary tax on all taxable income; or
   - ordinary tax on ordinary taxable income plus the stacked preferential tax.
8. Compute simplified NIIT:

   ```text
   supported net investment income = max(
     0,
     ordinary dividends
     + ordinary capital gain
     + preferential capital gain
     - deductible federal capital loss
   )

   MAGI excess = max(0, supported federal AGI - NIIT threshold)
   NIIT = NIIT rate x min(supported net investment income, MAGI excess)
   ```

   For this restricted income model, MAGI is treated as supported federal AGI. Investment expenses and other Form 8960 adjustments are not collected.
9. Compute estimated Additional Medicare Tax using projected federal taxable wages as a proxy for Medicare wages:

   ```text
   Additional Medicare Tax = rate x max(
     0,
     combined projected federal wages - MFJ threshold
   )
   ```

   This is intentionally approximate. Ordinary Medicare withholding does not offset it, and separately identified Additional Medicare Tax withholding is not collected.
10. Compute projected annual federal liability as regular federal tax plus NIIT plus estimated Additional Medicare Tax.
11. Compute projected federal withholding as projected payroll federal income-tax withholding plus recorded federal investment withholding.
12. Compute `amount requiring estimated payments = max(0, liability - withholding)`.

No federal credits are applied.

## California stages

1. Compute the California stock amount as `ordinary capital gain + preferential capital gain - deductible California capital loss`. The federal netting character is retained only to determine the net amount; California taxes all supported positive gains as ordinary income.
2. Compute supported California AGI:

   ```text
   projected California wages
   + ordinary dividends
   + ordinary capital gain
   + preferential capital gain
   - deductible California capital loss
   ```

3. Compute `taxable income = max(0, AGI - MFJ standard deduction)`.
4. Compute regular California tax with the active ordinary rate schedule. Use the rate schedule at every income level rather than reproducing the rounded tax-table lookup for lower incomes.
5. Retain regular tax before the exemption credit, then compute and retain `tax after exemption = max(0, regular tax - joint personal exemption credit)`.
6. Compute Behavioral Health Services Tax as `rate x max(0, taxable income - threshold)`.
7. Compute projected annual California liability as tax after exemption plus Behavioral Health Services Tax.
8. Compute projected California withholding as projected payroll California income-tax withholding plus recorded California investment withholding.
9. Compute `amount requiring estimated payments = max(0, liability - withholding)`.

Qualified dividends and long-term stock gains receive no preferential California treatment in this model.

## Outputs

Each jurisdiction result provides:

- Supported income total
- Standard deduction
- Taxable income
- Ordinary-income tax
- Preferential-income tax when applicable
- Supported additional taxes
- Supported California exemption credits
- Projected annual liability
- Projected withholding
- Amount requiring estimated payments
- Effective rate
- Warnings
- Rule revision used

The effective rate is `annual liability / supported AGI` when supported AGI is positive, otherwise zero. It is a display measure and is not used by any later calculation.

## Rules versus behavior

Rule data supplies brackets, rates, thresholds, deductions, credits, dates, and percentages. Application behavior owns income classification, calculation ordering, netting, stacking, and rounding.

## Required calculation checks

The calculation is not complete until deterministic examples cover:

- Zero income and income exactly at, one cent below, and one cent above every ordinary bracket boundary
- Preferential income spanning each preferential boundary after ordinary-income stacking
- All six short-term/long-term sign combinations in the capital-netting table
- Capital loss below, at, and above each jurisdiction's loss limit
- NIIT and Additional Medicare Tax below, at, and above their thresholds
- California tax below, at, and above the Behavioral Health Services Tax threshold
- Qualified dividends equal to zero and equal to ordinary dividends
- Custom rule revisions using the same fixed calculation ordering

Expected outputs for official default values should be checked against the applicable official worksheet or an independently calculated fixture.

## Official interpretation anchors

- [IRS 2026 Form 1040-ES](https://www.irs.gov/pub/irs-pdf/f1040es.pdf) supplies the 2026 standard deduction, ordinary tax schedule, and estimated-tax worksheet structure.
- [IRS Publication 505 for 2026](https://www.irs.gov/publications/p505) supplies the estimated-tax and qualified-dividend/capital-gain worksheet structure.
- [IRS NIIT guidance](https://www.irs.gov/taxtopics/tc559) defines the NIIT rate, MFJ threshold, and lesser-of calculation; this application uses the restricted approximation stated above.
- [IRS Additional Medicare Tax guidance](https://www.irs.gov/taxtopics/tc560) defines the MFJ threshold and rate; this application uses projected federal taxable wages as a proxy.
- [California 2026 Form 540-ES instructions](https://www.ftb.ca.gov/forms/2026/2026-540-es-instructions.html) direct 2026 estimates to the 2026 standard deduction, the 2025 tax schedule and exemption credit, and the Behavioral Health Services Tax worksheet.

## Warnings and accepted approximation

Relevant warnings include:

- Unsupported income is omitted
- Itemized deductions are not considered
- Federal credits are not considered
- AMT is not calculated
- Additional Medicare Tax is approximate
- NIIT omits investment expenses and Form 8960 adjustments
- Separately identified Additional Medicare Tax withholding is not collected
- California exemption-credit AGI limitation is not calculated; the fixed joint credit is used
- The 2026 enhanced federal senior deduction is not calculated
- Current-year capital loss may have an unused amount
- Custom tax rules are active

The calculation is a planning estimate and need not reproduce a complete tax return.
