# Agent Guidelines

## Project purpose

Build a local application that estimates 2026 federal and California quarterly taxes for a married couple filing jointly. The application uses quarterly paystub, dividend, stock-gain/loss, withholding, and estimated-payment information.

This is a planning estimate, not tax-return preparation software. Prefer a clear, useful estimate over support for every tax situation.

## Working rules

- Inspect and plan freely, but ask for approval before modifying implementation files.
- Implement only approved scope. Do not add adjacent features speculatively.
- Prefer the smallest design that satisfies the documented workflow.
- When complexity grows, stop and reconsider the model before adding abstractions.
- Avoid unrelated refactors and unnecessary dependencies.
- Keep user-facing and agent-facing output concise.
- Update the relevant architecture document when an approved decision changes.

## Backend standards

- Use C++20.
- Keep implementations out of headers unless technically required.
- Avoid duplicated code.
- Prefer clean, narrow APIs and cohesive classes.
- Do not create classes whose only purpose is wrapping one function.
- Keep tax calculation behavior independent from presentation concerns.
- Treat unsupported tax situations as warnings instead of expanding scope automatically.
- Use the shared internal `estimated_taxes::sqlite` utility for SQLite connection ownership, prepared statements, typed binds, transactions, and safe column reads. Keep stores responsible for their schemas and domain mapping; do not introduce repositories or an ORM.
- Treat SQLite column values as untrusted when reading. Use the utility's required/optional text readers rather than constructing a C++ string or string view directly from `sqlite3_column_text()`; report inconsistent stored data as a storage error rather than dereferencing null.

## Frontend standards

- Use Nuxt.
- Prioritize reusable components and shared interaction patterns.
- Maintain a consistent design language across Home, Quarter, History, and Settings pages.
- Do not duplicate quarter-entry behavior across Q1-Q4.
- Keep tax logic in the backend; the frontend collects inputs and presents backend results.
- ECharts may be used for future trends, but charts are not part of the MVP.

## Testing standards

Treat tests as part of each milestone, not as a later cleanup phase.

- Add unit tests for new behavior in the same milestone that introduces it.
- Keep projection, tax, rounding, and payment-recommendation tests deterministic and cover documented boundaries, not only typical values.
- Use integration tests where behavior crosses SQLite transactions, JSON serialization, API endpoints, backup, or restore.
- Test reusable frontend components and important user interactions without reproducing backend tax logic in frontend tests.
- When fixing a defect, add a regression test when practical.
- Control the as-of date and use isolated temporary data so tests do not depend on the clock, network, or a user's database.
- Prefer behavior-focused tests over tests coupled to private implementation details or excessive snapshots.
- A milestone is incomplete until its relevant tests pass. Do not skip, weaken, or disable a failing test to complete a milestone.

## Product constraints

- Local-only application with no login, cloud synchronization, telemetry, or external integrations.
- SQLite is the application data store.
- Backup and restore use a complete SQLite dump and restore.
- Logs are local and controlled only by configuration or command-line options.
- Log rotation and retention are external concerns.

## Architecture authority

Read [docs/architecture/README.md](docs/architecture/README.md) and [docs/architecture/implementation-milestones.md](docs/architecture/implementation-milestones.md) before implementation. Work through the milestones in dependency order and stop at each completion gate for review. Follow the linked backend files as the authority for behavior. The documents describe approved scope; unresolved or future ideas are not implementation requirements.
