# Backend 8: Multi-Year Tax Cases

## Responsibility

Define the future multi-year model. This document is architecture only; the implemented MVP remains the 2026 case until a separately approved implementation plan is completed.

## Tax-year cases

A tax-year case owns its household copy, quarterly inputs, federal and California rules and revisions, snapshots, and derived results. The database retains every app-created closed case and has exactly one open case.

A case records:

- Tax year
- `open` or `closed` state
- `closed_as_of_date` when closed
- `closure_snapshot_id` when closed

The case's closure snapshot is an ordinary complete snapshot with a protected role. It captures exact inputs, derived results, warnings, and rule revisions. It cannot be renamed or deleted.

## Open and closed behavior

The open case is the only editable case. It resolves one local-calendar as-of date per operation and passes it explicitly to clock-independent calculation logic.

A closed case is immutable and read-only. It returns its closure snapshot's captured result and never recalculates or reads the host clock. All mutations to a closed case—including quarter, household, rule, snapshot creation, rename, and deletion—return `409 Conflict` with a clear closed-case error. Complete database backup and restore remain global operations.

## Atomic rollover

The application exposes one rollover operation, labeled in the UI as `Start {year} tax year`. It may create only the consecutive year after the open case. In one transaction it:

1. Confirms complete, validated official federal and California baselines are installed for the next exact year.
2. Resolves one as-of date, calculates the open case, and saves its protected closure snapshot.
3. Marks the open case closed with its as-of date and closure snapshot identifier.
4. Creates the next case with a copied household.
5. Installs that year's official baselines and marks the new case open.

Any failure rolls back the entire rollover. Official baselines arrive through an application release or verified rule-data update; rollover never accepts user-entered official rule data.

## Rule and data boundary

Every case requires complete federal and California rules for its exact tax year. Calculating a historical case using another year's rules is prohibited. No case can be created without its exact-year official baselines.

## API and persistence direction

Year-owned endpoints use `/api/{year}` while preserving `/api/2026` as the canonical 2026 URL. `GET /api/years` lists available cases and their state. Global complete-database backup and restore remain `/api/backup` and `/api/restore`.

SQLite migrations transform the existing singleton 2026 data into a 2026 case without creating a synthetic prior year. They scope case-owned tables by `case_id`, preserve existing data and snapshots, and validate every case, its active rules, and its closure reference during restore.
