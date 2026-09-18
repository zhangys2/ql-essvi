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

Closed 2026-09-13 (`bd8383d`); resolution recorded 2026-09-16 (D13, D14) and updated
2026-09-17 for **D18** as implemented in `cpp/calibrator.cpp`.

- **Strategy:** sequential per-expiry fits going forward (Corbetta et al.). Each listed
  expiry has free `(theta_t, rho_t, psi_t)` under the T01/T10 butterfly bounds and the
  adjacent-slice calendar inequalities, including Mingone's sufficient `psi` upper bound.
  This supersedes the D13 joint surface fit of global `(log eta, lambda, a, b)` plus `theta_t`.
- **Objective:** bid/ask band loss in total variance. A quote costs nothing while the fitted `w`
  lies in `[T bid_iv^2, T ask_iv^2]`; outside the band it costs the squared distance to the nearer
  edge. This matches D6's containment metric. Adopted 2026-09-16, replacing unweighted least squares
  against `T * mid_iv^2`. The planned tiny mid-IV tie-break term was dropped: overlapping bands
  with different mids would pull the fit onto a band edge and just past it.
- **Starting point:** each `theta_t` starts at `T * mid_iv^2` of that expiry's included quote
  nearest ATM (smallest `|k|`), then is raised to honour calendar floors and the butterfly bound.
- **Constraints:** hard inequalities with a `1e-8` margin. `rho` is sampled; `psi` is searched
  inside the feasible interval given the previous slice (Mingone product-of-intervals, applied
  sequentially rather than as a joint 3N solve).
- **Optimizer:** no joint NLopt SLSQP. Per slice: coarse-then-fine `rho` grid and golden-section
  search on `psi`. There is no unconstrained fallback: a failure emits `converged=false` and an
  error.

**Rejected alternatives.**
- QuantLib `LBFGSB`: box bounds only, so it cannot express the butterfly or calendar inequalities.
- QuantLib Levenberg–Marquardt: unconstrained.
- Penalty methods: soft constraints conflict with D14's hard-constraint requirement.
- D13 joint 55-dimensional SLSQP on `(eta, lambda, a, b, theta_t)`: the four globals never left
  their start (T08).
- Mingone *global* joint reparameterization as the primary solver: sequential Corbetta never
  produced an empty feasible set or dense-grid calendar violations on this snapshot (T11).

**Failure modes:**
- The per-slice search is local in `rho`; a coarse grid can miss a better smile.
- Empty feasible interval at a tenor reports `converged=false`.
- Calendar coupling can still force `theta_t` above the local ATM band (T08).
