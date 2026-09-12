---
id: T07
title: Decide the shared preprocessing and scoring pipeline
type: Research (AFK)
status: open
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
