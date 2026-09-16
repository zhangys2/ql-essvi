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
- **Objective:** bid/ask band loss in total variance. A quote costs nothing while the fitted `w`
  lies in `[T bid_iv^2, T ask_iv^2]`; outside the band it costs the squared distance to the nearer
  edge. This matches D6's containment metric. Adopted 2026-09-16, replacing unweighted least squares
  against `T * mid_iv^2`. The planned tiny mid-IV tie-break term was dropped: overlapping bands
  with different mids would pull the fit onto a band edge and just past it.
- **Starting point:** each `theta_t` starts at `T * mid_iv^2` of that expiry's included quote
  nearest ATM (smallest `|k|`), raised where needed to at least 1.01 times the previous expiry's
  start. Adopted 2026-09-16, replacing `0.002 + 0.04 T`, which could not reach a feasible band fit
  within `maxeval` on the synthetic test. The increasing floor is needed on the real chain: without
  it, the 7-day expiry's ~116% near-ATM IV starts above the next expiry, and SLSQP stops at an
  infeasible point.
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
- Running out of `maxeval` or `maxtime` reports `converged=false`. Before 2026-09-16 it counted as
  converged, because `rc > 0` includes `NLOPT_MAXEVAL_REACHED`.
- `NLOPT_ROUNDOFF_LIMITED` counts as converged if the post-fit `validate` passes.
- Finite-difference gradients dominate the runtime.

**Discrepancies found while recording, fixed 2026-09-16:** the code did not start `theta_t` from
ATM data as D13 says, and it fitted mid IV rather than the band D6 scores. Both are fixed as
described above and covered by the `essvi::calibrate` test in `tests/test_essvi.cpp`.
