# Frontend Architecture

## Responsibility

Provide a simple quarter-centered Nuxt interface for entering data, reviewing annual status, and understanding the current federal and California amounts owed.

Tax calculation behavior remains in the backend.

## Design direction

The interface uses the Nuxt UI Dashboard as its structural baseline with a restrained dark financial-dashboard theme. Detailed visual, component, reuse, and future chart rules are defined in [Frontend design language](frontend-design-language.md).

Charts remain outside the MVP. When added, they use straight line segments, visible data-point markers, explicit actual/projected treatment, and no smoothing.

## Navigation

The application has four primary areas:

1. Home
2. Quarters
3. History
4. Settings

Q1-Q4 are directly accessible within Quarters, and the open case's backend-provided current quarter opens by default. A visible Tax year selector distinguishes the selected tax year from its Q1-Q4 navigation; it remains visible but disabled when only the 2026 case exists.

## Home

Home is read-only and answers: "Where do we currently stand for 2026?"

### Annual tax status

- Projected federal tax
- Projected California tax
- Remaining projected federal obligation
- Remaining projected California obligation
- Current-quarter federal recommendation
- Current-quarter California recommendation

### Actual earnings and investments so far

- **Taxable wages:** Federal and California values form one full-width comparable pair.
- **Investment activity:** ordinary dividends, qualified dividends, and net short- and long-term gains/losses form a four-column wide-screen grid.
- A divider separates wages from investments; the investment grid collapses to two columns and then one on narrower screens.

### Taxes paid so far

- **Tax withholding:** Federal and California values.
- **Estimated payments:** Federal and California values.

Withholding and estimated payments remain visually distinct; group headings carry the shared context, so child labels only name the jurisdiction.

### Annual projection

- **Projected wages:** Federal and California values.
- **Projected withholding:** Federal and California values.
- **Projected tax position:** annual liability and remaining obligation groups, each with Federal and California values.

A divider separates the projected tax position from projected wages and withholding.

### Quarter status

A concise Q1-Q4 summary shows data-entry completeness, payments recorded, and payment status. Selecting a quarter opens it.

Trend charts and trend-based extrapolation are deferred beyond the MVP.

## Quarter page

Current and historic quarters use the same page structure.

### 1. Quarter header

- Quarter and date range
- Current or historic status
- Federal and California due dates
- Data completeness
- Save status

### 2. Estimated taxes owed

A shared **Estimated taxes owed** heading precedes side-by-side Federal and California cards. Each card has:

- Jurisdiction heading
- Paid, partially paid, due, catch-up, or no-payment-needed status
- Aligned amount currently owed row
- Cumulative target and payments recorded
- Due date and due-date status

A divider separates the current amount from the supporting two-column details grid. This is the primary result on the current-quarter page.

### 3. Spouse paystub inputs

One repeated section for each spouse:

- Paystub date
- Pay frequency
- **Current pay period:** regular wages, bonus wages, Federal withholding, and California withholding
- **Year to date:** Federal and California taxable wages, Federal and California withholding, Social Security withholding, Medicare withholding, and California SDI withholding

The saved spouse label appears in each paystub heading. Section headings supply time context, so individual field labels do not repeat “Current pay period” or “YTD”; labels use title case and preserve acronyms. There is no job setup or job selection.

### 4. Quarterly investments

- Ordinary dividends
- Qualified dividends
- Net short-term gain/loss
- Net long-term gain/loss
- Supported investment withholding
- Optional notes

The section must clearly state that values are for the selected quarter, not YTD.

### 5. Estimated payments

Separate federal and California entries:

- Amount paid during the quarter
- Payment date

The section also shows the jurisdiction's scheduled percentage and due date.

### 6. Updated annual position

- **Income & investments:** projected wages and recorded dividend income
- **Covered taxes:** projected withholding and estimated payments recorded
- **Projected tax position:** Federal and California annual liability and remaining obligation

Sections use low-contrast dividers and grouped Federal/California metric grids. The section heading supplies actual/projected/paid context, so amount suffixes do not repeat it.

### 7. Warnings

Warnings appear near the affected section. Broad calculation limitations appear with the annual summary.

## Historic quarters

Historic quarters in the open case use the same layout and remain editable. Editing one recalculates the open case's current annual result and current-quarter recommendation.

A historic quarter does not recreate its old recommendation. Closed tax-year cases are read-only and display their protected closure snapshot rather than recalculating.

## Calculation details

Federal and California details remain separate. Each explanation proceeds from income through deduction, taxable income, core tax, supported additional tax, withholding, payment target, prior payments, and current recommendation.

The detail should explain the estimate without mimicking tax forms.

## History

History lists saved snapshots by date, label, as-of date, and federal/California recommendations.

The user may save, view, rename, and delete snapshots. There is no comparison, filtering, branching, or scenario workflow.

## Settings

### Household

- Spouse labels
- Supported age-related information
- Read-only 2026, married-filing-jointly, California assumptions

### Tax rules

- Separate federal and California settings
- Active values and official source
- Direct edit and save
- Restore official defaults
- View and restore prior revisions

Saving valid values makes them active immediately. There is no draft or activation flow.

### Data

- Create complete SQLite dump
- Restore complete SQLite dump

Logging has no frontend presence.

## Shared interaction rules

- Actual, projected, paid, and recommended amounts are explicitly labeled.
- Federal and California remain visually distinct and consistently named.
- Q1-Q4 use identical input structure and terminology.
- Money formatting and negative-value treatment are consistent.
- Color is not the only way to convey status.
- Saving a quarter keeps the user on that quarter and refreshes its result.
- Unsaved changes require confirmation before leaving.
- Restore operations require confirmation.
- Home and every quarter remain directly accessible without deep navigation.

## Reuse boundary

The frontend should share the same structural patterns for:

- Federal and California summary panels
- Spouse paystub sections
- Quarter navigation and status
- Money entry and display
- Actual-versus-projected labeling
- Warnings and validation messages
- Calculation breakdowns
- Tax-rule editing

Q1-Q4 must not have separate implementations.

## MVP exclusions

- Separate job, income, investment, or payment pages
- Job setup and future-income planning
- Scenario analysis
- Multiple households, concurrent open years, states, or filing statuses
- Trend charts
- Configurable dashboard widgets
- Tax forms
- Payment transmission
- File uploads or payroll/brokerage imports
- Search, notifications, accounts, or collaboration
- Snapshot comparison
- Logging controls

## Primary workflow

1. Open Home and review the annual position.
2. Open the current quarter.
3. Enter the latest paystub for each spouse.
4. Enter quarterly dividends and gains/losses.
5. Record federal and California payments.
6. Save.
7. Review the recalculated amounts owed at the top of the quarter page.
8. Return to Home for the updated annual view.
