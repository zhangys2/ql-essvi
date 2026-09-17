# eSSVI vs CVI benchmark

Run on WSL2/GCC against the 2024-11-11 snapshot. Provenance is recorded in
[`evidence/benchmark_manifest.json`](evidence/benchmark_manifest.json), including the pushed
CVI notebook commit and hashes/row counts of the generated artifacts.

## Results on the shared benchmark grid

| model | normalized rows | fitted rows | scored quotes | bid/ask containment | containment on common 7,594 | butterfly violations | calendar violations | grid points | fit time (s) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| eSSVI (D18) | 14,364 | 7,740 | 7,740 | 1.8605% | 1.7777% | 0 | 0 | 5,151 | 0.072 |
| CVI | 14,364 | 7,594 | 7,594 | 0.2370% | 0.2370% | 1,780 | 3,963 | 5,050 | 26.936 |

Both models are scored on included quotes from the same 2024-11-11 snapshot. eSSVI (D18)
fits all 51 expiries and is arbitrage-scored on its 51 × 101 dense grid (5,151 points).
The shared normalized input contains 14,364 rows, of which 7,740 pass the deterministic filter.
Restricted to the 7,594 quote IDs both models fit, eSSVI contains 135 quotes (1.7777%) and CVI
contains 18 (0.2370%). D18 raised eSSVI containment from the D13 run's 1.14% to 1.86% with
per-slice `rho` in about [-0.49, 0.32], still inside T08's low-single-digit ceiling at `1e-12`.

The CVI notebook's additive export emits the exact shared 7-column fitted-quotes schema,
dense grid values, per-tenor parameter summaries, and diagnostics. Its executed notebook and
outputs are committed in the CVI repository. The ql-essvi adapters preserve all normalized
quote IDs and resample both models onto the identical common grid before arbitrage scoring.
CVI fitting behavior was not changed. Timing is indicative and is not a decisive
cross-language metric.

## Reproduction and safety

- Python owns normalization, adaptation, common-grid construction, schema validation, and scoring.
- C++ consumes normalized total-variance inputs and fits free `(theta_t, rho_t, psi_t)` per
  expiry under T01 plus Mingone's calendar `psi` upper bound (D18, Corbetta sequential). No
  unconstrained fallback is used. The objective penalises only fitted total variance outside
  each quote's bid/ask band, starting each `theta_t` from its nearest-ATM quote.
- Dense-grid arbitrage checks include tail points; quote containment uses observed quotes only.
- Raw/archive directories and QuantLib sources are not modified.
- Re-run the commands in the manifest after checking out the recorded ql-essvi and CVI commits.
