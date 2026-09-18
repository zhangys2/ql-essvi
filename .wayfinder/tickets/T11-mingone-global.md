---
id: T11
title: Decide the Mingone global product-of-intervals fallback
type: Research (AFK)
status: resolved
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

T08 left Mingone's global product-of-intervals reparameterization as the D14 fallback if
sequential Corbetta calendar-locks. After D18, should that joint solve be implemented or retired?

## Why it matters

The global map `(rho_i, theta_1, a_2..a_N, c_1..c_N) -> (theta_i, psi_i)` is the same 3N eSSVI
family, encoded so every point is butterfly- and calendar-safe by construction. Building it is
only justified if sequential fitting produces an empty feasible set or dense-grid calendar
violations.

## Done when

Either a global Mingone solver exists and is the fallback, or the fallback is retired with
evidence that sequential Corbetta does not lock on this snapshot.

## Resolution

Closed 2026-09-17. This is **D20**. **Retire** the joint Mingone global solver.

Evidence from the D18 sequential calibrator on the 2024-11-11 snapshot:

- every listed expiry produced a feasible slice (`diagnostics.csv` `converged=true`)
- dense-grid calendar violations are 0 (`essvi_score.json` / `essvi_common_score.json`)
- the sequential search already uses the Mingone adjacent interval
  `psi_1 p_2 < psi_2 < min(psi_1 theta_2 / theta_1, f_2)` (T01 plus Mingone's `psi` upper bound)

A joint look-ahead over later `f_j / prod p` would only matter if an early slice exhausted later
capacity. That did not happen. Sequential Corbetta remains the calibrator; unconstrained
fallback remains forbidden (D14). Reopen only if a later chain reports `empty feasible set`
or non-zero calendar violations after the MM butterfly change (T10).
