---
id: T04
title: Define the shared CSV contract between the two fitters
type: Grilling (HITL)
status: resolved
blocked_by: [T07]
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

What exactly are the input and output CSV schemas that both the C++ eSSVI fitter and the Python CVI
fitter must read and write, so a single scorer can compare them?

## Why it matters

D4 made this contract the entire basis of the comparison. If the two fitters consume even slightly
different inputs — a different forward, a different filter, a different IV inversion — the benchmark
measures preprocessing differences rather than the models.

## Scope

- Input schema: per quote, at minimum expiry, time to expiry, forward, discount factor, strike,
  option type, bid IV, ask IV, mid IV, and an inclusion flag from filtering.
- Output schema: fitted IV or total variance per quote, plus fitted values on a dense grid for
  arbitrage checking beyond the listed strikes.
- Diagnostics each fitter must emit: per-slice parameters, convergence status, timing (indicative
  only per D4).
- Numeric conventions: units (vol points vs variance), precision, date format, and how missing or
  filtered quotes are represented.

## Done when

Both schemas are written down with column names, types and units, and a tiny hand-made example file
exists for each so T06 has an unambiguous target.

## Resolution

Closed 2026-09-13 (`bd8383d`); resolution recorded 2026-09-16. The canonical schemas are in
[`docs/csv-contract.md`](../../docs/csv-contract.md) and are enforced by
`python/validate_schema.py` (D15).

- **Files:** input `normalized_quotes.csv`; outputs `fitted_quotes.csv`, `surface_grid.csv`,
  `parameters.csv`, `diagnostics.csv`. Long-form, one row per quote or grid point.
- **Conventions:** UTF-8, `YYYY-MM-DD` dates, year-fraction times, decimal IVs (not vol points).
  Total variance is canonical and IV is a reporting field (D14). At least 12 significant digits;
  no `NaN`/`inf` tokens.
- **Missing values:** empty numeric fields, explained by `included=false` plus `filter_reason` on
  inputs and by `status` on fitted rows.
- **Dense grid:** `k = -0.5..0.5` in steps of 0.01 per expiry. `is_tail` marks points outside the
  observed quote range, and all points enter the arbitrage checks.
- **Diagnostics:** one row per fit. A failure gives `converged=false` with a non-empty `error`.
  Runtime is indicative only (D4).
- **CVI divergence:** the CVI notebook's `parameters.csv` has its own schema (ATM total variance and
  observed range), because CVI has no eSSVI parameters.

- **Examples:** every schema has an example file, `schemas/<file>.example.csv`, and each one must
  pass `validate_schema.py` (`tests/test_pipeline.py`). Added 2026-09-16; before that only an
  invalid input example existed.
