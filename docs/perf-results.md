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

## WP-B7 — Replay-calibrated scenarios and full-session replay

### Move estimator and data coverage

The replay command was `python/run_snapshot_replay.py --data-root /home/zhangy/vol-rv/data --output-dir build/snapshot-replay --bench build/essvi_bench`. It read IBIT `ibit/chain_*.jsonl.gz` and SPY `zdte/spy_*.jsonl.gz`. Quotes were converted to European Black-Scholes mid implied volatility, matched across consecutive five-minute snapshots by expiry and contract identity, then fitted per expiry with a Theil-Sen regression of IV change against mean log-moneyness. The intercept is the common level move; half the slope is the tilt move for the benchmark's `clamp(2*k)` shape; residual variation estimates per-quote noise. Quantiles below are absolute moves.

Pooled quantiles give equal weight to each asset and session day; observations within an asset-day share that day's weight equally. This prevents IBIT's denser chain from dominating SPY. The final capture contained 620 snapshots (619 usable, one unusable), covering 10 usable asset-days (five IBIT and five SPY). It produced 6,199 level/tilt observations and 164,009 residual observations. Same-session expiries use a fixed one-day date-based year fraction so expiry identity is stable across intraday snapshots. The current 2026-10-16 files were partial when captured.

| Asset | Measure | Observations | p10 | p50 | p95 |
|---|---|---:|---:|---:|---:|
| IBIT | Level | 5,882 | 0.000046521 | 0.000467756 | 0.003376293 |
| IBIT | Tilt | 5,882 | 0.000379699 | 0.003578760 | 0.054064609 |
| IBIT | Residual noise | 155,064 | 0.000040666 | 0.001008464 | 0.007305616 |
| SPY | Level | 317 | 0.000065178 | 0.000523359 | 0.002172665 |
| SPY | Tilt | 317 | 0.005496195 | 0.065444918 | 0.233002777 |
| SPY | Residual noise | 8,945 | 0.000048059 | 0.000749564 | 0.003841574 |

| Pooled measure | Observations | p10 | p50 | p95 |
|---|---:|---:|---:|---:|
| Level | 6,199 | 0.000057870 | 0.000485901 | 0.002928020 |
| Tilt | 6,199 | 0.000731869 | 0.013646118 | 0.170740395 |
| Residual noise | 164,009 | 0.000044357 | 0.000863814 | 0.005561586 |

Quiet uses p10, normal p50, fast p95. The measured front/back level ratio is 2.805297395; the front scenario uses the normal median for back expiries and scales front expiries by this ratio. No unmeasured per-expiry idiosyncratic amplitude is added.

| Scenario | Level move | Tilt move | Residual quote noise | Front level / back level |
|---|---:|---:|---:|---:|
| quiet | 0.000057870 | 0.000731869 | 0.000044357 | — |
| normal | 0.000485901 | 0.013646118 | 0.000863814 | — |
| fast | 0.002928020 | 0.170740395 | 0.005561586 | — |
| front | 0.001363098 | 0.013646118 | 0.000863814 | 2.805297395 |

### Full-session replay coverage

Replay retained each source timestamp and excluded locked/crossed quotes only in this benchmark adapter; the general preparation/CVI path is unchanged. Per-snapshot exclusion counts are in `build/snapshot-replay/replay_manifest.tsv`. Across the capture: 16 locked, 0 crossed, and 228,055 other quote exclusions. The unusable IBIT 2026-10-10 snapshot had no valid quotes. There were 601 consecutive warm pairs across the 10 usable asset-days, 18 cold starts (10 session starts plus 8 gap restarts), 8 gap events, and 16 missing five-minute intervals. Skipped/unusable snapshots and timestamp gaps break the chain and force a cold start.

| Asset | Session | Snapshots | Usable / skipped | Warm pairs | Cold starts | Gap events | Missing intervals | Locked | Crossed | Other exclusions |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| IBIT | 2026-10-10 | 1 | 0 / 1 | 0 | 0 | 0 | 0 | 0 | 0 | 50 |
| IBIT | 2026-10-12 | 60 | 60 / 0 | 51 | 9 | 8 | 16 | 2 | 0 | 41,653 |
| IBIT | 2026-10-13 | 76 | 76 / 0 | 75 | 1 | 0 | 0 | 2 | 0 | 51,377 |
| IBIT | 2026-10-14 | 76 | 76 / 0 | 75 | 1 | 0 | 0 | 1 | 0 | 52,978 |
| IBIT | 2026-10-15 | 76 | 76 / 0 | 75 | 1 | 0 | 0 | 0 | 0 | 54,903 |
| IBIT | 2026-10-16 | 9 | 9 / 0 | 8 | 1 | 0 | 0 | 0 | 0 | 6,420 |
| SPY | 2026-10-12 | 76 | 76 / 0 | 75 | 1 | 0 | 0 | 1 | 0 | 5,316 |
| SPY | 2026-10-13 | 78 | 78 / 0 | 77 | 1 | 0 | 0 | 1 | 0 | 5,561 |
| SPY | 2026-10-14 | 78 | 78 / 0 | 77 | 1 | 0 | 0 | 7 | 0 | 4,758 |
| SPY | 2026-10-15 | 78 | 78 / 0 | 77 | 1 | 0 | 0 | 1 | 0 | 4,325 |
| SPY | 2026-10-16 | 12 | 12 / 0 | 11 | 1 | 0 | 0 | 1 | 0 | 714 |

### Per-day quality and cap sensitivity

These are pre-WP-B2 warm-path measurements. `Max W/C` is maximum excess ratio (warm/cold); `RMS W` is warm RMS excess ratio; loss is summed warm band loss divided by cold; RMS IV delta is warm minus cold in vol points. Each cap cell shows warm/cold share above cap as percent, then warm-minus-cold percentage points. The unusable IBIT 2026-10-10 row has no quality metrics because no fit was possible.

| Asset | Session | Warm p99 (ms) | Fallback updates (%) | Max W/C | RMS W | Loss W/C | RMS IV delta (vol pt) |
|---|---|---:|---:|---:|---:|---:|---:|
| IBIT | 2026-10-12 | 56.879 | 98.04 | 37.792 / 28.536 | 4.725 | 1.367 | 0.719 |
| IBIT | 2026-10-13 | 216.066 | 92.00 | 68.765 / 64.134 | 2.693 | 0.554 | -0.300 |
| IBIT | 2026-10-14 | 87.138 | 90.67 | 32.453 / 19.308 | 2.532 | 0.511 | -0.036 |
| IBIT | 2026-10-15 | 86.135 | 84.00 | 22.887 / 20.038 | 2.557 | 0.643 | -0.341 |
| IBIT | 2026-10-16 | 58.853 | 87.50 | 22.729 / 11.043 | 2.147 | 0.509 | 0.096 |
| SPY | 2026-10-12 | 7.523 | 20.00 | 8.557 / 8.148 | 2.050 | 1.018 | 0.029 |
| SPY | 2026-10-13 | 10.880 | 22.08 | 10.164 / 9.634 | 2.067 | 1.001 | 0.030 |
| SPY | 2026-10-14 | 5.487 | 3.90 | 7.928 / 6.106 | 1.890 | 0.764 | 0.059 |
| SPY | 2026-10-15 | 3.867 | 6.49 | 11.382 / 11.093 | 3.342 | 0.903 | 0.059 |
| SPY | 2026-10-16 | 6.681 | 0.00 | 6.745 / 5.826 | 2.119 | 0.793 | 0.070 |

| Asset | Session | Cap 0.05: warm/cold %, Δpp | Cap 0.1: warm/cold %, Δpp | Cap 0.2: warm/cold %, Δpp |
|---|---|---:|---:|---:|
| IBIT | 2026-10-12 | 77.64 / 73.38, +4.26 | 75.27 / 71.05, +4.23 | 70.78 / 66.70, +4.09 |
| IBIT | 2026-10-13 | 76.71 / 74.07, +2.64 | 74.45 / 72.01, +2.44 | 70.07 / 67.30, +2.77 |
| IBIT | 2026-10-14 | 77.27 / 74.52, +2.75 | 75.26 / 72.09, +3.17 | 70.81 / 67.75, +3.05 |
| IBIT | 2026-10-15 | 78.95 / 76.49, +2.46 | 76.71 / 74.40, +2.31 | 72.25 / 70.43, +1.82 |
| IBIT | 2026-10-16 | 78.52 / 73.66, +4.86 | 76.02 / 71.41, +4.61 | 71.25 / 67.14, +4.11 |
| SPY | 2026-10-12 | 79.63 / 80.03, -0.40 | 76.90 / 76.61, +0.29 | 72.22 / 71.71, +0.51 |
| SPY | 2026-10-13 | 78.77 / 80.13, -1.36 | 77.47 / 77.05, +0.41 | 71.97 / 71.56, +0.41 |
| SPY | 2026-10-14 | 74.44 / 76.10, -1.66 | 71.50 / 73.49, -1.99 | 67.32 / 69.14, -1.82 |
| SPY | 2026-10-15 | 79.89 / 80.98, -1.09 | 78.02 / 78.48, -0.46 | 74.63 / 74.17, +0.46 |
| SPY | 2026-10-16 | 77.07 / 74.31, +2.76 | 75.14 / 70.99, +4.14 | 69.34 / 68.23, +1.10 |

Every replay session reported zero warm/cold failures, zero invalid surfaces, and zero dense-grid butterfly/calendar violations. The large IBIT fallback rates and quality differences are measured results from the old gate, not acceptance passes.

### Five-seed scenario verification

The recalibrated synthetic scenarios were checked with `scripts/verify_scenarios.sh`: quiet, normal, fast, and front; seeds 1–5; 200 updates per run; 40 expiries × 100 quotes. All 20 runs completed and emitted their target/hard-limit tables and load averages. All 20 `--check` invocations returned 2 for target misses; the script returned 1 after summarizing, with no execution failure. The results below are median [minimum, maximum]. Quiet p50 median was below the 49fed2b 0.47 ms baseline, but its range includes regressions; quiet p99 median exceeded the 7.6 ms baseline. Quiet's summed-loss ratio is unavailable because cold reference loss was zero in some seeds. Fast seed 5 had 73 unpaired/unscored updates and 67 warm failures.

| Scenario | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | Cap-share delta (pp) |
|---|---:|---:|---:|---:|
| quiet | 0.414886 [0.392560, 0.580925] | 20.6996 [2.79764, 39.8506] | 1.5 [0, 5] | 2.60587 [0.297, 4.36013] |
| normal | 77.2783 [60.3902, 81.8564] | 143.287 [85.1124, 282.596] | 100 [100, 100] | 1.21513 [-0.563125, 2.64525] |
| fast | 38.528 [30.5705, 51.5613] | 89.6673 [66.3265, 242.096] | 99.5 [98.5, 100] | 0.0301802 [-0.000225225, 0.105224] |
| front | 74.9967 [66.174, 77.1202] | 291.366 [110.198, 402.263] | 100 [100, 100] | 1.1475 [-0.646, 2.66612] |

| Scenario | Maximum ratio warm / cold | RMS ratio warm / cold | Summed band-loss ratio | RMS excess-IV delta (vol pt) |
|---|---:|---:|---:|---:|
| quiet | 0.616787 [0.319872, 0.680804] / 0.0121983 [0, 0.0325625] | 0.0338438 [0.0123335, 0.0460532] / 1.79078e-05 [0, 4.48450e-05] | unavailable (zero cold reference) | 0.0358302 [0.0134186, 0.0500544] |
| normal | 84.6709 [5.00511, 25,374.1] / 83.4915 [4.24346, 24,478.9] | 2.24823 [0.423733, 1,602.34] / 2.34549 [0.421754, 1,601.57] | 1.14520 [0.902552, 1.22732] | 0.0368503 [-0.0659836, 0.0977803] |
| fast | 6.20630e6 [3.92477e6, 8.77257e6] / 6.20630e6 [3.91312e6, 8.77257e6] | 389,897 [227,658, 526,315] / 392,176 [224,946, 526,448] | 1.00137 [0.999922, 1.00435] | 0.332433 [0.150394, 0.480907] |
| front | 394.507 [4.19593, 25,671.2] / 420.478 [4.19519, 25,239.1] | 2.35910 [0.432090, 1,549.75] / 2.45847 [0.430492, 1,568.86] | 1.14804 [0.901853, 1.24268] | 0.0359792 [-0.0655997, 0.0927253] |

The current pre-B2 gate misses Addendum B quality/latency targets in all four scenario families. Quiet median latency remains near baseline, but p99 non-regression does not hold; normal and front have 100% fallback updates; fast has extreme per-quote ratios and unpaired/failure cases despite its low median cap-share delta. These are baseline issues for WP-B2/B3/B4, not hidden passes.

| Scenario | Seed | Load average before → after (1/5/15 min) |
|---|---:|---|
| quiet | 1 | 1.83 2.80 2.56 → 2.10 2.81 2.57 |
| quiet | 2 | 2.10 2.81 2.57 → 2.48 2.87 2.59 |
| quiet | 3 | 2.48 2.87 2.59 → 2.68 2.89 2.60 |
| quiet | 4 | 2.68 2.89 2.60 → 2.59 2.86 2.59 |
| quiet | 5 | 2.59 2.86 2.59 → 2.89 2.92 2.62 |
| normal | 1 | 2.89 2.92 2.62 → 3.44 3.07 2.68 |
| normal | 2 | 3.44 3.07 2.68 → 4.43 3.33 2.78 |
| normal | 3 | 4.43 3.33 2.78 → 4.58 3.49 2.86 |
| normal | 4 | 4.58 3.49 2.86 → 4.61 3.61 2.92 |
| normal | 5 | 4.61 3.61 2.92 → 4.20 3.59 2.93 |
| fast | 1 | 4.20 3.59 2.93 → 3.91 3.57 2.94 |
| fast | 2 | 3.91 3.57 2.94 → 3.57 3.51 2.93 |
| fast | 3 | 3.57 3.51 2.93 → 3.46 3.49 2.93 |
| fast | 4 | 3.46 3.49 2.93 → 3.85 3.58 2.97 |
| fast | 5 | 3.85 3.58 2.97 → 3.50 3.51 2.97 |
| front | 1 | 3.50 3.51 2.97 → 3.51 3.52 2.98 |
| front | 2 | 3.51 3.52 2.98 → 3.98 3.63 3.04 |
| front | 3 | 3.98 3.63 3.04 → 3.93 3.66 3.07 |
| front | 4 | 3.93 3.66 3.07 → 5.25 4.04 3.22 |
| front | 5 | 5.25 4.04 3.22 → 5.09 4.13 3.28 |

All real-data IVs are European Black-Scholes calibration-path stand-ins. The source quote bands are not verified consolidated NBBO, and the replay makes no American-pricing accuracy claim. The generated outputs are `build/snapshot-replay/move_measurements.json`, `replay_manifest.tsv`, `daily_replay_results.json`, and `daily_replay_quality.csv`.


## WP-B3 — Local solver and fallback reason diagnostics

The changed-slice solver now uses three deterministic starts within prior rho ±0.05, with feasible-psi clamping and an ATM-anchored theta start. It keeps a feasible warm incumbent, applies adaptive Levenberg–Marquardt damping, stops on exact zero loss, and shares a 25-objective-evaluation cap across incumbent and starts. Result and benchmark diagnostics expose local starts and four reason counters: infeasible local result, loss above gate, calendar repair failed, and bound check failed. Reason totals count rejected-candidate/fallback events, not distinct updates; a single update can contribute multiple events.

The focused warm test verifies all three starts, the evaluation cap, repeatable search diagnostics, bound-rejection behavior, all four reason categories, and a zero-loss fixture that stops after the incumbent plus one ATM start (2 evaluations). `scripts/verify_scenarios.sh` then completed all four scenarios × five seeds × 200 updates at 40 expiries × 100 quotes. Every invocation ran to completion; all 20 `--check` results were 2 for target misses and the script returned 1 after reporting the full matrix. That is a target failure, not an execution failure. Values below are median [minimum, maximum] over seeds; load averages are 1/5/15-minute values before → after each sequential run.

| Scenario | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | Cap-share delta (pp) | Maximum excess ratio warm / cold | RMS excess ratio warm / cold | Summed band loss warm / cold | RMS excess-IV delta (vol pt) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| quiet | 1.43041 [1.14823, 1.70749] | 69.5995 [58.7284, 115.405] | 25 [13, 30] | 0 [0, 0] | 0.0998562 [0.0993879, 0.0999863] / 0.0121983 [0, 0.0325625] | 0.00504428 [0.00401056, 0.00632181] / 1.79078e-05 [0, 4.4845e-05] | unavailable (zero cold loss in some seeds) | 0.00499885 [0.00393728, 0.00629424] |
| normal | 159.869 [150.107, 207.614] | 415.887 [233.892, 524.616] | 100 [100, 100] | 0.00275 [-0.016375, 0.014875] | 83.4903 [5.005, 24478.9] / 83.4915 [4.24346, 24478.9] | 2.34593 [0.422471, 1601.62] / 2.34549 [0.421754, 1601.57] | 1.00005 [0.99776, 1.00558] | 0.000132492 [-0.00158258, 0.00050315] |
| fast | 86.5161 [32.5042, 103.138] | 208.346 [130.766, 383.778] | 64.5 [37.5, 91] | 0 [0, 0] | 6.44294e+06 [3.91312e+06, 8.77257e+06] / 6.44294e+06 [3.91312e+06, 8.77257e+06] | 387276 [238316, 598838] / 387276 [238316, 598838] | 1 [1, 1] | 4.87485e-07 [0, 2.12791e-05] |
| front | 165.985 [139.775, 178.087] | 464.729 [240.038, 845.667] | 100 [100, 100] | -0.020375 [-0.024125, 0.01825] | 420.467 [4.19631, 25239] / 420.478 [4.19519, 25239.1] | 2.45865 [0.430232, 1568.86] / 2.45847 [0.430492, 1568.86] | 1.00016 [0.997374, 1.00027] | 8.89794e-05 [-0.000235588, 0.000511336] |

| Scenario | Infeasible local result | Loss above gate | Calendar repair failed | Bound check failed |
|---|---:|---:|---:|---:|
| quiet | 0 | 3,865 | 0 | 0 |
| normal | 0 | 118,177 | 0 | 330 |
| fast | 0 | 66,439 | 6,560 | 2,145,106 |
| front | 0 | 118,199 | 0 | 371 |

| Scenario | Seed | Load average before → after |
|---|---:|---|
| quiet | 1 | 1.26 1.86 1.76 → 1.94 1.98 1.81 |
| quiet | 2 | 1.94 1.98 1.81 → 3.06 2.24 1.90 |
| quiet | 3 | 3.06 2.24 1.90 → 2.87 2.24 1.90 |
| quiet | 4 | 2.87 2.24 1.90 → 3.12 2.33 1.94 |
| quiet | 5 | 3.12 2.33 1.94 → 2.79 2.31 1.94 |
| normal | 1 | 2.79 2.31 1.94 → 4.20 2.84 2.15 |
| normal | 2 | 4.20 2.84 2.15 → 5.25 3.43 2.40 |
| normal | 3 | 5.25 3.43 2.40 → 4.47 3.47 2.46 |
| normal | 4 | 4.47 3.47 2.46 → 3.17 3.28 2.45 |
| normal | 5 | 3.17 3.28 2.45 → 3.12 3.25 2.48 |
| fast | 1 | 3.12 3.25 2.48 → 2.92 3.19 2.48 |
| fast | 2 | 2.92 3.19 2.48 → 2.84 3.16 2.49 |
| fast | 3 | 2.84 3.16 2.49 → 2.72 3.11 2.49 |
| fast | 4 | 2.72 3.11 2.49 → 3.32 3.23 2.56 |
| fast | 5 | 3.32 3.23 2.56 → 3.55 3.29 2.59 |
| front | 1 | 3.55 3.29 2.59 → 3.96 3.47 2.69 |
| front | 2 | 3.96 3.47 2.69 → 3.21 3.34 2.70 |
| front | 3 | 3.21 3.34 2.70 → 3.46 3.37 2.73 |
| front | 4 | 3.46 3.37 2.73 → 2.93 3.25 2.72 |
| front | 5 | 2.93 3.25 2.72 → 4.17 3.57 2.87 |

The warm p99 target and fallback target are missed in every scenario; normal and front fallback on every update. Quiet misses the 0.47 ms median and 7.6 ms p99 non-regression reference, and its loss ratio is unavailable because some cold reference losses are zero. Fast seeds 2–5 reported 71, 126, 86, and 59 warm failures respectively; these runs also had unpaired/unscored updates. The benchmark reported zero invalid surfaces and zero dense-grid butterfly/calendar violations across the 20 runs. The 25-evaluation local-solver cap and quality gate remain covered by focused tests; the market-like scenario results show substantial latency/fallback and fast-path failure residuals for CAL-B4/B5 to address.

## WP-B4 — Bounded fallback and degraded results

Warm calibration now runs local → restricted-cold → full-cold stages with a deterministic limit of eight full-cold reference/fallback searches per update. Initial calibration without a usable prior result is not budgeted as warm fallback work. If the next full-cold request exceeds the budget, the calibrator returns the lowest-loss feasible candidate among the restricted fit, the current local/warm candidate, and the retained prior slice, after checking butterfly and current-predecessor calendar constraints. If no feasible candidate can complete the surface (for example, a new expiry after the budget is consumed), calibration returns a non-empty failure instead of a partial successful result.

A valid budget-exhausted surface has `path=degraded` and `converged=true`; the final surface still passes `validate()`. `degraded_ages` aligns with `surface.slices`: a slice failing the base per-quote cap increments its consecutive age, and an age resets to zero only when the returned slice passes that cap. `degraded_slices` counts nonzero ages. A slice with prior age ≥3 bypasses reuse/local work and is forced through a full-cold refit on the next update, even above the ordinary cap; the age resets only if that selected fit passes the gate. Local refits for pending slices are ordered by descending age, with expiry order as the deterministic tie-break. Benchmark output distinguishes fallback and degraded paths, counts both as fallback updates, and reports search counts, reason histograms, achieved ratio summaries, and final per-slice ages in JSON without changing shared CSV headers.

The approved high-fallback shortcut uses this exact threshold: if the previous result's fallback-slice count is greater than half its surface slices and the **mean absolute midpoint-IV move across matching included quotes** is at least `0.005561586`, skip local prefit and start at restricted cold. The threshold is the pooled fast-scenario p95 residual-noise move measured in WP-B7. The move metric matches quotes by expiry-group order and quote ID; unavailable matches do not contribute.

Focused warm tests cover local skipping under that shortcut, restricted-stage diagnostics, the eight-search limit, valid degraded surfaces and dense-grid safety, age carry-forward and forced full cold after three degraded outputs, age reset on a passing forced fit, and explicit failure when budget exhaustion prevents fitting a new expiry. The configured full-suite result is recorded in the CAL-B4 ticket.

Small output smoke (not the 40 × 100 / 200-update acceptance run): `build/essvi_bench --scenario fast --scenario-updates 5 --expiries 10 --quotes-per-expiry 100 --seed 11 --timings-out /tmp/cal-b4-smoke.json`. All five warm outputs converged with zero invalid surfaces and zero dense-grid violations; the JSON summary parsed successfully.

| Metric | Result |
|---|---:|
| Warm p50 / p99 | 16.303 / 34.225 ms |
| Warm fallback / degraded updates | 2 / 3 of 5 (100% combined) |
| Mean / maximum degraded slices | 10 / 10 |
| Maximum degraded age | 5 |
| Restricted / full-cold / forced full-cold searches | 30 / 44 / 20 |
| Budget-exhaustion reason count | 6 |
| Warm / cold summed band-loss ratio | 2.1203 |
| Warm max excess ratio / worst-slice RMS ratio | 21,942.002 / 5,343.792 |
| Warm failures / invalid surfaces / dense-grid violations | 0 / 0 / 0 |

This short run confirms bounded stage accounting and valid degraded-path reporting, not Addendum B performance acceptance. It reports 100% combined fallback updates and large per-quote excess ratios; the quality and latency gaps remain visible for CAL-B5's regression sequence.

## WP-B2 — Per-quote gate and reuse drift

The warm gate now evaluates each included two-sided quote independently in total variance: `ratio = max(0, bid_var - fit_var, fit_var - ask_var) / max(ask_var - bid_var, 1e-6)`. A candidate passes the base gate only when every ratio is at most `WarmOptions::quote_cap` (default 0.1). A failed reuse falls through to local fitting; a local candidate that fails the base gate is followed by a cold search restricted to prior rho ±0.15 over the feasible psi interval. If that restricted result passes the base cap, it is used. Otherwise one same-snapshot full-cold reference is cached per slice, and the restricted candidate is retained only when every quote is within `max(quote_cap, cold_ratio + 0.01)`; otherwise the full-cold result is selected. Reuse also requires the per-quote cap and calendar check; `CalibrationResult::reuse_ages` tracks unchanged-parameter reuse and forces refit after 12 consecutive updates. `calibrateWarm` now takes the prior `CalibrationResult` (nullable) so the surface and reuse state travel together. `loss_tol` remains a deprecated compatibility/reporting value; it no longer gates reuse or acceptance. Scheduled `cold_check_interval` was removed. The benchmark accepts `--quote-cap`; its reported share-above-cap metric remains fixed at 0.1 for the approved target.

Small sensitivity smoke: quiet scenario, seed 7, 15 updates, 3 expiries × 100 quotes. These measurements exercise the option, not the 40 × 100 / 200-update acceptance scenario.

| Gate cap | Fallback updates | Warm median (ms) | Warm/cold summed band-loss ratio | Warm share above 0.1 | Cold share above 0.1 | Load average before → after (1/5/15 min) |
|---:|---:|---:|---:|---:|---:|---|
| 0.05 | 15 / 15 | 5.371 | 0.9396 | 21.00% | 21.44% | 1.18 1.50 1.59 → 1.18 1.50 1.59 |
| 0.1 | 15 / 15 | 5.857 | 0.9549 | 22.36% | 21.44% | 1.25 1.51 1.59 → 1.25 1.51 1.59 |
| 0.2 | 15 / 15 | 5.771 | 1.0712 | 22.91% | 21.44% | 1.15 1.48 1.58 → 1.15 1.48 1.58 |

All three runs had zero warm/cold failures, invalid surfaces, and dense-grid violations. Each had 15/15 fallback-path updates, so this short quiet smoke confirms that WP-B2’s stricter gate and restricted/full-cold stages work but does not meet the fallback target; WP-B3/WP-B4 are expected to reduce that rate. The warm/cold share metric is always computed at 0.1, independently of the selected gate cap. These 3-slice timings are not comparable to the 40 × 100 acceptance targets.

The former `qualityGateFallsBackToColdSearch` assertion pinned `loss_tol * bandLossScale`; it was replaced with a per-quote candidate/reference assertion because that behavior is explicitly retired. The unchanged-slice fixture for `localWarmRefitHonorsBudgetAndQuality` uses cap 0.5 to isolate its local-refit budget assertion from unrelated slices rejected by the stricter 0.1 gate. The failed-prefit fixture uses cap 1.0 so its unaffected slice does not add an unrelated local fit. The 100-update reuse test now asserts the 12-update reuse ceiling and refit transition.
