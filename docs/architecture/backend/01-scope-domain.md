# Backend 1: Scope and Domain

## Responsibility

Define the supported tax case, the business concepts owned by the backend, and the boundaries that later backend work must preserve.

## Tax Year Case

The central aggregate is a Tax Year Case containing:

- One copied household
- One or two spouses
- Four quarters
- Federal and California tax rules for that exact year
- Current derived result while open, or a protected closure snapshot while closed
- Optional saved calculation snapshots

The implemented MVP has one open 2026 case. Approved multi-year case lifecycle is defined in [Multi-year tax cases](08-multi-year-cases.md).

The filing status is married filing jointly. The household is assumed to be a full-year California resident household.

## Household

The household contains:

- Spouse labels
- Age/blindness information needed by supported deductions
- Fixed filing and residency assumptions
- Quarterly information
- Saved calculation snapshots

The application does not store Social Security numbers, addresses, employer identifiers, brokerage account numbers, bank information, or tax documents.

## Quarter

Q1-Q4 are the primary input periods. Each quarter may contain:

- One latest paystub snapshot for each spouse
- One household investment summary
- One federal estimated-payment record
- One California estimated-payment record

Historic quarters remain editable. Changing a historic quarter recalculates the current annual result but does not alter saved snapshots.

## Paystub snapshot

A paystub snapshot represents the latest available paystub for one spouse in the selected quarter.

It contains enough information to identify:

- Paystub date, pay frequency, and optional pay-pattern projection end date
- Current-period regular compensation
- Current-period bonus compensation
- Federal taxable wages YTD
- California taxable wages YTD
- Federal income-tax withholding YTD
- California income-tax withholding YTD
- Other supported withholding values

A paystub includes a bonus when `current_period_bonus_wages_cents > 0`; this is derived and not stored separately.

`projection_end_date` is `null` to repeat the regular pay pattern through December 31, the paystub date to stop after the reported period, or a later 2026 date to limit projection through that date. It limits a pay pattern only; it is not an employment termination date and does not create a Job or Employer concept.

There is no Job concept. The MVP assumes exactly one consolidated pay source and one paystub snapshot per spouse. Multiple undifferentiated paystubs are unsupported because same-employer YTD totals overlap and must not be added together.

### Deferred multiple pay sources

A future extension may add lightweight pay sources without becoming a Job feature. Each source needs a stable `pay_source_id`, optional label, and paystub snapshots; each snapshot retains its optional projection end date. It does not require employer addresses, job titles, employment dates, or other job-management data.

For a mid-quarter transition, the old source retains its final YTD paystub and ends its projection; the new source starts with its separate YTD paystub. Projection must sum the latest authoritative paystub from each source while projecting forward only sources without an end date.

## Quarterly investment summary

Each quarter contains household totals for:

- Ordinary dividends
- Qualified portion of ordinary dividends
- Net realized short-term gain or loss
- Net realized long-term gain or loss
- Optional investment-related withholding
- Optional notes

Investment values are for that quarter only. Q1-Q4 are added together.

The application does not model accounts, securities, transactions, proceeds, basis, holding periods, lots, wash sales, or capital-loss carryovers.

## Estimated payment

Each jurisdiction may have one quarterly total containing:

- Amount paid
- Payment date

If multiple payments occurred, the user enters their combined quarterly amount.

Estimated payments and payroll withholding are distinct. Payments are actual facts and are never projected.

## Tax-rule set

A rule set is a complete version of the supported 2026 values for one jurisdiction. The application uses exactly one active federal revision and one active California revision.

Rule data is independent from household data and calculation behavior.

## Derived concepts

The backend derives but does not allow direct editing of:

- Annual wage projection
- Annual investment totals
- Federal and California taxable income
- Annual tax liabilities
- Estimated-payment targets
- Current-quarter recommendations
- Warnings

## Saved snapshot

A saved snapshot is an immutable historical calculation result with editable label metadata. It records its inputs, rules, results, recommendation, and warnings.

Snapshots support save, view, rename, and delete only. They do not support comparison, branching, or recalculation.

## Domain invariants

- A case is one exact tax year, married filing jointly, and California resident.
- Exactly one case is open; closed cases are immutable and reference a protected closure snapshot.
- A case has no more than two spouses.
- An MVP spouse has no more than one consolidated paystub snapshot per quarter. A future multiple-source model must use stable pay-source identities rather than an unstructured paystub list.
- Investments contain quarter-specific values.
- Qualified dividends cannot exceed ordinary dividends.
- Payments belong to exactly one jurisdiction and quarter.
- Federal and California rule sets cannot be interchanged.
- Derived results cannot be edited directly.
- Saved snapshots retain the rule revisions that produced them.

## Supported tax boundary

The backend supports wages, ordinary and qualified dividends, current-year short-term and long-term stock gains/losses, standard deductions, basic age-related deduction additions, supported California spouse exemption credits, NIIT, estimated Additional Medicare Tax, and California Behavioral Health Services Tax.

NIIT and Additional Medicare Tax are deliberately simplified estimates based only on the income already collected by the application. They do not introduce extra tax-form inputs.

Anything else is omitted and, when known to be relevant, reported as a warning rather than modeled through a generic adjustment. In particular, ordinary Social Security and Medicare payroll withholding and California SDI are display-only amounts; they are neither income-tax liabilities nor credits against the estimated-payment target.
