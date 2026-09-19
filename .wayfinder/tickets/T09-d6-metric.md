---
id: T09
title: Revisit D6 so the primary metric measures the model on this chain
type: Research (AFK)
status: resolved
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

After D18, exact `1e-12` bid/ask-IV containment is still about 1.86%. T08 said that if a real
Corbetta 3N fit stayed near 1%, D6 itself is the problem. What should the benchmark treat as
primary so that eSSVI vs CVI measures the models rather than a binary that this snapshot
structurally caps at low single digits?

## Why it matters

D6's in-band fraction is the comparison the project was built to make. On this listed-index
chain the bands are O(8–19) vol-bps, at least 12 adjacent ATM bands are calendar-infeasible,
and T01 originally used Gatheral–Jacquier sufficient butterfly bounds (later D21 / T10).
Exact containment then stays near zero for *any* arbitrage-free surface, so
"eSSVI 1.86% vs CVI 0.24%" does not show how far each fit sits from the market.

## Scope

- Recompute D18 residuals: exact containment, 10 vol-bp containment, mean/median IV distance
  to the nearer band edge (zero inside).
- Keep D6's definition (in-band fraction at `1e-12`) as a reported count; decide what is
  *decisive* for ranking.
- Do not replace D6 with a looser epsilon as a way to hide a several-vol-point residual
  (T08 already rejected that as a diagnosis).

## Done when

One ranking rule is chosen, the rejected alternatives are recorded, and the scorer emits the
new fields.

## Resolution

Closed 2026-09-17. This is **D19**. Implemented in `python/score_outputs.py`.

**Position.** Keep D6's exact in-band fraction at `1e-12`. Rank models by **mean and median
IV distance to the bid/ask band** (0 if the fitted IV is inside, otherwise the distance to
the nearer edge). Arbitrage-violation counts stay secondary (D6).

**Evidence, D18 vs CVI on the 2024-11-11 snapshot** (`normalized_quotes.csv` with
`fitted_quotes.csv` / `cvi_fitted_quotes.csv`):

| | n | exact in-band | 10 vol-bp | 1 vol point | mean dist | median dist |
|---|---:|---:|---:|---:|---:|---:|
| eSSVI | 7,740 | 1.8605% | 2.7390% | 8.8889% | 4.87 vol pts | 4.46 vol pts |
| CVI | 7,594 | 0.2370% | 0.6452% | 5.4912% | 42.79 vol pts | 29.94 vol pts |
| eSSVI on common 7,594 | 7,594 | 1.7777% | | | 4.85 vol pts | 4.45 vol pts |

10 vol-bp containment barely moves (1.86% → 2.74%), so this is still not a `1e-12` artefact.
Mean distance does move: eSSVI is about nine times closer to the bands than CVI. Most misses
on both models sit above ask (eSSVI 6,860 / 7,740; CVI 7,133 / 7,594).

**Load-bearing assumption.** D6's object of study remains "is the fitted IV inside the quoted
band?", not mid-IV RMSE. Distance-to-band is that same quantity with a magnitude.

**Rejected alternatives.**

- **Replace D6 with containment at 10 vol-bps or 1 vol point.** T08 already showed a modest
  epsilon does not change the story; D18 confirms it (2.74% and 8.89%). A looser binary still
  hides a 4.5-vol-point typical miss.
- **Switch the primary metric to price error (Corbetta).** Honest for the literature, but it
  is a different question than D6's quoted-band containment.
- **Leave exact containment as the only quality number.** After D18 it cannot distinguish a
  5-vol-point arb-free miss from a 40-vol-point interpolant.

**Scorer fields.** `bid_ask_containment` unchanged (`1e-12`). Added
`mean_iv_distance_to_band` and `median_iv_distance_to_band` in decimal IV (same units as
`bid_iv`).
