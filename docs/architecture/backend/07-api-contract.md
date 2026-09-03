# Backend 7: Frontend/Backend JSON API

## Responsibility

Define the coarse-grained local API between the Nuxt frontend and C++ backend.

The API follows the quarter-centered product model. It exposes whole business resources rather than individual getters and setters for every field. The backend listens only on loopback, defaulting to `127.0.0.1:8080`; a command-line port override may change the port but never the loopback bind address.

Implementation uses a small HTTP/1.1 loopback listener without an additional HTTP dependency. It accepts `Content-Length` and chunked request framing, closes each connection after its response, and intentionally omits general-purpose server features not needed by this local API.

## Principles

- JSON is the default request and response format.
- A quarter is read and saved as one aggregate document.
- `PUT` completely replaces the editable state of a known resource.
- `POST` creates a resource or runs an explicit command.
- `DELETE` removes a resource.
- `PATCH` is not used.
- Derived projections, tax results, and recommendations are response-only.
- Saves are validated and applied atomically.
- Save responses contain refreshed results so the frontend does not immediately need another call.
- The frontend does not reproduce backend tax calculations.
- Backup and restore are the only non-JSON payloads.

## JSON conventions

### Money

Money is represented as integer cents. Money fields use the `_cents` suffix.

```json
{
  "federal_withholding_ytd_cents": 1200000
}
```

This avoids binary floating-point ambiguity.

### Rates

Editable tax rates are represented as integer parts per million and use the `_ppm` suffix.

```json
{
  "rate_ppm": 220000
}
```

This example represents 22%.

### Dates

Calendar dates use ISO `YYYY-MM-DD` strings. The application orchestration layer resolves the current as-of date once per API operation through an injectable local-calendar clock. It passes that exact value through date-sensitive validation, derived calculations, persistence checks, and response composition; no domain component reads the system clock. The value is response-only: editable household, quarter, and rule requests must not supply `as_of_date`.

Current derived-result responses include the resolved `as_of_date`. Immutable snapshot responses return their captured `as_of_date` without recalculation.

### Missing values

- `null` means the user has not supplied a value or the object does not exist.
- `0` means the user explicitly supplied or confirmed zero.
- Missing optional notes have no effect.

### Identifiers

The household uses stable `spouse_1` and `spouse_2` keys. Display labels may change without changing these keys.

Snapshot and rule-revision identifiers are opaque strings supplied by the backend.

### Warnings

Warnings use a consistent shape:

```json
{
  "severity": "caution",
  "code": "bonus_withholding_projection",
  "path": "paystubs.spouse_1",
  "message": "The latest paystub contains a bonus; projected withholding may be less accurate."
}
```

Supported severities are `blocking`, `caution`, and `information`.

## Complete derived-result shape

Every response that includes refreshed current results uses this exact `current_result` object. All money fields are integer cents; all absent recommendation amounts are `null`.

```json
{
  "as_of_date": "2026-09-02",
  "projection": {
    "federal_wages": {"actual_cents": 0, "projected_remaining_cents": 0, "projected_annual_cents": 0},
    "california_wages": {"actual_cents": 0, "projected_remaining_cents": 0, "projected_annual_cents": 0},
    "federal_withholding": {"actual_cents": 0, "projected_remaining_cents": 0, "projected_annual_cents": 0},
    "california_withholding": {"actual_cents": 0, "projected_remaining_cents": 0, "projected_annual_cents": 0},
    "investments": {"ordinary_dividends_cents": 0, "qualified_dividends_cents": 0, "short_term_gain_cents": 0, "long_term_gain_cents": 0, "federal_withholding_cents": 0, "california_withholding_cents": 0},
    "spouses": [{"key": "spouse_1", "authoritative_quarter": null, "remaining_pay_periods": 0, "federal_wages": {}, "california_wages": {}, "federal_withholding": {}, "california_withholding": {}}],
    "warnings": []
  },
  "tax": {"federal": {}, "california": {}},
  "recommendations": {"federal": {}, "california": {}, "validation": []}
}
```

Each spouse amount object has the three fields shown in the top-level amount objects. Each tax jurisdiction object has exactly:

```json
{
  "details": {"supported_agi_cents": 0, "deduction_cents": 0, "taxable_income_cents": 0, "preferential_pool_cents": 0, "ordinary_taxable_income_cents": 0, "ordinary_tax_cents": 0, "preferential_tax_cents": 0, "ordinary_tax_on_all_income_cents": 0, "regular_tax_cents": 0, "exemption_credit_cents": 0, "tax_before_exemption_cents": 0, "tax_after_exemption_cents": 0, "niit_cents": 0, "additional_medicare_cents": 0, "behavioral_health_services_cents": 0, "annual_liability_cents": 0, "projected_withholding_cents": 0, "amount_requiring_estimated_payments_cents": 0},
  "capital_netting": {"ordinary_gain_cents": 0, "preferential_gain_cents": 0, "deductible_loss_cents": 0, "unused_loss_cents": 0},
  "effective_rate_ppm": 0,
  "rule_revision_id": "1",
  "warnings": []
}
```

Each recommendation jurisdiction object has exactly:

```json
{
  "calculation_status": "available",
  "recommendation_status": "payment_recommended",
  "due_date_status": "upcoming",
  "projected_overpayment_cents": 0,
  "projected_overpayment": false,
  "annual_target_cents": 0,
  "cumulative_target_cents": 0,
  "payments_credited_cents": 0,
  "scheduled_installment_cents": 0,
  "scheduled_recommendation_cents": 0,
  "catch_up_recommendation_cents": 0,
  "recommended_payment_cents": 0,
  "remaining_before_recommendation_cents": 0,
  "remaining_after_recommendation_cents": 0,
  "current_quarter": 1,
  "due_date": "2026-04-15",
  "future_outlook": [{"quarter": 2, "cumulative_target_cents": 0, "recommended_payment_cents": 0}],
  "rule_revision_id": "1"
}
```

`recommendation_status` and all recommendation amount fields are `null` when `calculation_status` is `insufficient_information`. Validation outcomes use `{ "severity": "blocking|caution|informational", "jurisdiction": "federal|california|null", "code": "...", "message": "..." }`. Projection and tax warnings use the warning shape already documented.

Rule revisions use `{ "revision_id": "1", "jurisdiction": "federal|california", "official_baseline": false, "modified": false, "sources": [{"agency": "...", "title": "...", "url": "...", "publication_date": "...", "verification_date": "...", "interpretation_note": "..."}], "rules": { ... } }`, where `rules` is the applicable complete editable federal or California rule object. Brackets use `{ "lower_bound_cents": 0, "upper_bound_cents": null, "rate_ppm": 0 }`; installments use `{ "quarter": 1, "due_date": "2026-04-15", "period_ppm": 0, "cumulative_ppm": 0 }`.

Snapshot summaries are `{ "id": "1", "label": "Q3", "as_of_date": "2026-09-02" }`. A complete snapshot is `{ "id": "1", "label": "Q3", "as_of_date": "2026-09-02", "household": { ... }, "quarters": [{ ... }], "current_result": { ... }, "rules": { "federal": { ... }, "california": { ... } } }`, where the nested values use the exact reusable shapes in this document and are immutable.

## Endpoint summary

| Method | Path | Responsibility |
| --- | --- | --- |
| `GET` | `/api/2026` | Home and application bootstrap data |
| `GET` | `/api/2026/quarters/{quarter}` | Complete Quarter page resource |
| `PUT` | `/api/2026/quarters/{quarter}` | Replace and recalculate a quarter |
| `GET` | `/api/2026/household` | Household settings |
| `PUT` | `/api/2026/household` | Replace household settings |
| `GET` | `/api/2026/tax-rules` | Active rules, sources, and revision summaries |
| `PUT` | `/api/2026/tax-rules` | Replace active federal and California rule values |
| `POST` | `/api/2026/tax-rules/restore` | Restore official or archived rules |
| `GET` | `/api/2026/snapshots` | Snapshot summaries |
| `POST` | `/api/2026/snapshots` | Save the current calculation |
| `GET` | `/api/2026/snapshots/{id}` | One saved snapshot |
| `PUT` | `/api/2026/snapshots/{id}/metadata` | Replace editable snapshot metadata |
| `DELETE` | `/api/2026/snapshots/{id}` | Delete one snapshot |
| `GET` | `/api/backup` | Download a complete SQLite dump |
| `POST` | `/api/restore` | Replace local data from a SQLite dump |

There are no field-level endpoints such as `setDividend`, `setWithholding`, or `setFederalPayment`.

## Application bootstrap

### `GET /api/2026`

Returns everything needed to render Home and the global quarter navigation.

```json
{
  "tax_year": 2026,
  "as_of_date": "2026-09-02",
  "current_quarter": 3,
  "household": {
    "spouses": [
      { "key": "spouse_1", "label": "Spouse 1" },
      { "key": "spouse_2", "label": "Spouse 2" }
    ]
  },
  "actuals": {
    "federal_wages_ytd_cents": 12000000,
    "california_wages_ytd_cents": 12100000,
    "ordinary_dividends_cents": 400000,
    "qualified_dividends_cents": 350000,
    "short_term_gain_cents": -120000,
    "long_term_gain_cents": 1200000,
    "federal_withholding_ytd_cents": 2100000,
    "california_withholding_ytd_cents": 1050000,
    "federal_estimated_payments_cents": 1000000,
    "california_estimated_payments_cents": 700000
  },
  "projection": {
    "federal_wages_cents": 18000000,
    "california_wages_cents": 18150000,
    "federal_withholding_cents": 3150000,
    "california_withholding_cents": 1575000
  },
  "tax": {
    "federal": {
      "annual_liability_cents": 4800000,
      "remaining_obligation_cents": 650000,
      "current_recommendation_cents": 325000
    },
    "california": {
      "annual_liability_cents": 2100000,
      "remaining_obligation_cents": 175000,
      "current_recommendation_cents": 0
    }
  },
  "quarters": [
    { "quarter": 1, "status": "complete" },
    { "quarter": 2, "status": "complete" },
    { "quarter": 3, "status": "in_progress" },
    { "quarter": 4, "status": "not_started" }
  ],
  "warnings": []
}
```

The example values illustrate shape only; they are not test fixtures or official calculations.

## Quarter resource

### `GET /api/2026/quarters/{quarter}`

Returns the complete selected-quarter input plus the derived information needed by the Quarter page.

Valid quarter path values are `1`, `2`, `3`, and `4`.

### `PUT /api/2026/quarters/{quarter}`

Replaces all editable inputs for the selected quarter.

```json
{
  "paystubs": {
    "spouse_1": {
      "date": "2026-09-01",
      "pay_frequency": "biweekly",
      "current_period_regular_wages_cents": 420000,
      "current_period_bonus_wages_cents": 0,
      "current_period_federal_withholding_cents": 70000,
      "current_period_california_withholding_cents": 35000,
      "federal_taxable_wages_ytd_cents": 7200000,
      "california_taxable_wages_ytd_cents": 7300000,
      "federal_withholding_ytd_cents": 1200000,
      "california_withholding_ytd_cents": 620000,
      "social_security_withholding_ytd_cents": 446400,
      "medicare_withholding_ytd_cents": 104400,
      "california_sdi_withholding_ytd_cents": 73000
    },
    "spouse_2": null
  },
  "investments": {
    "ordinary_dividends_cents": 400000,
    "qualified_dividends_cents": 350000,
    "short_term_gain_cents": -120000,
    "long_term_gain_cents": 1200000,
    "federal_withholding_cents": 0,
    "california_withholding_cents": 0,
    "notes": null
  },
  "payments": {
    "federal": {
      "amount_cents": 500000,
      "date": "2026-09-01"
    },
    "california": {
      "amount_cents": 0,
      "date": null
    }
  }
}
```

The response uses the same shape for `GET` and successful `PUT`; `result.as_of_date` is the single resolved date used throughout the operation:

```json
{
  "tax_year": 2026,
  "quarter": 3,
  "input": {},
  "result": {
    "as_of_date": "2026-09-02",
    "annual_summary": {},
    "federal": {},
    "california": {},
    "current_recommendations": {}
  },
  "warnings": []
}
```

`input` contains the backend's canonical saved representation. A successful `PUT` response includes the refreshed annual summary and recommendations so no immediate Home request is required.

Repeatedly sending the same valid `PUT` document produces the same saved quarter state.

Supported `pay_frequency` values are `weekly`, `biweekly`, `semimonthly`, and `monthly`. The ordinary Medicare field is the total shown on the paystub and is display-only; it is not treated as federal income-tax withholding or as separately identified Additional Medicare Tax withholding.

Payment records describe completed payments. A non-null payment date must not be after the calculation as-of date. Use a zero amount and `null` date when no payment was made.

A future calendar quarter accepts only its empty resource shape. Paystub, investment, or payment facts cannot be saved there until that quarter begins.

## Household resource

### `GET /api/2026/household`

Returns:

```json
{
  "tax_year": 2026,
  "filing_status": "married_filing_jointly",
  "residency": "california_full_year",
  "spouses": [
    {
      "key": "spouse_1",
      "label": "Spouse 1",
      "age_65_or_older": false,
      "blind": false
    },
    {
      "key": "spouse_2",
      "label": "Spouse 2",
      "age_65_or_older": false,
      "blind": false
    }
  ]
}
```

### `PUT /api/2026/household`

Replaces the complete household settings document. Tax year, filing status, and residency must remain the fixed MVP values.

A successful response returns the canonical saved household plus the refreshed annual summary and recommendations.

## Tax-rule resource

### `GET /api/2026/tax-rules`

Returns both active jurisdictions in one call:

```json
{
  "federal": {
    "revision_id": "opaque-revision-id",
    "official": true,
    "source": {},
    "standard_deduction_cents": 3220000,
    "age_or_blind_addition_cents": 165000,
    "ordinary_brackets": [],
    "preferential_brackets": [],
    "capital_loss_limit_cents": 300000,
    "additional_taxes": {
      "niit": {
        "threshold_cents": 25000000,
        "rate_ppm": 38000
      },
      "additional_medicare": {
        "threshold_cents": 25000000,
        "rate_ppm": 9000
      }
    },
    "installments": [],
    "archived_revisions": []
  },
  "california": {
    "revision_id": "opaque-revision-id",
    "official": true,
    "source": {},
    "standard_deduction_cents": 1141200,
    "ordinary_brackets": [],
    "joint_personal_exemption_credit_cents": 0,
    "capital_loss_limit_cents": 300000,
    "additional_taxes": {
      "behavioral_health_services": {
        "threshold_cents": 100000000,
        "rate_ppm": 10000
      }
    },
    "installments": [],
    "archived_revisions": []
  }
}
```

### `PUT /api/2026/tax-rules`

Replaces both active rule documents atomically. The body uses the same editable rule shape returned by `GET`; backend-owned revision metadata is ignored or rejected rather than accepted as user data.

The backend validates the complete federal and California rule sets before saving either. A successful response returns the canonical active revisions and refreshed annual results.

The zero California exemption-credit value in the example is a shape placeholder, not an official default. As with the other example values, authoritative seeded values come from the sourced rule revision.

Bracket and installment arrays use these shapes:

```json
{
  "ordinary_brackets": [
    {
      "lower_bound_cents": 0,
      "upper_bound_cents": 2480000,
      "rate_ppm": 100000
    },
    {
      "lower_bound_cents": 2480000,
      "upper_bound_cents": null,
      "rate_ppm": 120000
    }
  ],
  "installments": [
    {
      "quarter": 1,
      "due_date": "2026-04-15",
      "period_ppm": 250000,
      "cumulative_ppm": 250000
    }
  ]
}
```

The abbreviated values illustrate representation only. A saved rule set must contain complete contiguous brackets and all four installments.

### `POST /api/2026/tax-rules/restore`

Restore official values:

```json
{
  "jurisdiction": "federal",
  "source": "official"
}
```

Restore an archived revision:

```json
{
  "jurisdiction": "california",
  "source": "revision",
  "revision_id": "opaque-revision-id"
}
```

Restore creates a new active revision; it does not rewrite history.

## Snapshot resources

### `GET /api/2026/snapshots`

Returns snapshot summaries ordered newest first.

### `POST /api/2026/snapshots`

Saves the current calculation:

```json
{
  "label": "After Q3 update"
}
```

Returns `201 Created` with the saved snapshot summary.

### `GET /api/2026/snapshots/{id}`

Returns the immutable saved inputs, rules, results, recommendations, warnings, and captured `as_of_date`; it does not recalculate the snapshot.

### `PUT /api/2026/snapshots/{id}/metadata`

Replaces the editable metadata:

```json
{
  "label": "Q3 final"
}
```

The saved calculation content cannot be modified.

### `DELETE /api/2026/snapshots/{id}`

Deletes the selected snapshot and returns `204 No Content`.

## Backup and restore

### `GET /api/backup`

Returns a complete SQLite dump as a download. The response is binary rather than JSON.

### Request body limits

Normal JSON request bodies are limited to 1 MiB (1,048,576 bytes). SQLite restore uploads are separately limited to 64 MiB (67,108,864 bytes). The server enforces the applicable limit against raw incoming bytes before JSON parsing or restore staging: it rejects an excessive `Content-Length` immediately and stops reading a streamed or length-unknown body at the limit. Every oversized request receives the documented `413` error envelope.

### `POST /api/restore`

Accepts one complete SQLite dump as the request body. The backend applies the separate 64 MiB (67,108,864 bytes) restore limit before staging, then validates it before replacing current data.

A successful response is JSON containing the restored Home/bootstrap resource. Validation or restore failure leaves current data unchanged.

## Errors

Errors use one envelope:

```json
{
  "error": {
    "code": "validation_failed",
    "message": "The quarter contains invalid values.",
    "fields": [
      {
        "path": "investments.qualified_dividends_cents",
        "code": "exceeds_ordinary_dividends",
        "message": "Qualified dividends cannot exceed ordinary dividends."
      }
    ]
  }
}
```

Use these status codes:

| Status | Meaning |
| --- | --- |
| `200` | Successful read, replacement, or command |
| `201` | New snapshot created |
| `204` | Snapshot deleted |
| `400` | Malformed JSON or unsupported request shape |
| `404` | Resource not found |
| `413` | Restore payload is too large |
| `415` | Unsupported content type |
| `422` | Structurally valid request with domain-validation errors |
| `500` | Unexpected backend or storage failure |

Unknown input fields should be rejected with `400` so frontend/backend contract drift is visible during development.

## Atomicity

- A failed `PUT` changes nothing.
- A failed tax-rule save changes neither jurisdiction.
- A failed restore preserves current data.
- A successful quarter save persists inputs before returning recalculated results.
- Snapshot creation captures one internally consistent current result.

## Deliberate exclusions

- Field-level getters and setters
- GraphQL
- WebSockets or server-sent events
- Batch endpoints spanning unrelated resources
- Partial updates
- Remote authentication or authorization
- API-driven log management
- Multiple API or tax-year versions in the MVP
