---
id: T05
title: Choose the eSSVI calibration approach under no-arbitrage constraints
type: Research (AFK)
status: resolved
blocked_by: [T01]
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

How will eSSVI parameters be calibrated to the quote set while respecting the no-arbitrage
constraints from T01, and which optimizer performs that fit?

## Why it matters

This is the main technical risk in the whole effort. eSSVI calibration is non-convex in general,
and the no-arbitrage conditions are inequality constraints — precisely the structure that made CVI
a constrained QP. A poor calibration would lose the benchmark for reasons unrelated to the model.

## Scope

- Fitting strategy: joint surface fit versus sequential per-slice fits with the surface
  constraints imposed across slices; how `theta_t` is obtained (fitted or taken from ATM quotes).
- Constraint handling: penalty, projection, explicit constrained optimizer, or a reparameterization
  that satisfies the conditions by construction.
- Optimizer choice: QuantLib's `LBFGSB` (box bounds only), its Levenberg-Marquardt, or an external
  convex/conic solver. Note the standing evidence that for problems of this size CPU solvers are
  more than fast enough, and that GPU acceleration is out of scope.
- Starting values and robustness: how initial guesses are produced, and what happens on failure.

## Done when

One calibration strategy is chosen with its constraint-handling mechanism and optimizer named, the
rejected alternatives are recorded with reasons, and the expected failure modes are listed.

## Resolution

Closed 2026-09-13 (`bd8383d`); resolution recorded 2026-09-16 (D13, D14). Implemented in
`cpp/calibrator.cpp`.

- **Strategy:** one joint surface fit. The variables are the global `log eta`, `lambda`, `a`, `b`
  plus a free `theta_t` per expiry.
- **Objective:** unweighted least squares on total variance against `T * mid_iv^2` over included
  quotes.
- **Constraints:** every T01 condition as a hard inequality, per slice and per adjacent slice pair,
  with a `1e-8` margin. Parameter domains are box bounds.
- **Optimizer:** NLopt `LD_SLSQP` with forward-difference gradients, `maxeval 3000`,
  `ftol_rel 1e-10`. There is no unconstrained fallback: a failure emits `converged=false` and an
  error.

**Rejected alternatives.** No record from the time exists; these reasons are reconstructed from
D13/D14.
- QuantLib `LBFGSB`: box bounds only, so it cannot express the butterfly or calendar inequalities.
- QuantLib Levenberg–Marquardt: unconstrained.
- Penalty methods: soft constraints conflict with D14's hard-constraint requirement.
- Sequential per-slice fits: the calendar conditions couple adjacent slices, and D13 chose global
  smooth `rho`/`phi` families.
- Reparameterization that is arbitrage-free by construction (Mingone et al.): not adopted; the
  reason is unrecorded.

**Failure modes:**
- SLSQP is a local method on a non-convex problem, so the result depends on the starting point.
- Running out of `maxeval` reports non-convergence.
- `NLOPT_ROUNDOFF_LIMITED` counts as converged if the post-fit `validate` passes.
- Finite-difference gradients dominate the runtime.

**Discrepancies found while recording:**
- D13 says `theta_t` is initialised from ATM data. The code starts every expiry at
  `0.002 + 0.04 T`, with no ATM input.
- The objective fits mid IV, not the bid/ask band that D6 scores. This likely contributes to the
  low containment reported in `benchmark_report.md`.
