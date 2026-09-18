---
id: T08
title: Decide whether the eSSVI family has enough capacity for this chain
type: Research (AFK)
status: resolved
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

Why does the calibrated eSSVI surface keep only about 1% of included quotes inside their bid/ask
band, and which eSSVI form should the benchmark calibrate so that D6's primary metric measures the
model rather than an over-restricted parameterization?

## Why it matters

After D17, when eSSVI is calibrated directly against the bid/ask bands, it contains 88 of 7,740
included quotes (1.1370%), with zero arbitrage violations. On the 7,594 quotes both models fit, the
figures are 1.1193% for eSSVI and 0.2370% for CVI. At these levels neither fitter is close to the
market, so "eSSVI wins on containment" says little. The calibrated family (D13) has 4 global
parameters (`eta`, `lambda`, `a`, `b`) shared across 51 expiries, plus one `theta_t` per expiry.
Every smile shape therefore has to come from those 4 global parameters.

## Scope

- **Diagnose where the misfit sits:**
  - by expiry (the 7-day expiry has about 116% near-ATM IV), moneyness and band width
  - whether it is structural (the model cannot bend enough) or an optimisation failure (SLSQP local
    minimum or starting point)
- **Candidate forms, all still eSSVI or SSVI per D5 (raw per-slice SVI stays out of scope):**
  - free `rho_t` and `psi_t` per slice under the T01 butterfly and calendar conditions
    (Corbetta et al. slice-by-slice)
  - richer global `phi` or `rho` families
  - the arbitrage-free global parameterization of Mingone et al.
- **Consequences:**
  - which decisions each candidate reopens (D13, D14, D17)
  - parameter count against 7,740 quotes
  - calibration robustness and runtime
- **Honest ceiling:** what containment a well-fitted arbitrage-free surface can reach on this
  snapshot, given that containment is scored with a `1e-12` tolerance on tight listed-index bands.

## Done when

The main cause of the low containment is identified with evidence, and one parameterization is
recommended with the decisions it changes and the rejected alternatives recorded. Implementation is
authorised separately.

## Resolution

Closed 2026-09-16. This is D18. It supersedes D13's choice of a 4-global-parameter shape family.
The canonical statement is [`docs/parameterization.md`](../../docs/parameterization.md).
Implemented 2026-09-16: sequential Corbetta calibration of free `(θ_t, ρ_t, ψ_t)` in
`cpp/calibrator.cpp`, with Mingone's sufficient calendar `ψ` upper bound added so the dense-grid
calendar scan stays clean.

**Position.** Calibrate eSSVI as three free parameters `(θ_t, ρ_t, ψ_t)` per expiry under the
existing T01 Gatheral–Jacquier butterfly and Hendriks–Martini / Corbetta calendar inequalities,
not D13's shared `(η, λ, a, b)` plus `θ_t`.

**Cheapest disconfirming observation.** Run a Corbetta sequential (or Mingone global) fit of that
3N family on this snapshot with the D17 band objective; if 1e-12 containment stays near 1%, the
form change was not the bottleneck and D6 itself needs revisiting.

**Load-bearing assumption.** D5's object of study is the eSSVI *slice* `(θ, ρ, ψ)`, with constant
`ρ` recovering SSVI — not a particular low-dimensional curve for `ρ(T)` and `φ(θ)`.

### Diagnosis (current D13/D17 fit)

Evidence from `parameters.csv`, `diagnostics.csv`, `fitted_quotes.csv`, `normalized_quotes.csv`,
and `python/score_outputs.py` on the 2024-11-11 snapshot (7,740 included quotes, 51 expiries).

1. **The four globals never left the SLSQP start.** Recovered `η = 0.3`, `λ = 0.25`, `ρ(T) ≡ 0`
   (because `a = 0`, `b = 0`). That is exactly `x[0]=log(0.3)`, `x[1]=0.25`, `x[2]=0`, `x[3]=0`
   in `cpp/calibrator.cpp`. Only the 51 `θ_t` moved. The published surface is symmetric SSVI with
   a frozen power-law `φ`, not a fitted eSSVI skew. `supported`

2. **This is not a `1e-12` scoring artefact.** Median half-band is 8.1 vol-bps; mean half-band is
   19 vol-bps. Median distance from the fitted IV to the nearer band edge is 5.8 vol points.
   Loosening the scorer to 10 vol-bps raises containment only from 1.14% to 2.38%; 1 vol point
   raises it to 14.5%. `supported`

3. **Misfit is a smile-shape and term-structure problem, not a short-dated ATM spike.** The
   7-day nearest-ATM included mid is 10.55% and the fit matches it. The "~116% near-ATM IV" in
   T05/T08 is `contradicted` for included quotes: no included 7-day mid exceeds 60.5% (k = −0.27).
   IVs around 118% are excluded `iv_inversion_failed` asks on 3400-strike puts. Put wings
   (k < −0.2) contain 0.22% with mean distance 19.3 vol points; near-ATM (|k| < 0.05) still only
   2.4%. 4,575 quotes sit below bid, 3,077 above ask. `supported`

4. **Calendar no-arbitrage and this chain's ATM term structure cannot both be honoured.** 12 of
   50 adjacent expiry pairs have market ATM *bands* that are strictly calendar-infeasible
   (`θ_ask(T_{i+1}) ≤ θ_bid(T_i)`), e.g. T = 0.044 ATM mid 11.4% vs T = 0.049 ATM mid 5.2%.
   Any arbitrage-free surface must miss at least one ATM in each such pair. On the fitted
   surface, adjacent `θ` and `ψ` slacks are ~6e-6 (binding); the first GJ butterfly slack is
   idle because `ψ` is tiny. `supported`

5. **Even independent GJ-constrained SSVI slices barely contain.** A coarse per-slice search
   over `(θ, ρ, ψ)` with T01 butterfly and *no* calendar coupling reaches ~2.0% containment
   (mean |ρ| ≈ 0.59, shortest slice 6.5%, best slice 29% at 5y). That is only a lower bound —
   the search is not Corbetta-quality — but it already shows that unlocking D13's four globals
   cannot, by itself, produce a high D6 score on this snapshot. `inferred` (optimizer is crude)

Candidate explanations, tested in that order:

| Hypothesis | Prediction | Result |
|---|---|---|
| 4 globals stuck at start | `η, λ, a, b` match `calibrator.cpp` init; `ρ ≡ 0` | Confirmed |
| `1e-12` on tight bands | modest ε jumps containment | Rejected (10bp → 2.4%) |
| Calendar vs non-monotone ATM `θ` | some adjacent ATM bands disjoint in total variance | Confirmed (12 pairs) |
| GJ curvature bound vs short-dated wings | independent 3-param slices still miss most quotes | Confirmed at ~2% |
| SLSQP local min on an otherwise rich family | different start would fit smiles with `ρ ≠ 0` | Partial: globals did not move, but the family is also too thin |

### Recommended form

**eSSVI with free `(θ_t, ρ_t, ψ_t)` per listed expiry** (Hendriks–Martini eSSVI; Corbetta et al.
2018, arXiv:1804.04924). Constant `ρ_t` still recovers SSVI (D5). Parameter count: 3 × 51 = 153
against 7,740 quotes (~51 quotes/slice). T01 inequalities stay as hard constraints; do not add
Mingone's extra sufficient calendar bound `ψ₂ ≤ (ψ₁/θ₁) θ₂` unless a later ticket chooses it.

**Calibration encoding (for the later implementation, not a new form):** fit slices sequentially
going forward as in Corbetta §3 (anchor near-ATM, sample `ρ`, 1-D search on `ψ` inside the T01
bounds). That is the robustness fix for D14: the current joint 55-dimensional SLSQP did not move
four globals in 498 evaluations, and a raw 153-dimensional SLSQP would be worse. Mingone 2022
(arXiv:2204.00312) is the *same* 3N family reparameterized as a product of intervals for a
global solve; switch to it only if sequential calendar locking is observed after Corbetta is
implemented.

Keep D17's bid/ask band objective in total variance. Drop D13's power-law `φ` and sigmoid `ρ(T)`.

**Decisions reopened.** D13 (shape family) — superseded by this ticket. D14 (joint NLopt SLSQP)
— replace with Corbetta sequential, with Mingone global as the fallback. D17 (band loss, ATM `θ`
start) — keep.

**Runtime.** Corbetta report 1.2 s in Python for 12 SPX maturities; 51 slices should stay well
inside the "indicative only" timing of D4. Finite-difference 55-D SLSQP is the thing to delete,
not a reason to keep D13.

### Honest ceiling on this snapshot

D6 at `1e-12` on these listed bands will not look like a high score even with the right form.

- Half-bands are O(8–19) vol-bps, so any IV error above ~10 bps is a miss. Corbetta's published
  quality is *price* error of a few bps of spot, not IV-band containment. `verified-source`
  (Corbetta §4; this chain's band widths).
- At least 12 ATM quotes are pairwise calendar-infeasible. `supported`
- GJ sufficient butterfly caps short-dated `ψ` at `√(4θ/(1+|ρ|))` ≈ 0.022 at 7 days with
  `ρ = −0.8`, which cannot reach the included 60% put wing. The Martini–Mingone necessary-and-sufficient
  butterfly infimum is larger, but T01 originally used GJ; that constraint ticket is
  [T10](T10-mm-butterfly.md) / D21. `inferred`
- A well-fitted 3N eSSVI should beat 1.14% and should beat CVI on the common 7,594 by a meaningful
  margin if the model has capacity worth measuring. A plausible range at `1e-12` is **low single
  digits to low tens of percent**, not 80%+. Treat anything still near 1% after a real Corbetta
  fit as a D6-metric problem, not another parameterization problem. `inferred`

### Rejected alternatives

- **Keep D13 and only fix the optimizer / start.** A fitted `(η, λ, a, b)` still imposes one
  skew curve and one wing curve on 51 expiries; Hendriks–Martini introduced eSSVI precisely
  because constant/`T`-smooth `ρ` underfits. The independent-slice ρ's already span roughly
  −0.98 to +0.32.
- **Richer global `φ` or `ρ` families (still low-dimensional).** Same objection at higher
  parameter count; does not give per-slice smile freedom and does not relax GJ or calendar.
- **Mingone as a different model.** It is a constraint reparameterization of the recommended
  3N form, not extra capacity. Prefer it as D14 fallback, not as the headline form.
- **Raw per-slice SVI.** Out of scope (D5 / map).
- **Relaxing D6's `1e-12` to "near band" as the diagnosis.** That would hide a 5.8-vol-point
  residual; it is a possible *later* metric ticket, not an explanation of the current 1%.
  Done as [T09](T09-d6-metric.md) / D19: exact containment is kept, ranking uses distance-to-band.

### Sources

- Fitted artifacts in this repo: `parameters.csv`, `diagnostics.csv`, `fitted_quotes.csv`,
  `normalized_quotes.csv`, `benchmark_report.md`, `cpp/calibrator.cpp`.
- Gatheral & Jacquier, arXiv:1204.0646 (SSVI, GJ butterfly).
- Corbetta, Cohort, Laachir & Martini, arXiv:1804.04924 (sequential 3N eSSVI calibration).
- Mingone, arXiv:2204.00312 (global product-of-intervals reparam of the same 3N family).
- Hendriks & Martini as cited by both; T01 already records the adjacent-slice calendar test.
