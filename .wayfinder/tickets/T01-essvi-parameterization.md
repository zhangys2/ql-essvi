---
id: T01
title: Fix the eSSVI parameterization and its no-arbitrage conditions
type: Research (AFK)
status: resolved
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

Which exact eSSVI parameterization will this project implement, and what are the precise
sufficient conditions on its parameters for the surface to be free of butterfly and calendar-spread
arbitrage?

## Why it matters

Everything downstream hangs on this: the calibration variables, the constraints the optimizer must
respect, and what the benchmark's arbitrage scorer should find (ideally nothing). Getting the
constraint set wrong makes a "win" on fit quality meaningless.

## Scope

- Pin the surface form: total variance `w(k, theta_t)` in terms of ATM total variance `theta_t`,
  `rho(theta_t)` and `phi(theta_t)`; state the chosen `phi` family (power law vs alternatives).
- State the constant-rho reduction that recovers plain SSVI, per D5.
- Gather the sufficient no-arbitrage conditions from the literature and note where variants differ
  (Gatheral–Jacquier 2014 SSVI; Hendriks–Martini and Corbetta et al. for eSSVI).
- Record which conditions are enforced *during* calibration versus merely *checked* afterwards.

## Done when

The parameterization is written down unambiguously (symbols, domains, the reduction to SSVI) with
its no-arbitrage conditions as explicit inequalities, each traced to a named source.
