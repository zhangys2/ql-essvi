---
id: T10
title: Replace GJ butterfly sufficiency with the Martini–Mingone N&S infimum
type: Research (AFK)
status: resolved
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

Should calibration keep Gatheral–Jacquier's sufficient butterfly bounds, or switch to the
Martini–Mingone necessary-and-sufficient infimum that T08 left as a separate constraint ticket?

## Why it matters

GJ's second inequality `psi^2 (1+|rho|) < 4 theta` caps short-dated `psi` well below what an
eSSVI slice can do without butterfly arbitrage. T08 identified that cap as one reason D6
containment stays low-single-digit even after D18 unlocked per-slice `(theta, rho, psi)`.

## Done when

The bound used in `butterflySafe` and during calibration is stated, traced to a source, and
enforced in code with a test that a GJ-infeasible but MM-feasible slice is accepted.

## Resolution

Closed 2026-09-17. This is **D21**. Implemented in `cpp/essvi_surface.cpp` (`mmPsi2Bound`,
`butterflySafe`) and `cpp/calibrator.cpp` (`anchored` raises `theta` until MM holds).

Keep Roger-Lee `psi (1+|rho|) < 4`. Replace GJ `psi^2 (1+|rho|) < 4 theta` with

```text
psi^2 <= inf_{l > l2(|rho|)}  4 theta sqrt(1-rho^2) h(l)^2
                              / (theta sqrt(1-rho^2) g(l)^2 - g2(l))
```

with `N(l)=sqrt(1-rho^2)+rho l+sqrt(l^2+1)`, `g=N'/4`,
`h=1-(l-rho/sqrt(1-rho^2)) N'/(2N)`, `g2=N''-(N')^2/(2N)`, and
`l2=1/tan(arccos(-|rho|)/3)`, as in Mingone arXiv:2204.00312 §2.2.2 (Martini–Mingone
arXiv:2106.02418 Prop. 6.3). The infimum is evaluated on a log-spaced grid in `l`. Calibration searches `psi` in
`[psi_lower, min(4/(1+|rho|), sqrt(f_MM(theta_ATM)))]` because, unlike GJ, raising `theta`
cannot make an arbitrarily large `psi` butterfly-safe.

Dense-grid butterfly scans in `python/score_outputs.py` remain the independent check.
