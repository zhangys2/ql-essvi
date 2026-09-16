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
[`docs/parameterization.md`](../../docs/parameterization.md); this is its decision record (D13).

- **Form:** `w(k,T) = 1/2 (theta + rho psi k + sqrt((psi k + theta rho)^2 + theta^2 (1-rho^2)))`
  with `k = log(K/F(T))`, `theta` forward ATM total variance, `psi = theta phi`.
- **`phi` family:** power law `phi(theta) = eta theta^(-lambda)`, `eta > 0`, `0 <= lambda <= 1/2`.
- **`rho` family:** `rho = -0.98 + 1.96 sigmoid(a + b log T)`, `a in [-5,5]`, `b in [-2,2]`, so
  `|rho| < 0.98`. Constant rho (`b = 0`) recovers SSVI (D5).
- **Sufficient conditions, enforced during calibration** (strict, margin `1e-8`):
  - `theta > 0`, `psi > 0`, `|rho| < 1`
  - butterfly (Gatheral–Jacquier 2014): `psi (1+|rho|) < 4`, `psi^2 (1+|rho|) < 4 theta`
  - calendar, adjacent slices (Hendriks–Martini; Corbetta et al.): `theta_2 > theta_1`,
    `psi_2 > psi_1`, `psi_2 > psi_1 max((1+rho_1)/(1+rho_2), (1-rho_1)/(1-rho_2))`
- **Checked afterwards only:** dense-grid butterfly (call-price convexity) and calendar (total
  variance non-decreasing at fixed `k`) scans in `python/score_outputs.py`, tails included. The
  necessary-and-sufficient butterfly infimum is deliberately not used.

**Rho driver:** rho is a function of `log T`, not `log theta`. When this resolution was recorded,
the doc said `log theta` while the code used `log T`. On 2026-09-16 the doc was corrected to match
the code, which leaves results unchanged. Both drivers increase with expiry in a calendar-safe fit.
