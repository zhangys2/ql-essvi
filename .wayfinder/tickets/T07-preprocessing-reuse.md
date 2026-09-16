---
id: T07
title: Decide the shared preprocessing and scoring pipeline
type: Research (AFK)
status: resolved
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

Which single pipeline turns `option_chain.csv` plus `sofrDFs.csv` into the calibration inputs both
fitters use, and which code scores the results?

## Why it matters

The raw chain has no implied vols, no forward and no discount factors, so every number in the
benchmark depends on this step. It also blocks T04, because the contract's input schema is exactly
this pipeline's output. Reusing one implementation for both fitters is what keeps D6's metric
comparable.

## Scope

- Reuse assessment of the notebook's existing functions — `EstimateForwards`, `filterData`,
  `black_model`, `CalculatePDF`, `NoButterflyArbitrageCDF`/`PDF`, `NoCalendarSpreadArbitrage`,
  `PositiveVariance`, `UpwardSlopingWings`, `RogerLeeMoment` — against reimplementing in C++.
- Whether the arbitrage scorer should be the notebook's checks (shared, therefore fair) or an
  independent implementation (cross-checks CVI itself, but risks scoring the two differently).
- Where the pipeline runs given T03's platform choice, and how its output reaches the C++ fitter.
- Bid/ask IV inversion details: discounting, forward estimation, and what to do where inversion
  fails (zero bids, deep wings).

## Done when

The pipeline's owner (Python-shared versus C++-reimplemented) is decided for both preprocessing and
scoring, with the reasons recorded, and its output fields are listed precisely enough for T04 to
formalise.

## Resolution

Closed 2026-09-13 (`bd8383d`); resolution recorded 2026-09-16 (D10).

- **Owner:** Python, shared by both fitters, for preprocessing (`python/prepare_inputs.py`) and for
  scoring (`python/score_outputs.py`). Nothing is reimplemented in C++. The notebook functions are
  not imported; the pipeline is a stdlib-only reimplementation.
- **Scorer:** independent of the notebook's arbitrage checks and applied identically to both
  models.
  - Butterfly: convexity of normalized call prices across strikes on the grid.
  - Calendar: total variance non-decreasing in expiry at fixed `k`.
- **Preprocessing:**
  - Discount factors: linear interpolation of `sofrDFs.csv` in `T`, flat beyond the ends.
  - Forward: mean over strikes of put–call parity `K + (C - P)/DF` on bid/ask mids.
  - Time: ACT/365.
- **Filters,** in order (`filter_reason`):
  - `invalid_or_crossed_quote`
  - `stale_quote` (last trade more than 7 days before the effective date)
  - `not_otm` (aligned with CVI's OTM selection, `f39d5bd`)
  - `iv_inversion_failed`
- **IV inversion:** Black bisection on `[1e-8, 5]` with 100 iterations, separately for bid and ask.
  It fails, and the quote is excluded, when the price is at or below intrinsic (`+1e-10`) or at or
  above the no-arbitrage upper bound. `mid_iv` is the mean of the bid and ask IVs.
- **Where it runs:** WSL (T03). The C++ fitter reads `normalized_quotes.csv` directly.

