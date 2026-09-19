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

## Resolution

Closed 2026-09-13 (`bd8383d`); resolution recorded 2026-09-16. The canonical statement is
[`docs/parameterization.md`](../../docs/parameterization.md). **D18** (2026-09-16,
[T08](T08-essvi-model-capacity.md)) superseded D13's power-law `phi` and sigmoid `rho(T)` with
free `(theta_t, rho_t, psi_t)` per expiry. **D21** (2026-09-17, [T10](T10-mm-butterfly.md))
replaced Gatheral–Jacquier butterfly sufficiency with the Martini–Mingone infimum.

- **Form:** `w(k,T) = 1/2 (theta + rho psi k + sqrt((psi k + theta rho)^2 + theta^2 (1-rho^2)))`
  with `k = log(K/F(T))`, `theta` forward ATM total variance, `psi = theta phi`.
  Each listed expiry has free `(theta_t, rho_t, psi_t)`. Constant rho recovers SSVI (D5).
- **Superseded D13 families:** power law `phi(theta) = eta theta^(-lambda)` and
  `rho = -0.98 + 1.96 sigmoid(a + b log T)` are not calibrated.
- **Sufficient conditions, enforced during calibration** (strict, margin `1e-8`):
  - `theta > 0`, `psi > 0`, `|rho| < 1`
  - butterfly (Martini–Mingone N&S, D21): `psi (1+|rho|) < 4` (Roger-Lee) and
    `psi^2 <= f_MM(theta, |rho|)` as in [T10](T10-mm-butterfly.md). This replaces
    Gatheral–Jacquier `psi^2 (1+|rho|) < 4 theta`.
  - calendar, adjacent slices (Hendriks–Martini; Corbetta et al.), plus Mingone's sufficient
    `psi` upper bound (added with D18): `theta_2 > theta_1`, `psi_2 > psi_1`,
    `psi_2 > psi_1 max((1+rho_1)/(1+rho_2), (1-rho_1)/(1-rho_2))`, and
    `psi_2 < (psi_1 / theta_1) * theta_2`.
- **Checked afterwards only:** dense-grid butterfly (call-price convexity) and calendar (total
  variance non-decreasing at fixed `k`) scans in `python/score_outputs.py`, tails included.
