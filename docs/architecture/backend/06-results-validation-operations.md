# Backend 6: Results, Validation, and Local Operations

## Responsibility

Define the result hierarchy, explanation and snapshot behavior, validation severity, local data ownership, backup/restore, and diagnostics.

## Current result hierarchy

### Household overview

- Projected annual income
- Federal and California liability
- Federal and California withholding
- Estimated payments made
- Current recommendations
- Remaining projected obligations

### Jurisdiction summary

- Supported income
- Deduction and taxable income
- Core and additional tax
- Supported California exemption credits
- Annual liability
- Withholding
- Payment target
- Payments made
- Current recommendation

### Calculation detail

- Income categories included
- Actual versus projected wages and withholding
- Tax calculation stages
- Payment calculation stages
- Assumptions inherent in the projection
- Rule source and revision
- Relevant warnings

The displayed breakdown must reconcile with its headline result.

## Saved snapshots

Snapshots are created explicitly, not after every edit. A snapshot preserves its as-of date, inputs, projection, rules, federal and California results, recommendations, and warnings.

The user may save, view, rename, and delete snapshots. There is no snapshot comparison, branching, or recalculation workflow.

## Validation severity

### Blocking

Prevent an affected result from being presented as valid.

Examples:

- Missing or invalid active rule set
- Qualified dividends exceed ordinary dividends
- Contradictory paystub values
- Invalid jurisdiction or quarter
- Unreadable or internally inconsistent stored data

Federal and California may fail independently when possible.

### Caution

Allow calculation but disclose a material limitation.

Examples:

- Paystub appears stale
- Investment quarter is missing
- Future investment income is assumed to be zero
- Unsupported tax situation is omitted
- AMT is not calculated
- Additional Medicare Tax is approximate
- NIIT omits unsupported investment expenses and adjustments
- Separately identified Additional Medicare Tax withholding is not collected
- California exemption-credit AGI limitation is omitted
- Enhanced federal senior deduction is omitted
- Custom rules are active
- A due date has passed

### Informational

Explain normal behavior, such as California's zero-percent Q3 installment or a recommendation containing catch-up.

## Source-of-truth categories

- **User facts:** paystubs, quarterly investments, and payments
- **Derived results:** projections, liabilities, and recommendations
- **Immutable history:** saved snapshots and archived rule revisions

Derived results are recalculated and never directly edited. The application orchestration layer obtains one local-calendar as-of date from its injectable clock at the beginning of an operation, then passes it explicitly through every date-sensitive validation and calculation stage. Domain calculations do not read the system clock. Tests use a fixed clock; production uses the host operating system's configured local timezone. Snapshot reads use the immutable captured as-of date and do not recalculate.

## Local data and privacy

All data remains local. Normal operation uses no network services.

The application has no login, roles, remote access, telemetry, or built-in database encryption. It relies on operating-system account and disk protection.

## Saving and correction

Valid changes are saved completely or not at all. A failed save leaves the prior valid state and calculation intact.

Correcting any quarter or active tax rule recalculates the current result. It does not rewrite recorded payments, saved snapshots, or archived rule revisions.

## Backup and restore

Backup creates a complete SQLite dump of application state.

Restore:

1. Validates that the dump is readable and compatible.
2. Confirms replacement of current data.
3. Reconstructs the complete local database.
4. Leaves current data unchanged if validation or restore fails.
5. Revalidates active rules and recalculates the current result.

Restore is full replacement only. It does not merge databases or restore selected records.

## Logging

Local logging supports `error`, `info`, and `debug` levels.

Configuration comes from a configuration file or command-line options, with command-line values taking precedence. Output may go to console or a configured file.

Logs may describe lifecycle events, validation outcomes, rule revisions, calculation stages, and backup/restore stages. They should avoid complete paystub values, investment values, results, notes, or backup contents.

There is no frontend logging UI, runtime log management, remote transmission, rotation, retention, cleanup, search, or export. Rotation and retention are external responsibilities.

## Backend/frontend boundary

The backend owns business validation, persistence, projection, calculation, recommendations, snapshots, rules, backup/restore, and warnings.

The frontend submits business inputs and presents backend-provided results. It must not independently reproduce tax calculations.

## Startup behavior

On startup:

1. Open and validate local data.
2. Confirm valid active federal and California rules.
3. Load the 2026 Tax Year Case.
4. Recalculate the current projection and recommendations.
5. Report blocking errors and relevant warnings.

Startup does not contact external services.
