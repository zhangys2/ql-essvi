# eSSVI vs CVI benchmark

Run on WSL2/GCC against the 2024-11-11 snapshot. Provenance is recorded in
[`evidence/benchmark_manifest.json`](evidence/benchmark_manifest.json), including the pushed
CVI notebook commit and hashes/row counts of the generated artifacts.

## Results on the shared benchmark grid

| model | normalized rows | fitted rows | scored quotes | bid/ask containment | containment on common 7,594 | butterfly violations | calendar violations | common grid points | fit time (s) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| eSSVI | 14,364 | 7,740 | 7,740 | 1.1370% | 1.1193% | 0 | 0 | 5,050 | 4.042 |
| CVI | 14,364 | 7,594 | 7,594 | 0.2370% | 0.2370% | 1,780 | 3,963 | 5,050 | 26.936 |

Both models are scored on the same 50-maturity intersection and identical 101-point
`k=-0.5..0.5` grid at each maturity (5,050 points). The shared normalized input contains
14,364 rows, of which 7,740 pass the deterministic filter. Both fitted-quote exports
preserve all 14,364 quote IDs. eSSVI produces 7,740 fitted rows; the unchanged legacy CVI
fit produces 7,594 fitted rows and leaves the remaining included rows as `not_fitted`. The
scorer measures each model's containment over its own fitted quotes, as recorded in
`essvi_common_score.json` and `cvi_common_score.json`. Restricted to the 7,594 quote IDs both
models fit, eSSVI contains 85 quotes (1.1193%) and CVI contains 18 (0.2370%). The dropped
maturity and quote rows are explicit limitations of benchmarking the legacy fitter, not hidden
filtering.

The CVI notebook's additive export emits the exact shared 7-column fitted-quotes schema,
dense grid values, per-tenor parameter summaries, and diagnostics. Its executed notebook and
outputs are committed in the CVI repository. The ql-essvi adapters preserve all normalized
quote IDs and resample both models onto the identical common grid before arbitrage scoring.
CVI fitting behavior was not changed. Timing is indicative and is not a decisive
cross-language metric.

## Reproduction and safety

- Python owns normalization, adaptation, common-grid construction, schema validation, and scoring.
- C++ consumes normalized total-variance inputs and enforces the eSSVI power-law and smooth
  bounded-rho family with hard NLopt SLSQP constraints; no unconstrained fallback is used.
  The objective penalises only fitted total variance outside each quote's bid/ask band, starting
  each `theta_t` from its nearest-ATM quote. Hitting the evaluation limit is reported as a failure.
- Dense-grid arbitrage checks include tail points; quote containment uses observed quotes only.
- Raw/archive directories and QuantLib sources are not modified.
- Re-run the commands in the manifest after checking out the recorded ql-essvi and CVI commits.
