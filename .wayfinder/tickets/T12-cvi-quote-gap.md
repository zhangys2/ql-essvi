---
id: T12
title: Close the CVI vs eSSVI included-quote fairness gap
type: Task (AFK)
status: resolved
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

CVI's export fitted 7,594 of 7,740 included quotes and left 146 as `not_fitted`. How should the
benchmark close that residual fairness gap without changing CVI fitting behaviour (D11)?

## Why it matters

Headline scores on 7,740 vs 7,594 mix a model difference with an export difference. T06 already
scored containment on the intersection; the leftover was those 146 rows.

## Done when

Every included quote that lies on a CVI-fitted tenor is assigned a CVI IV from the existing CVI
smile, and quotes on tenors CVI never fitted remain `not_fitted`. Adapter tests cover both.

## Resolution

Closed 2026-09-17. This is **D22**. Do **not** change the CVI notebook.

Diagnosis: the 146 `not_fitted` rows were included quotes (empty `filter_reason`), 90 puts / 56
calls, scattered across many expiries. After interpolation, 120 of them receive a CVI IV from
the existing smile. The remaining 26 are the 2028-12-15 tenor CVI never fitted and stay
`not_fitted`. Canonical ranking uses the 7,714-quote intersection.

`python/adapt_cvi_output.py` now linearly interpolates CVI total variance in log-moneyness on
each CVI tenor when the exact strike is absent. That evaluates the *existing* CVI smile at the
shared filter's strikes. Quotes whose tenor has no CVI smile stay `not_fitted`. D11 holds:
fitting cells are untouched; only the ql-essvi adapter fills IVs.
