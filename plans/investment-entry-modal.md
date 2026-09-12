# Investment entry modal

## Scope

Replace the quarter investment direct-entry grid with editable aggregate totals and an Add modal. This is a frontend interaction change only: the existing whole-quarter API and aggregate SQLite fields remain unchanged.

## Behavior

The modal selects one of ordinary dividends, qualified dividends, short-term gain/loss, long-term gain/loss, Federal withholding, or California withholding and accepts a nonzero signed cent amount.

- Additions update the selected aggregate only.
- Negative amounts reduce any aggregate, but ordinary/qualified dividends and withholding cannot become negative.
- Qualified dividends cannot exceed ordinary dividends after an addition.
- Short- and long-term gains/losses remain signed.
- The modal resets on Cancel, Add, and reopen; individual entries are not retained.

The section continues to display editable aggregate values, notes, warnings, backend errors, and whole-section removal.

## Verification

Test modal validation and reset behavior, signed aggregate updates, qualified-dividend boundaries, immutable section updates preserving unrelated values, and frontend typecheck, tests, lint, production build, and diff check.
