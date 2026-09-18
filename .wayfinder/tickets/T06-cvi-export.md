---
id: T06
title: Export CVI's fitted surface and diagnostics to the shared contract
type: Task (AFK)
status: resolved
blocked_by: [T04]
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

What is the smallest change to the CVI project that makes it emit its fitted surface and
diagnostics in the T04 output schema?

## Why it matters

Without this the benchmark has no baseline to compare against. The CVI fitter currently lives
inside `CVI Vol Surface Fitting.ipynb` with **no export calls** and no CLI entry point, so its
results cannot leave the notebook today.

## Scope

- Decide whether to extract the fitting code into an importable module or to add an export cell to
  the notebook, weighing that `CVI-Vol-Surface-Fitting` is a **public** repo and any change there is
  visible once pushed.
- Emit fitted values on both the quote set and the dense grid required by T04.
- Keep the change additive: do not alter CVI's fitting behaviour, or the baseline stops being the
  thing you already trust.

## Done when

Running CVI on `option_chain.csv` produces output files that validate against the T04 schema, and
the change is confined to an agreed location.

## Resolution

Closed 2026-09-13 (`bd8383d`); resolution recorded 2026-09-16 (D11, D16).

- **Location:** an additive export cell in the existing `CVI Vol Surface Fitting.ipynb`. The user
  authorised modifying, committing and pushing the public CVI repository. There is no module
  extraction, and the fitting cells are unchanged.
- **Output:** `benchmark_output/{fitted_quotes,surface_grid,parameters,diagnostics}.csv`, pinned to
  CVI commit `f7da1b3829e8bb2bc00426605e15aaad20ddad09` in `evidence/benchmark_manifest.json`.
- **Adapters on the ql-essvi side:**
  - `python/adapt_cvi_output.py` and `python/adapt_cvi_grid.py` map CVI output onto all normalized
    quote IDs and the shared grid.
  - `python/make_common_grid.py` resamples both models onto one common grid.
- **Residual gap (closed D22 / T12):** missing CVI strikes on a fitted tenor are filled by
  interpolating the existing CVI smile. Quotes on tenors CVI never fitted stay `not_fitted`.
