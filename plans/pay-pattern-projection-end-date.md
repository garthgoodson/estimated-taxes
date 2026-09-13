# Pay-pattern projection end date

## Scope

Add an optional `projection_end_date` to the existing single consolidated paystub snapshot for each spouse. It limits repetition of that snapshot's regular wages and withholding; it does not create jobs, employers, multiple pay sources, or future-income entries.

## Implementation

1. Add the optional date to the domain model, SQLite migration 4, whole-quarter JSON codec, frontend type validation, and explicit clone path.
2. Validate a non-null ISO 2026 date only when a paystub exists; require it to be on or after the paystub date.
3. Compute remaining periods as completed periods at the selected horizon minus completed periods at the authoritative paystub date. Preserve current YTD, bonus, tax, and recommendation behavior.
4. Expose authoritative date, frequency, horizon, completed counts, and remaining periods in spouse projection detail. Emit an informational warning when the selected date reduces periods.
5. Extend snapshot encoding with a backward-compatible version and retain the complete SQLite backup/restore behavior.
6. Add the shared PaystubSection selector and tests for mode mapping, date changes, dirty state, validation display, spouse isolation, persistence, projection boundaries, snapshots, and backup/restore.

## Verification

Run backend build and ctest, frontend tests, lint, typecheck, production build, and `git diff --check`.
