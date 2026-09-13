# eSSVI vs CVI benchmark

Run on WSL2/GCC against the 2024-11-11 snapshot. Provenance is recorded in
[`evidence/benchmark_manifest.json`](evidence/benchmark_manifest.json), including the pushed
CVI notebook commit and hashes/row counts of the generated artifacts.

## Results

| model | normalized rows | fitted rows | common scored quotes | bid/ask containment | butterfly violations | calendar violations | grid points | fit time (s) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| eSSVI | 14,364 | 7,740 | 7,365 | 0.6925% | 0 | 0 | 5,151 | 2.425 |
| CVI | 14,364 | 7,365 | 7,365 | 0.2444% | 2,138 | 2,398 | 5,000 | 3.177 |

The shared normalized input contains 14,364 rows, of which 7,740 pass the deterministic
filter. Both fitted-quote exports preserve all 14,364 quote IDs. The eSSVI fitter produces
7,740 fitted rows; the legacy CVI fit produces 7,365 fitted rows and marks 375 included rows
as `not_fitted` because its existing fitting behavior selects a narrower OTM subset. The
reported containment comparison therefore uses the 7,365-row intersection, while the full
row counts remain visible above. This is an explicit limitation of benchmarking an unchanged
legacy fitter, not an unreported filtering difference.

CVI fitting behavior was not changed. The additive notebook export now emits fitted quotes,
dense grid values with discount factors and tail metadata, per-tenor CVI parameter summaries,
and diagnostics. The ql-essvi adapters only normalize those outputs to the shared contract.
Timing is indicative and is not a decisive cross-language metric.

## Reproduction and safety

- Python owns normalization, adaptation, schema validation, and scoring.
- C++ consumes normalized total-variance inputs and enforces the eSSVI power-law and smooth
  bounded-rho family with hard NLopt SLSQP constraints; no unconstrained fallback is used.
- Dense-grid arbitrage checks include tail points; quote containment uses observed quotes only.
- Raw/archive directories and QuantLib sources are not modified.
- Re-run the commands in the manifest after checking out the recorded ql-essvi and CVI commits.
