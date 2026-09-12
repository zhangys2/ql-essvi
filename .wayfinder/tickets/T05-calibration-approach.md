---
id: T05
title: Choose the eSSVI calibration approach under no-arbitrage constraints
type: Research (AFK)
status: open
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
