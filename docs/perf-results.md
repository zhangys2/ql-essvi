# Calibrator speedup results

WP0-WP4 measurements were recorded on 2026-10-15; the controlled WP5 remeasurement and WP6-WP11 results are recorded below. Baseline is recorded in [`perf-baseline.md`](perf-baseline.md); implementation branch: `perf/essvi-calibrator`.

## WP0-WP4 environment and method

- CPU: AMD EPYC 9354P, 4 vCPUs exposed to a KVM guest; Linux 7.0.0-34-generic, x86_64.
- Compiler: GCC/G++ 16.2.0; CMake 4.4.4; C++20 Release, effective `-O2`, no `-march=native`.
- Load average during the 40 × 100 run: `1.20 1.47 1.52` (1/5/15 minute). Single-slice run: `1.35 1.31 1.42`.
- Same deterministic synthetic chain as the baseline: 40 expiries × 100 quotes; seven repeated cold runs after the first cold call.
- Warm run: 100 updates; each update moves included bid/ask IV by `1e-7` from the prior snapshot (`quote_tol=1e-6`, `loss_tol=1e-6`, round-robin cold-check interval 10). The periodic cold check searches one expiry per scheduled update; no changed slice required refitting in this sequence.
- Hardware `perf` counters remain unavailable (`perf_event_paranoid=4`); the compile-time MM-bound timer was used for profiling.

## WP0-WP4 results

| Metric | Target | Hard limit | Measured | Status |
|---|---:|---:|---:|---|
| Cold full chain, speedup vs WP0 baseline | ≥10× | ≥5× | 13.76× (`1.25889 s` → `0.09151 s`) | Met |
| Warm full-chain update, p99 | ≤3 ms | ≤10 ms | 2.797 ms | Met |
| Single-slice cold, 100 quotes | ≤1 ms | ≤5 ms | 3.910 ms | Target missed; hard limit met |
| Cold parameters vs WP0 fixture, max relative error | ≤1e-9 | ≤1e-6 | `theta` 0; `psi` 4.21e-16; `rho` 0 | Met |
| Dense-grid butterfly violations | 0 | 0 | 0 | Met |
| Dense-grid calendar violations | 0 | 0 | 0 | Met |

The dense-grid scorer ran on the C++ CLI output: 4,000 scored quotes and 4,040 surface-grid points. Warm p99 includes the configured one-slice periodic cold searches; fallback tests separately force a cold fallback on changed quotes.

The full-chain profile build recorded 98,620 MM-bound/search-hint calls and 0.08450 s inside those calls out of 0.11518 s instrumented wall time (73.4%). The uninstrumented full-chain cold result was 0.09151 s. Candidate/objective evaluations remained 48,158; objective was 0.

## WP5 bounded rho parallelism

CMake option `ESSVI_MAX_CALIBRATION_THREADS` defaults to 4 and accepts 1–4. The limit includes the calling thread. A per-calibration worker pool is reused across rho windows; each rho writes to its own result slot, then reduction follows original rho order with strict `<` comparisons. Single-expiry requests stay serial to avoid worker startup overhead; multi-expiry calls use the bounded pool. The public `calibrate()` signature and behavior are unchanged.

Remeasured in Release `-O2` on the same 4-vCPU host with GCC/G++ 15.2.0. The serial comparison uses `ESSVI_MAX_CALIBRATION_THREADS=1`; the parallel build uses `=4`. Both runs use 40 × 100 synthetic quotes, 9 repeated cold runs after the first, and 100 warm updates with cold-check interval 10.

| Metric | 4-thread cap | 1-thread cap | Comparison |
|---|---:|---:|---:|
| First cold 40 × 100 | 82.956 ms | 114.366 ms | 1.38× faster |
| Repeated cold median | 64.927 ms | 91.954 ms | 1.42× faster |
| Repeated cold p99 | 74.165 ms | 103.371 ms | — |
| Warm update p99 | 2.944 ms | 3.004 ms | 4-thread result meets 3 ms target |
| Objective evaluations | 48,158 | 48,158 | unchanged |
| MM-bound calls | 98,620 | 98,620 | unchanged |

The 4-thread cold time is 15.18× the historical WP0 time (1.25889 s), but that cross-version comparison uses GCC 15.2.0 here versus GCC 16.2.0 for WP0. The same-compiler serial/parallel comparison above is the controlled WP5 result. Serial and parallel parameter CSVs compared byte-for-byte identical.

Single-expiry, 100-quote latency in the 4-thread-cap build (serial scheduling): first call 3.348 ms; repeat median 2.280 ms, p90 2.653 ms, p99 4.216 ms, max 4.583 ms. This remains above the 1 ms target but below the 5 ms hard limit. The cutoff avoids the worse tail observed when starting workers for a one-slice job.

## Implementation and verification

- WP0: `essvi_core` builds without QuantLib; the QuantLib adapter is optional and disabled when the package is absent. Baseline fixture and benchmark are reproducible.
- WP1: `mmPsi2Bound` uses per-rho workspaces, coarse-valley detection, and local refinement with downward slack. The original finite-grid implementation remains test-only as `mmPsi2BoundReference`. One million seeded random pairs and 4,096 fine-scan/local-refinement samples pass. A candidate-search-only finite-grid hint preserves the legacy search envelope; all accepted candidates still pass the refined conservative bound.
- WP2: quote log-moneyness and bid/ask total-variance bounds are prepared once per slice; per-evaluation quote loops allocate nothing. Calendar terms and per-slice bound results are memoized. `tests/baseline_essvi_40x100.csv` confirms cold parameters remain within `1e-9` relative (measured max `4.21e-16`).
- WP3: the dedicated `calibrateWarm(current_quotes, previous_surface, previous_quotes, options)` API reuses unchanged slices, searches locally around prior parameters for changed slices, checks predecessor compatibility, and cold-falls back on a failed quality gate. Periodic cold checks rotate through expiries; `update_number` makes the schedule deterministic.
- WP4: `CalibrationResult` exposes stage times, objective evaluations, MM-bound calls, refit/reuse counts, and cold/warm/fallback path. `essvi_bench --timings-out <path>` writes JSON without changing `diagnostics.csv` headers.
- WP5: rho candidates are independent, the fixed-order reduction is deterministic, and lazily initialized shared rho workspaces use `std::call_once`. Bound-call counts use per-thread local counters and are aggregated after each window.
- CTest: 4/4 passed in both max-thread configurations (1 and 4), including the million-sample bound test, cold fixture comparison, 100 warm updates, deterministic repeated results, cold fallback, failure behavior, and dense-grid call convexity/calendar checks. ThreadSanitizer passed the cold regression and warm calibrator tests.
- End-to-end smoke: `essvi_fit` plus `python/score_outputs.py` on a 5-quote, one-expiry input; all 5 quotes scored, 101 grid points, 0 butterfly and 0 calendar violations.

## WP0-WP5 PR description table

| Metric | Result |
|---|---:|
| Cold 40 × 100, 4-thread-cap vs 1-thread-cap | 1.38× first-call; 1.42× repeated median |
| Cold 40 × 100 vs historical WP0 | 15.18× (cross-compiler comparison) |
| Warm p99 (100 updates, periodic interval 10) | 2.944 ms |
| Cold single-slice latency | 2.280 ms repeat median; target miss, hard limit met |
| Cold parameter difference, serial vs parallel | Byte-identical |
| Dense-grid butterfly/calendar violations | 0 / 0 |

## WP6-WP11 market-like warm path

The benchmark runs four seeded 40 × 100 scenarios for 200 consecutive updates. It reports warm/cold p50, p90, p99, and max, path/refit/reuse counts, failures, dense-grid validity, summed band losses, local-solver work, and load averages before/after each run. The result below used seed 1 and cold-check interval 10 on the 4-vCPU GCC 15.2 Release `-O2` host. It measures the WP9 loss-scale gate and final WP10 calendar-repair eligibility. Load averages are 1/5/15-minute values; the host load rose over the run, so absolute timings are noisy. Scenario changes are cumulative IV changes from the prior snapshot: quiet moves one tenth of expiries by ±0.0005; normal uses common level ±0.002 and tilt ±0.0005; fast uses level ±0.01 and tilt ±0.002; front moves the first quarter by ±0.004 and the rest by ±0.0005 with a shared direction. Small per-expiry and per-quote noise are seeded. Fast/front idiosyncratic movements are kept small and group shocks correlated to avoid the independently moving-expiry failure mode described in the addendum.

| Scenario | Load average before → after | Warm p50 / p90 / p99 / max (ms) | Cold p50 / p90 / p99 / max (ms) | Warm / cold summed band loss | Loss ratio | Outcome |
|---|---|---:|---:|---:|---:|---|
| quiet | 0.69 0.96 1.19 → 1.42 1.11 1.24 | 0.416 / 1.631 / 3.641 / 4.817 | 67.037 / 84.080 / 100.792 / 121.022 | 0.0185610 / 0.0105839 | 1.754 | 3 ms p99 target missed narrowly; 10 ms hard limit met; 5% loss target missed |
| normal | 1.42 1.11 1.24 → 1.72 1.19 1.26 | 1.035 / 15.568 / 50.275 / 66.863 | 67.853 / 88.032 / 137.072 / 169.460 | 0.0706105 / 0.00124384 | 56.768 | 10 ms p99, 1% fallback, and 5% loss targets missed |
| fast | 1.72 1.19 1.26 → 2.65 1.46 1.35 | 64.552 / 88.669 / 356.272 / 518.384 | 67.805 / 91.745 / 202.207 / 338.648 | 3.00897 / 2.91347 | 1.033 | 20 ms p99 and 10% fallback targets missed; 5% loss target met |
| front | 2.65 1.46 1.35 → 3.55 1.74 1.44 | 1.156 / 12.515 / 24.418 / 66.208 | 71.174 / 89.480 / 159.405 / 338.759 | 0.0580503 / 0.000373925 | 155.246 | 3 ms p99 and 10 ms hard limit missed; 5% loss target missed |

All four runs had 0 warm failures, 0 cold failures, and 0 invalid warm/cold surfaces in the 101-point dense-grid scan per expiry. The loss ratio is summed warm band loss divided by summed cold band loss. Only fast is within the 5% loss criterion. Quiet misses the 3 ms target but stays below the 10 ms hard limit. Normal misses latency, fallback, and loss targets. Fast misses latency and fallback targets but meets the loss criterion. Front misses the latency target and 10 ms hard limit, as well as the loss criterion.

| Scenario | Mean refit / reuse slices (of 40) | Fallback updates / 200 | Fallback slices | Local-fit attempts | Local objective evals / attempt | Local MM-bound calls / attempt |
|---|---:|---:|---:|---:|---:|---:|
| quiet | 0.01 / 39.99 | 1 (0.5%) | 1 | 2 | 2.50 | 3.50 |
| normal | 5.53 / 34.47 | 46 (23%) | 378 | 1273 | 4.48 | 4.05 |
| fast | 39.99 / 0.01 | 193 (96.5%) | 7178 | 11678 | 3.02 | 2.95 |
| front | 4.52 / 35.48 | 61 (30.5%) | 289 | 1012 | 4.94 | 3.95 |

| Scenario | Quality checks | Checked warm loss | Checked cold reference loss | Checked band-loss scale | `loss_tol` |
|---|---:|---:|---:|---:|---:|
| quiet | 21 | 0.000157117 | 0.00000535550 | 0.0179986 | 0.05 |
| normal | 183 | 0.0416918 | 0.00786465 | 0.413964 | 0.05 |
| fast | 3589 | 3.82786 | 1.65309 | 3.25694 | 0.05 |
| front | 190 | 0.00687931 | 0.000494523 | 0.102199 | 0.05 |

The local solver stayed within WP8's 15-objective-evaluation budget for each local-fit attempt; the reported local MM-bound calls include initial validation and selected-candidate checks. WP10 sequential repair attempts are included in the local-fit counts. Full warm objective totals also include periodic cold quality checks and cold fallbacks.

WP7 reuses a previous slice only when it remains feasible against the predecessor and its new-quote band loss is within the width-scaled tolerance. WP8 uses scaled local Gauss-Newton steps, feasible backtracking, and a conservative MM-bound check. WP9 compares a checked warm candidate with the cold reference using `candidate_loss > cold.objective + loss_tol * bandLossScale`; the result records warm/reference losses, scale, tolerance, and fallback path. The user decision was to accept any feasible in-band surface, with no secondary mid-centering objective. No mid-centering term was added.

WP10 is implemented: changed slices are prefitted in parallel against previous-snapshot predecessors using the existing bounded pool (maximum four threads). A deterministic sequential pass checks `calendarSafe` against each newly selected predecessor and locally repairs only a candidate that fails that updated check. A failed Jacobi prefit goes directly to the cold fallback instead of being retried as calendar repair. Worker startup is avoided when no changed slice needs a local fit. Regression coverage exercises sequential repair and a failed prefit with a changed predecessor; the latter records two local attempts (the two Jacobi prefits) without an extra retry. The measured misses are explicit: quiet misses the loss criterion; normal misses latency, fallback, and loss; fast misses latency and fallback but meets the loss criterion; front misses latency, hard-limit, and loss targets; the single-slice hard limit is also missed.

WP11's candidate-search hint scans the precomputed coarse MM-bound workspace directly; every accepted slice still passes the refined conservative bound. On the current binary, single-slice cold latency (100 quotes, 20 repeats) measured 3.909 ms first call; repeat median 4.384 ms, p90 5.507 ms, p99 5.928 ms, max 5.981 ms. The 1 ms target and 5 ms hard limit are missed. Load average was 1.65 1.49 1.39 before and after.

### Replay and verification artifacts

The current IBIT conversion smoke read three snapshots from the configured data directory. The first had 0 valid two-sided OTM quotes and was skipped; the next two produced 498 and 495 valid quotes. Replay on the current binary used those two converted snapshots: one warm transition, 20 refitted/0 reused slices, 29 local-fit attempts, 5 fallback slices, 0 warm/cold failures, and 0 invalid surfaces. The final replay smoke measured 20.356 ms warm vs 25.850 ms cold; summed loss was 0.000594771 vs 0.001043726 (ratio 0.570). Load average was 1.05 1.33 1.34 before and after. Timing JSON reports `updates: 1` for the one adjacent-snapshot transition; the full scenario report records `updates: 200`. This is a plumbing/performance sample only; the converter uses European Black-Scholes stand-ins, not American-IV accuracy evidence. Replay, scenario, single-slice, and legacy timing JSON files parsed successfully.

Final verification on the current Release tree: full CTest 4/4 passed (25.04 s); Python pytest 5/5 passed (0.52 s). The focused warm test also passed under ThreadSanitizer (4-thread cap, 7.90 s) and Release max-thread cap 1 (0.66 s). The legacy bench smoke (`--repeats 2 --warm-updates 3`) reused 40/40 slices and emitted parseable JSON; warm p99 was 0.685 ms. The full WP6-WP11 scenario benchmark includes replay-free synthetic data; replay results are reported separately above.

## WP-B1 — `49fed2b` baseline and per-quote quality

The WP-B1 benchmark instrumentation was run against the approved base, `49fed2bd0a31f1df60ffc370f8af82978d1b51d0`. The implementation branch started at `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536`; its tree matches `49fed2b`. CAL-B1 changed only benchmark reporting, tests, and runner scripts; the calibrator and loss-scale gate were not modified. These measurements are from the pre-WP-B2 gate, not the new per-quote gate.

The Release build used C++20, GCC 15.2.0, effective `-O2`, and the 4-vCPU AMD EPYC 9354P host. `scripts/verify_scenarios.sh` ran quiet, normal, fast, and front for seeds 1–5, 200 updates per scenario/seed, 40 expiries × 100 quotes, with the existing `loss_tol=0.05` and cold-check interval 10. Scenario amplitudes remain the original synthetic values; WP-B7 will recalibrate them from collected snapshots. Timers cover only warm/cold calibration calls. The runner completed all 20 cases serially. Every `--check` returned 2 because at least one target was missed; fast seeds 2 and 3 also failed full paired-update coverage (197/200 and 196/200 respectively). The script returned 1 after printing the complete matrix; this was an expected quality/coverage failure, not an execution failure.

Warm/cold quality metrics are merged only when both valid results cover the complete valid two-sided quote set for the same update. Thus each comparison uses matching quote populations. The run included 3,993 paired updates of 4,000 total: the 18 other scenario/seed runs had 200 paired updates each, while fast seeds 2 and 3 had 197 and 196. Unpaired updates are reported and make `--check` fail. Values below are five-seed median [minimum, maximum]. The cap is 0.1; loss is the ratio of summed per-quote band loss, not the existing width-scaled gate value.

| Scenario | Paired updates / 200 | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | Share above cap delta (pp) | Maximum ratio warm / cold | RMS ratio warm / cold | Summed band-loss ratio | RMS excess-IV delta (vol pt) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| quiet | 200 [200, 200] | 0.413031 [0.404705, 0.496840] | 4.99531 [3.71924, 6.79089] | 0 [0, 1.5] | 3.00838 [1.28625, 7.22463] | 0.328828 [0.254271, 0.474417] / 0.240286 [0.137301, 0.294707] | 0.0340939 [0.0215308, 0.0546441] / 0.00708899 [0.00338581, 0.0152022] | 16.3978 [1.75370, 72.3258] | 0.0196046 [0.0112145, 0.0491839] |
| normal | 200 [200, 200] | 1.03759 [0.948927, 1.32622] | 54.7095 [46.1574, 67.3341] | 22.5 [20.5, 29.5] | 9.96412 [8.41425, 11.36] | 0.744533 [0.711175, 0.891100] / 0.246743 [0.151293, 0.473936] | 0.0710031 [0.0665109, 0.0788025] / 0.00554696 [0.00291107, 0.0174514] | 53.8362 [10.9119, 112.573] | 0.0536984 [0.0464304, 0.0590196] |
| fast | 200 [196, 200] | 54.0474 [30.6489, 65.0477] | 117.488 [74.9509, 235.313] | 86.5 [80.5, 96.5] | 1.24237 [0.747625, 2.64872] | 4.69974 [0.778388, 7806.4] / 4.70701 [0.187145, 7750.37] | 0.433228 [0.0386822, 54.5329] / 0.419033 [0.00178535, 54.195] | 1.10881 [0.996895, 299.499] | 0.0135374 [0.00586116, 0.0300491] |
| front | 200 [200, 200] | 1.19720 [1.00284, 11.5672] | 34.5177 [25.7381, 68.1123] | 30.5 [25, 35.5] | 7.17750 [5.05813, 10.1587] | 2.12641 [0.842882, 8.85910] / 2.58437 [0.665395, 9.39147] | 0.0707055 [0.0562263, 0.812919] / 0.0527643 [0.00998099, 0.835272] | 2.22556 [0.901585, 155.246] | 0.0121359 [-0.0239676, 0.0423118] |

All 20 runs had zero warm/cold failures, zero invalid warm/cold surfaces, and zero dense-grid butterfly/calendar violations. Quiet p50 median stayed below the `49fed2b` baseline of 0.47 ms, but seed 2 measured 0.496840 ms; quiet p99 stayed below the 7.6 ms baseline in all five seeds. The stricter quiet p99 target of 3 ms was missed in all five seeds. Normal, fast, and front missed their p99 and fallback targets in all five seeds; each also breached the corresponding hard limits. The quality target misses are reported per seed in the `--check` output. The summary medians do not determine individual pass/fail status.

The old gate and new per-quote measurements are reported side by side. Old-gate fields are medians [ranges] across seeds; checked losses and scale aggregate only scheduled quality-check events. The fallback-update percentage in the primary table is the existing loss-scale gate's resulting fallback-path rate.

| Scenario | Old-gate checks | Checked warm loss | Cold reference loss | Band-loss scale | `loss_tol` |
|---|---:|---:|---:|---:|---:|
| quiet | 20 [20, 23] | 4.26242e-05 [8.68514e-06, 0.000957942] | 9.23326e-11 [0, 0.000150919] | 0.0152508 [0.0151561, 0.0306513] | 0.05 |
| normal | 180 [158, 220] | 0.0490752 [0.0416918, 0.0544085] | 0.00933619 [0.00582524, 0.0179343] | 0.483858 [0.413964, 0.510053] | 0.05 |
| fast | 2546 [1862, 3589] | 3.82786 [2.66946, 4.53582] | 0.251244 [0.000136121, 1.89970] | 4.49517 [2.97981, 9.291] | 0.05 |
| front | 190 [103, 836] | 0.0157761 [0.00422110, 3.55787] | 0.00872655 [0.000127251, 3.36029] | 0.102199 [0.0657856, 0.864477] | 0.05 |

Load averages are 1/5/15-minute values, before → after each run. The sequence was serial (quiet, normal, fast, front); host load was elevated and changed during the run, so absolute timings are noisy.

| Scenario | Seed | Load average before → after |
|---|---:|---|
| quiet | 1 | 1.34 1.16 1.08 → 2.25 1.39 1.16 |
| quiet | 2 | 2.25 1.39 1.16 → 3.27 1.70 1.27 |
| quiet | 3 | 3.27 1.70 1.27 → 3.52 1.83 1.32 |
| quiet | 4 | 3.52 1.83 1.32 → 4.19 2.09 1.42 |
| quiet | 5 | 4.19 2.09 1.42 → 4.39 2.23 1.48 |
| normal | 1 | 4.39 2.23 1.48 → 4.42 2.35 1.53 |
| normal | 2 | 4.42 2.35 1.53 → 4.11 2.38 1.55 |
| normal | 3 | 4.11 2.38 1.55 → 3.92 2.43 1.58 |
| normal | 4 | 3.92 2.43 1.58 → 3.73 2.48 1.62 |
| normal | 5 | 3.73 2.48 1.62 → 4.33 2.67 1.69 |
| fast | 1 | 4.33 2.67 1.69 → 4.45 2.86 1.79 |
| fast | 2 | 4.45 2.86 1.79 → 4.31 2.96 1.85 |
| fast | 3 | 4.31 2.96 1.85 → 4.76 3.15 1.94 |
| fast | 4 | 4.76 3.15 1.94 → 5.05 3.34 2.03 |
| fast | 5 | 5.05 3.34 2.03 → 5.03 3.47 2.11 |
| front | 1 | 5.03 3.47 2.11 → 5.01 3.55 2.16 |
| front | 2 | 5.01 3.55 2.16 → 4.92 3.61 2.21 |
| front | 3 | 4.92 3.61 2.21 → 5.41 3.81 2.31 |
| front | 4 | 5.41 3.81 2.31 → 4.94 3.79 2.32 |
| front | 5 | 4.94 3.79 2.32 → 4.75 3.80 2.35 |
