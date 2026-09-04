# Frontend implementation plan

## F1 — Frontend foundation

Build infrastructure and only the shared components exercised by a representative shell fixture. F2 must not begin until every configured F1 check passes.

### Foundation

- Nuxt UI Dashboard shell with a restrained dark semantic theme.
- Responsive navigation for Home, Quarters, History, and Settings.
- Routes: `/`, `/quarters/[quarter]`, `/history`, and `/settings`.
- Validate quarter route parameters. The Quarters navigation entry resolves to the current bootstrap quarter, or Q1 while bootstrap state is unavailable.
- Lightweight route placeholders use the same content layout and state presentations as later pages.
- The backend URL is runtime-configurable. Development uses a `/api` proxy only; production code does not depend on that proxy.

### Typed backend integration

- TypeScript request and response models match the documented API contract, including integer cents, nullable values, dates, warnings, validation errors, and recommendation states.
- One API client owns JSON transport, HTTP status handling, error-envelope normalization, and malformed-response handling.
- One bootstrap composable owns loading, loaded, and failed application state. The shell presents that state; pages consume it without duplicating state or tax logic.
- No state-management dependency unless the composable approach later proves insufficient.

### Shared financial presentation

Implement only components demonstrated by the shell or a small representative Home/Quarter fixture:

- `MoneyInput` with cent-safe empty, zero, and permitted-negative handling.
- `MoneyDisplay` with explicit actual/projected/paid/recommended meaning.
- `MetricSummary` and `JurisdictionSummary`.
- `WarningList` and `QuarterStatus` with text/icons in addition to color.
- Common loading, empty, validation-error, and backend-error presentations.

Defer `CalculationBreakdown` and `SaveState` to F2 unless a real F1 use requires them. Use Nuxt UI directly for ordinary buttons, dialogs, selects, tables, and text fields; do not wrap a Nuxt UI component without shared financial behavior or domain meaning.

### Tests

Add minimal Nuxt-compatible Vitest component tests with deterministic API mocks. Cover:

- cents formatting and parsing boundaries, including empty versus explicit zero and permitted negatives;
- jurisdiction, status, and warning rendering without color-only meaning;
- successful API decoding and documented validation/backend-error normalization;
- transport and malformed-response errors;
- bootstrap loading, success, and failure states;
- primary navigation and quarter-route validation.

Run configured typecheck, tests, production build, and the existing lint command. Tests must not require a backend process or network.

F2 — Quarter workflow

This should be implemented before Home because it is the application’s primary workflow and exercises most of the shared patterns.

Page-level components
QuarterPage
One implementation parameterized by Q1–Q4
QuarterHeader
Quarter dates, current/historic state, due dates, completeness
QuarterRecommendationSummary
Federal and California amounts currently owed
PaystubSection
Reused once for each spouse
InvestmentSection
Quarter-only dividends and gains/losses
EstimatedPaymentsSection
Federal and California payments
AnnualPositionSummary
Updated annual projection after saving
QuarterWarnings
QuarterSaveBar
Implement in this order
Load and display a quarter.
Establish the complete quarter form model.
Implement the reusable spouse paystub section.
Add investments and estimated payments.
Add client-side input validation and backend validation display.
Save the whole quarter with one PUT.
Refresh recommendations from the save response.
Add dirty-state and navigation confirmation.
Verify the same implementation works for all four quarters.
Add historic-quarter editing behavior.

Do not calculate or infer taxes in the frontend.

F3 — Remaining pages
Home

Build first within F3 because it reuses the quarter result and summary components:

Annual tax status
Actual earnings and investments
Taxes paid
Annual projection
Q1–Q4 status list

No charts for MVP.

History
Snapshot list
Snapshot detail
Save snapshot dialog
Rename dialog
Delete confirmation

No snapshot comparison.

Settings
Household settings form
Federal tax-rule editor
California tax-rule editor
Rule revision list and restore confirmation
SQLite backup panel
SQLite restore panel and destructive confirmation
R1 — Frontend release readiness
Component and interaction tests
API integration tests
Responsive layout review
Keyboard and accessibility review
Loading/error/empty-state verification
Clean frontend build
Complete representative workflow against the real backend
Backup and restore workflow verification

