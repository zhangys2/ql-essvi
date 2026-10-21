# ql-essvi Calibrator Speedup — Implementation Spec

Status: WP0-WP11 code is present on `perf/essvi-calibrator`; WP10 now uses bounded Jacobi warm refits with sequential calendar repair. The current WP6-WP11 benchmark still misses multiple latency, fallback, and loss targets; measured results and verification are recorded in [`perf-results.md`](perf-results.md). This spec was originally drafted from a read-only source review; its estimates have been measured and its warm acceptance criteria updated by Addendum A.
Parent: [[2026-10-15-spy-options-cpu-latency-benchmark-design]] (stage S4).

## 1. Goal

Make `essvi::calibrate()` fast enough that a full SPY/SPX-sized chain (about 40 expiries x 100 quotes per expiry) can be re-calibrated in a warm-started update within a few milliseconds on vps1 (4 vCPUs, AMD EPYC 9354P), without changing what the calibrator computes.

Acceptance targets, with warm scenarios revised by Addendum A:

| Metric | Target | Hard limit | Other |
|---|---:|---:|---|
| Cold full chain (40 x 100), speedup vs baseline | >= 10x | >= 5x | |
| Warm quiet update, full chain, p99 | <= 3 ms | <= 10 ms | |
| Warm normal update, full chain, p99 | <= 3 ms | <= 10 ms | fallback rate <= 1% |
| Warm front update, full chain, p99 | <= 3 ms | <= 10 ms | |
| Warm fast update, full chain, p99 | <= 10 ms | <= 20 ms | fallback rate <= 10% |
| Single slice, cold | <= 1 ms | <= 5 ms | |
| Output parameters, cold path, vs baseline | within 1e-9 relative where CM1 does not tighten the psi search interval; report all headroom-driven differences | 1e-6 relative where the interval is unchanged | |
| Dense-grid butterfly and calendar violations | 0 | 0 | |

For every warm scenario: zero invalid surfaces and warm failures, summed band loss within 5% of cold, and a clean dense-grid arbitrage scan. Report any missed target rather than eliding it.

## 2. Non-goals

- No change to the parameterization, the band-loss objective (D17), the Martini-Mingone butterfly bound (D21), or the "no unconstrained fallback" rule (D18-D22 stay in force).
- CM1 amendment: add 0.5% proportional headroom to the adjacent-slice psi-ratio floor when feasible; retry with the original strict-epsilon floor only if that leaves no feasible set. The original strict calendar conditions remain the public validity contract.
- No American implied-vol inversion, delta-knot spline, or projection stage (other tickets).
- No change to the CSV contract headers (`docs/csv-contract.md` is exact-header). Timing goes in a new file.

## 3. What the code does today (from reading it)

- `calibrate()` sorts expiries and calls `fitSlice()` for each, sequentially; each slice is constrained by the previous one (`pairSafe`: `butterflySafe` + `calendarSafe`).
- `fitSlice()` searches rho on a 0.05 grid (about 39 values) with a golden-section search on psi (22 steps) inside the feasible interval, then a 0.01 refinement window around the best rho (about 13 values, 18 steps), then 6 theta scalings. Roughly 1,000-1,500 candidate evaluations per slice. Every evaluation calls `pairSafe`.
- `butterflySafe()` calls `mmPsi2Bound(theta, |rho|)`, which minimizes a ratio over 257 log-spaced points of `l`, each with `pow`, three `sqrt`, a division; `acos` and `tan` once per call. `anchored()` can call it up to 48 more times.
- `bandLoss()` is one pass over the slice's quotes, with a `log` per quote recomputed every evaluation.
- `readQuotes()` uses an `unordered_map` lookup per field per row (CSV path only).
- No warm start, no reuse of the previous snapshot, no per-stage timing.

## 4. Hypothesis to confirm first

`mmPsi2Bound` dominates the run time, not `bandLoss`. Rough estimate: one call is about 20-40 microseconds (257 iterations with `pow` and `sqrt`), called about 1-2 times per candidate, so about 25-60 ms per slice and 1-2 s for 40 slices. `bandLoss` over 100 quotes is about 3-5 microseconds. If profiling disagrees, stop and report before optimizing the wrong thing.

Useful structure: in `ratio(l)`, the quantities `g`, `h`, `g2` and `s` depend only on `(l, rho)`, not on `theta`. Only `theta` enters through `4 theta s h^2 / (theta s g^2 - g2)`. So the per-rho part can be precomputed once and reused across all theta.

## 5. Work packages

### WP0 - Build and baseline (do first)

1. vps1 has no `cmake` and no `~/QuantLib/build/release`, and `CMakeLists.txt` requires QuantLib via `find_package(... REQUIRED)` even for the core library. Without sudo, install cmake into a venv (`uv pip install cmake` or `pip install cmake`).
2. Split the build: a core library `essvi_core` (`essvi_surface.cpp`, `calibrator.cpp`) with no QuantLib dependency, and the QuantLib adapter (`essvi_ql.cpp`, `quantlib_smoke`, `essvi_ql_tests`) behind `option(ESSVI_WITH_QUANTLIB ...)` that turns off automatically when QuantLib is not found. Existing core tests (`essvi_tests`) must pass without QuantLib.
3. Add `bench/essvi_bench.cpp` (new executable) that:
   - generates a deterministic synthetic chain from a known valid surface (N expiries, M quotes per expiry, bid/ask band around the surface, seeded noise), with defaults 40 x 100;
   - optionally reads a `normalized_quotes.csv`;
   - times `calibrate()` cold, repeated, and reports median, p90, p99, max, plus `iterations`;
   - counts `mmPsi2Bound` calls and total time spent in it (temporary instrumentation, behind a compile-time flag).
4. Profile (`perf` if available, else `gprof` or manual timers). Commit the baseline to `docs/perf-baseline.md` with hardware, compiler, flags, and the profile breakdown. Confirm or refute section 4.

Gate: report the baseline and profile before starting WP1.

### WP1 - Fast, safe `mmPsi2Bound`

- Keep the existing function as `mmPsi2BoundReference` (test-only) and add a fast implementation with the same signature.
- Precompute the rho-dependent parts once per rho. Options, in order of preference: cache keyed on rho (the calibrator uses a fixed rho grid); or a table over `(rho, theta)` with safe interpolation; or a closed-form or Newton minimizer.
- Safety requirement: the reference is a minimum over a finite grid, so it can be slightly above the true infimum, i.e. slightly non-conservative. The new function must return a value less than or equal to the reference (up to 1e-12 relative) and must not exceed the true infimum by more than 1e-12 relative. Verify the infimum by a very fine scan plus local refinement in tests.
- Test: random `(theta, |rho|)` over `theta in [1e-6, 5]`, `|rho| in [0, 0.99]`, at least 1e6 samples, comparing against the reference and the fine scan.

### WP2 - Remove redundant work in the cold search (same search path)

- Precompute per slice, once: `k = log(K/F)` per quote and the band variances `expiry * bid_iv^2`, `expiry * ask_iv^2` in contiguous arrays; make `bandLoss` branch-light and vectorizable.
- Evaluate `butterflySafe` once per `(rho, theta)` rather than once per psi candidate where `theta` does not change; hoist `calendarSafe` pieces that depend only on the previous slice.
- No per-evaluation allocation.
- Constraint: the search order and candidate set stay the same, so the cold-path output matches the baseline within tolerance (the "within 1e-9 relative" target). Behavior-changing search changes belong in WP3.

### WP3 - Warm start

- Add `CalibrationResult calibrateWarm(const std::vector<Quote>& quotes, const Surface& previous, const WarmOptions& opt)`.
- Per slice: search locally around the previous `(rho, psi, theta)` (rho window about +-0.03, psi interval shrunk around the previous value, theta scalings), instead of the full 0.05 grid.
- Reuse a slice unchanged if its quotes moved less than a tolerance (max change in bid/ask IV below `opt.quote_tol`) and its predecessor is unchanged or still compatible (`calendarSafe` holds).
- If slice `j` changed, revalidate slice `j+1` against the new slice `j` and refit it if the calendar conditions no longer hold. Sequential dependence is preserved.
- Quality gates, per slice: warm result must be feasible (`pairSafe`) and its band loss must not exceed the cold-search value by more than `opt.loss_tol`. How to check the second without running the cold search every time: run the cold search on every Nth update and on any slice where the warm loss exceeds a fixed threshold; otherwise fall back to the cold slice search on gate failure.
- Never return an unconstrained or infeasible result; on failure, `converged=false` with an error (D18).
- Deterministic: same inputs, same outputs.

### WP4 - Observability

- Extend `CalibrationResult` with per-call stage timings, number of objective evaluations, number of `mmPsi2Bound` calls, `slices_refit`, `slices_reused`, and `path` (cold, warm, fallback).
- Do not change `diagnostics.csv` headers. Emit timings from the bench as JSON or a new `timings.csv`.

### WP5 - Optional parallelism (only if WP1-WP4 miss the targets)

- Within one slice, candidates over the rho grid are independent: evaluate them in parallel (`std::thread` or OpenMP, at most 4 threads) and reduce deterministically (fixed tie-breaking). Slices stay sequential. Report speedup and whether results remain bit-identical to single-thread.

## 6. Tests

All new tests live in `tests/` and run via ctest; existing tests must still pass. Do not change an existing test's expectation; if one seems wrong, name the requirement you misread and make that fix its own declared step.

1. `mmPsi2Bound` fast vs reference vs fine scan (WP1).
2. Cold path: new output vs baseline output on the synthetic chain and on the per-slice truth surfaces already in `test_essvi.cpp`, parameters within tolerance (WP2).
3. Warm vs cold: on a sequence of at least 100 perturbed snapshots of the synthetic chain, warm band loss within `1 + 1e-3` relative (or absolute tolerance in loss units) of cold, and `validate()` passes on every snapshot.
4. Dense-grid arbitrage scan (k from -0.5 to 0.5 in 0.01 steps per expiry, as the scorer does): zero butterfly and calendar violations for cold and warm outputs.
5. Determinism: repeated runs give identical output.
6. Failure behavior: infeasible input returns `converged=false` and a non-empty error, never a fallback surface.

## 7. Data for benchmarks

- Synthetic chains from the generator (seeded), sizes: 10 x 50, 40 x 100, 60 x 150.
- Real snapshots: a small Python adapter (outside the core library) converts the IBIT snapshots on vps1 (`~/vol-rv/data/ibit/`) to the `normalized_quotes.csv` contract. For speed benchmarking, bid/mid/ask implied vols from a Black-Scholes European inversion are acceptable as a stand-in for American implied vols; label this in the results, because it does not validate pricing accuracy.
- The CVI dataset referenced in the README is not on vps1.

## 8. Process and constraints

- Work on a branch (`perf/essvi-calibrator`), small commits, one work package per commit group; tests before implementation for WP1, WP3.
- No sudo. Install tools in a venv. Compiler is g++ 15.2; default flags `-O2`; make `-O3 -march=native` an opt-in CMake option so benchmark numbers are labelled by flags.
- Report timings from a quiet machine: record the load average, and avoid the minutes when the cron collectors run (every 5 minutes, up to about 2 minutes each).
- Do not change the Python benchmark pipeline or the CVI comparison.
- Deliverables: code, tests, `docs/perf-baseline.md` (before) and `docs/perf-results.md` (after, with the same harness and flags), and a PR description with the table from section 1 filled in with measured numbers.

## 9. Risks

- Warm start can settle in a different local minimum because the band loss is flat inside the band; quality gates and periodic cold checks mitigate this.
- Floating-point changes from reordering can alter tie-breaking; tests compare within tolerance, not bit-for-bit, except where stated.
- A fast `mmPsi2Bound` that overestimates the infimum would admit butterfly arbitrage; the safety requirement in WP1 is the key test.
- Threading (WP5) can add nondeterminism if reduction order is not fixed.

## 10. Open questions for Yuan

1. Is a new branch and PR on `zhangys2/ql-essvi` fine, or should the work stay local to vps1?
2. Is the warm-start API shape (`calibrateWarm(quotes, previous, options)`) acceptable, or should `calibrate()` take an optional previous surface?
3. Should WP5 (threads) be allowed, given 4 vCPUs shared with the collectors?
