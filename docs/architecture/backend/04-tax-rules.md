# Backend 4: Tax-Rule Management

## Responsibility

Manage the editable 2026 federal and California values used by calculations while preserving official defaults and historical revisions.

Rule data is stored in SQLite.

## Rule-set boundary

A rule set is one complete revision for one jurisdiction and tax year. The application uses exactly one active federal revision and one active California revision.

The supported categories are limited to values used by the approved calculation scope.

### Federal categories

- Standard deduction
- Supported age/blindness additions
- Ordinary-income brackets and rates
- Qualified-dividend and long-term capital-gain brackets and rates
- Current-year capital-loss deduction limit
- NIIT threshold and rate
- Additional Medicare threshold and rate
- Estimated-payment dates and percentages

A complete federal rule revision therefore contains:

- MFJ standard deduction
- Per-condition married age/blindness addition
- Contiguous ordinary brackets
- Contiguous preferential brackets expressed against total taxable income
- Current-year capital-loss deduction limit
- NIIT MFJ threshold and rate
- Additional Medicare Tax MFJ threshold and rate
- Four installment dates, period percentages, and cumulative percentages

### California categories

- Standard deduction
- Ordinary-income brackets and rates
- Basic spouse exemption credits
- Behavioral Health Services Tax threshold and rate
- Estimated-payment dates and percentages

A complete California rule revision therefore contains:

- MFJ standard deduction
- Contiguous ordinary brackets
- One joint personal exemption credit total for the supported two-spouse MFJ household
- Current-year capital-loss deduction limit
- Behavioral Health Services Tax threshold and rate
- Four installment dates, period percentages, and cumulative percentages

The official 2026 California defaults intentionally combine values from official publications: the 2026 Form 540-ES standard deduction, payment schedule, and Behavioral Health Services Tax; the 2025 Form 540 tax schedule; the 2025 Form 540 2EZ booklet's $153 per-person exemption credit; and the 2025 Schedule D (540) instructions' $3,000 MFJ capital-loss limit. The $306 joint personal exemption credit is derived as two $153 personal exemptions. Provenance identifies the source for each field rather than implying that all fields came from one table.

The joint personal exemption credit is a fixed amount in this MVP. California's AGI-based exemption-credit limitation is outside scope and is not represented as editable rules.

## Lifecycle

There is no user-visible draft or activation workflow.

- **Official baseline:** one immutable supplied revision per jurisdiction
- **Active revision:** values currently used by calculations, referenced once per jurisdiction
- **Archived revision:** a previous active version retained for history

The user edits the active settings form and selects Save. A valid save atomically creates new federal and California revisions and moves both active references. An invalid save leaves both active rules unchanged. Restoring either an official baseline or archived revision copies its values into a new active revision; it never reactivates or mutates the source revision. A rule set is customized when its calculation values differ from its jurisdiction's official baseline, regardless of its revision origin.

## Validation

Before saving an active revision, verify:

- Required values are present
- Amounts and rates have sensible bounds
- Brackets are ordered, contiguous, and non-overlapping
- Every bracket starts where the prior bracket ends
- The first bracket starts at zero
- The final bracket is open-ended
- Dates are valid and ordered
- Cumulative installment percentages never decrease
- Final cumulative installment percentage equals 100%
- Each period percentage equals the change from the prior cumulative percentage
- Federal and California values are not mixed
- All parameters required by supported calculations exist

Validation establishes structural consistency, not legal correctness.

## Save effects

Saving valid changes:

- Makes them immediately active
- Recalculates the current result
- Archives the previous revision
- Does not change saved calculation snapshots
- Marks the result as customized when it differs from official defaults

## Restore behavior

The user may restore:

- Official defaults
- A prior archived revision

Restore creates a new active revision. It does not delete subsequent history or rewrite old snapshots.

## Provenance

Every official rule set records:

- Source agency
- Publication title and link
- Publication date
- Verification date
- Interpretation notes when needed

Custom revisions retain their origin and are marked as modified.

## Fixed behavior

Settings cannot modify:

- Income classification
- Gain/loss netting order
- Preferential-income stacking
- Deduction ordering
- Federal or California calculation sequence
- Projection behavior
- Rounding behavior
- Quarterly catch-up behavior

There is no formula editor.

## Update boundary

The application does not automatically search, download, scrape, or activate tax-law changes. Updates occur only through official application revisions or explicit local edits.
