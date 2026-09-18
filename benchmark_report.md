# eSSVI vs CVI benchmark

Run on WSL2/GCC against the 2024-11-11 snapshot. Provenance is recorded in
[`evidence/benchmark_manifest.json`](evidence/benchmark_manifest.json), including the pushed
CVI notebook commit and hashes/row counts of the generated artifacts.

## Results on the shared benchmark grid

| model | normalized rows | fitted rows | scored quotes | bid/ask containment | containment on common 7,714 | mean IV dist (vol pts) | median IV dist (vol pts) | butterfly violations | calendar violations | grid points | fit time (s) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| eSSVI (D21) | 14,364 | 7,740 | 7,740 | 2.1318% | 2.0871% | 4.71 | 4.37 | 0 | 0 | 5,151 | 0.643 |
| CVI (D22) | 14,364 | 7,714 | 7,714 | 0.2722% | 0.2722% | 42.80 | 29.82 | 1,780 | 3,963 | 5,050 | 26.936 |

Both models are scored on included quotes from the same 2024-11-11 snapshot. eSSVI (D21)
fits all 51 expiries under the Martini–Mingone butterfly infimum and is arbitrage-scored on
its 51 × 101 dense grid (5,151 points). The shared normalized input contains 14,364 rows, of
which 7,740 pass the deterministic filter. **D22** interpolates the existing CVI smile onto
missing strikes so CVI scores 7,714 quotes; the remaining 26 are the 2028-12-15 tenor CVI
never fitted. On that common 7,714, eSSVI contains 161 quotes (2.0871%) and CVI contains 21
(0.2722%).

**D19** ranks by IV distance to the band (zero inside): eSSVI's mean/median miss is
4.71 / 4.37 vol points versus CVI's 42.80 / 29.82. Dense-grid butterfly and calendar
violations remain 0 for eSSVI. D21 (MM instead of GJ) raised exact containment from D18's
1.86% to 2.13% and cut the mean miss from 4.87 to 4.71 vol points.

The CVI notebook's additive export emits the exact shared 7-column fitted-quotes schema,
dense grid values, per-tenor parameter summaries, and diagnostics. Its executed notebook and
outputs are committed in the CVI repository. The ql-essvi adapters preserve all normalized
quote IDs and resample both models onto the identical common grid before arbitrage scoring.
CVI fitting behavior was not changed. Timing is indicative and is not a decisive
cross-language metric.

## Reproduction and safety

- Python owns normalization, adaptation, common-grid construction, schema validation, and scoring.
- C++ consumes normalized total-variance inputs and fits free `(theta_t, rho_t, psi_t)` per
  expiry under T10 MM butterfly plus Mingone's calendar `psi` upper bound (D18/D21, Corbetta
  sequential). No unconstrained fallback is used. The objective penalises only fitted total
  variance outside each quote's bid/ask band, starting each `theta_t` from its nearest-ATM quote.
- Dense-grid arbitrage checks include tail points; quote containment uses observed quotes only.
  Exact in-band fraction uses `1e-12`; mean/median IV distance to the band is the ranking metric (D19).
- Raw/archive directories and QuantLib sources are not modified.
- Re-run the commands in the manifest after checking out the recorded ql-essvi and CVI commits.
