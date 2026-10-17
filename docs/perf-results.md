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

Pooled quantiles give equal weight to each asset and session day; observations within an asset-day share that day's weight equally. This prevents IBIT's denser chain from dominating SPY. The latest capture contains 739 snapshots (738 usable, one unusable), covering 10 usable asset-days (five IBIT and five SPY). It produced 7,448 level/tilt observations and 197,250 residual observations. Same-session expiries use a fixed one-day date-based year fraction so expiry identity is stable across intraday snapshots.

| Asset | Measure | Observations | p10 | p50 | p95 |
|---|---|---:|---:|---:|---:|
| IBIT | Level | 7,072 | 0.000055305 | 0.000464534 | 0.003683010 |
| IBIT | Tilt | 7,072 | 0.000388369 | 0.003278513 | 0.047604162 |
| IBIT | Residual noise | 186,212 | 0.000045478 | 0.000963738 | 0.007027728 |
| SPY | Level | 376 | 0.000060588 | 0.000479849 | 0.002938721 |
| SPY | Tilt | 376 | 0.003135817 | 0.059893002 | 0.262696334 |
| SPY | Residual noise | 11,038 | 0.000041873 | 0.000727467 | 0.004044560 |

| Pooled measure | Observations | p10 | p50 | p95 |
|---|---:|---:|---:|---:|
| Level | 7,448 | 0.000058552 | 0.000473653 | 0.003178583 |
| Tilt | 7,448 | 0.000685505 | 0.011289126 | 0.206807038 |
| Residual noise | 197,250 | 0.000043413 | 0.000836693 | 0.005535503 |

Quiet uses p10, normal p50, fast p95. The measured front/back level ratio is 2.714112941; the front scenario uses the normal median for back expiries and scales front expiries by this ratio. No unmeasured per-expiry idiosyncratic amplitude is added.

| Scenario | Level move | Tilt move | Residual quote noise | Front level / back level |
|---|---:|---:|---:|---:|
| quiet | 0.000058552 | 0.000685505 | 0.000043413 | — |
| normal | 0.000473653 | 0.011289126 | 0.000836693 | — |
| fast | 0.003178583 | 0.206807038 | 0.005535503 | — |
| front | 0.001285547 | 0.011289126 | 0.000836693 | 2.714112941 |

### Full-session replay coverage

Replay retained each source timestamp and excluded locked/crossed quotes only in this benchmark adapter; the general preparation/CVI path is unchanged. Per-snapshot exclusion counts are in `build/snapshot-replay/replay_manifest.tsv`. Across the latest capture: 20 locked, 0 crossed, and 274,878 other quote exclusions. The unusable IBIT 2026-10-10 snapshot had no valid quotes. There were 720 consecutive warm pairs across the 10 usable asset-days, 18 cold starts (10 session starts plus 8 gap restarts), 8 gap events, and 16 missing five-minute intervals. Skipped/unusable snapshots and timestamp gaps break the chain and force a cold start.

| Asset | Session | Snapshots | Usable / skipped | Warm pairs | Cold starts | Gap events | Missing intervals | Locked | Crossed | Other exclusions |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| IBIT | 2026-10-10 | 1 | 0 / 1 | 0 | 0 | 0 | 0 | 0 | 0 | 50 |
| IBIT | 2026-10-12 | 60 | 60 / 0 | 51 | 9 | 8 | 16 | 2 | 0 | 41,653 |
| IBIT | 2026-10-13 | 76 | 76 / 0 | 75 | 1 | 0 | 0 | 2 | 0 | 51,377 |
| IBIT | 2026-10-14 | 76 | 76 / 0 | 75 | 1 | 0 | 0 | 1 | 0 | 52,978 |
| IBIT | 2026-10-15 | 76 | 76 / 0 | 75 | 1 | 0 | 0 | 0 | 0 | 54,903 |
| IBIT | 2026-10-16 | 69 | 69 / 0 | 68 | 1 | 0 | 0 | 1 | 0 | 49,956 |
| SPY | 2026-10-12 | 76 | 76 / 0 | 75 | 1 | 0 | 0 | 1 | 0 | 5,316 |
| SPY | 2026-10-13 | 78 | 78 / 0 | 77 | 1 | 0 | 0 | 1 | 0 | 5,561 |
| SPY | 2026-10-14 | 78 | 78 / 0 | 77 | 1 | 0 | 0 | 7 | 0 | 4,758 |
| SPY | 2026-10-15 | 78 | 78 / 0 | 77 | 1 | 0 | 0 | 1 | 0 | 4,325 |
| SPY | 2026-10-16 | 71 | 71 / 0 | 70 | 1 | 0 | 0 | 4 | 0 | 4,001 |

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

Supplemental in-scope warm-seed refinement: cold `nearestAtm` output remains unchanged. For local warm fitting only, `fitSlice` derives theta from the selected nearest ATM quote's ordered bid/ask IV center; if that band is unavailable or invalid it retains the mid-IV seed. This keeps the start tied to the active band-loss data and does not add a mid-centering objective. A deterministic warm regression failed before and passed after; with identical bands, it asserts identical fitted parameters when only an in-band mid on the nonbinding nearest-ATM quote changes. The post-change 1-expiry × 100-quote fast one-step smoke reports RMS excess ratio 1.801741815, maximum 4.047629088, 1/1 fallback update, and warm/cold summed band loss 7.404145831457e-05; failures, invalid surfaces, and dense-grid violations are all zero. The configured full verification passed after the change: Release build, CTest 5/5, pytest 10/10. The refinement does not resolve the fast-scenario RMS hard limit of 0.05; it stabilizes local starts when the quote mid and band center differ, not the measured scenario mismatch.

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

The approved high-fallback shortcut skips local prefit and starts at restricted cold when the previous result's fallback-slice count is greater than half its surface slices and the **mean absolute midpoint-IV move across matching included quotes** is at least `0.10340351909729481`. This threshold is the latest pooled WP-B7 fast-scenario p95 absolute tilt (`0.20680703819458962`) multiplied by the mean absolute `clamp(2*k, -1, 1)` loading (`0.5`) for uniform log-moneyness over `[-0.5, 0.5]`. In a one-step 40 × 100 synthetic check using the 739-snapshot profile, observed mean movement was 0.00571308–0.00572677 for normal and 0.104090–0.104174 for fast (seeds 1–5), separating these empirical distributions. The residual-noise p95 is not used as a move threshold. Quotes match by expiry-group order and quote ID; unavailable matches do not contribute.

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

## WP-B5 — Deterministic 4 × 50 hard-limit regression

The standalone `essvi_warm_scenario_regression` target runs quiet, normal, fast, and front for 50 updates each at 40 expiries × 100 quotes, fixed seed 1. It aggregates quality over paired quote-update observations (the same population semantics as the scenario benchmark), checks Addendum B hard limits, validates every warm result and dense-grid surface, and exits nonzero on a breach. Each first breach reports scenario, seed, update, metric, observed value, and hard limit. It is built with the configured CMake build but is intentionally invoked separately from CTest: the hard-limit run currently returns nonzero and its measured latency limit is host-dependent.

Earlier 4 × 50 run, superseded by the 739-snapshot-profile rerun below: `build/essvi_warm_scenario_regression`. Every scenario breached at least one hard limit; no warm invalid surfaces or dense-grid violations were reported. Timing is one run and remains load-sensitive.

| Scenario | p99 (ms) / hard limit | Fallback updates (%) / hard limit | Warm RMS excess ratio / hard limit | Other reported hard-limit breaches |
|---|---:|---:|---:|---|
| quiet | 78.431 / 10 | 14 / 5 | 0.00687 / 0.05 | Median 1.434 / 0.47 ms and p99 78.431 / 7.6 ms also breach quiet non-regression; warm/cold summed-loss ratio unavailable because cold reference loss was zero while warm loss was positive. |
| normal | 116.936 / 10 | 100 / 5 | 0.23966 / 0.05 | None in paired cap-share, maximum-ratio, summed-loss, or RMS-IV-delta checks. |
| fast | 113.424 / 20 | 96 / 25 | 255,248.641 / 0.05 | One unpaired update and one warm plus cold-reference failure; RMS excess-IV delta 0.16032 / 0.05. |
| front | 196.840 / 10 | 100 / 5 | 0.24640 / 0.05 | None in paired cap-share, maximum-ratio, summed-loss, or RMS-IV-delta checks. |

The regression is intentionally not described as a performance pass: current 4 × 50 hard-limit misses remain visible for downstream work. The direct degraded-result tests in `tests/test_warm_calibrator.cpp` assert age/count agreement and the ordinary full-cold budget in addition to valid surfaces and forced-refit behavior.

### Latest 739-snapshot-profile 4 × 50 regression

Command: `build/essvi_warm_scenario_regression`. The fixed-seed run completed 50 updates per scenario and exited 1 because each scenario breached hard limits. No invalid warm surfaces or dense-grid violations were reported. Values below are the first reported breach for each metric; a dash means no breach was reported for that metric.

| Scenario | Warm p99 (ms) / limit | Fallback updates (%) / limit | Warm RMS excess ratio / limit | Other reported breaches |
|---|---:|---:|---:|---|
| quiet | 67.642 / 10 | 34 (17/50) / 5 | — / 0.05 | Median 1.604 / 0.47 ms; p99 67.642 / 7.6 ms; summed band-loss ratio `∞` / 1.2 because cold loss was zero. |
| normal | 103.223 / 10 | 100 (50/50) / 5 | 0.15234 / 0.05 | None reported in RMS excess-IV delta or paired quality checks. |
| fast | 92.455 / 20 | 100 (50/50) / 25 | 587.325 / 0.05 | RMS excess-IV delta 0.087678 / 0.05. |
| front | 110.578 / 10 | 100 (50/50) / 5 | 0.15715 / 0.05 | None reported in RMS excess-IV delta or paired quality checks. |

This remains a hard-limit failure, not a performance pass.

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

## WP-B6 — Cold-path A/B (inconclusive)

The designated reference is `build/wp5-parallel/essvi_bench`; it is an ELF x86-64 executable built 2026-10-15. Its CMake cache and compile commands identify the same source directory, GCC 15.2.0 at `/usr/bin/c++`, GNU C++20, Release (`-O3 -DNDEBUG` followed by effective `-O2`), `ESSVI_MAX_CALIBRATION_THREADS=4`, native optimization off, and MM-bound profiling off as the current build. Both binaries reported compiler 15.2.0. The reference cache does not record a Git revision; its objects predate the current source files, and the historical `perf/essvi-calibrator` tree is documented as uncommitted. Exact reference source provenance is therefore unverified. Per the approved spec, the comparison is **inconclusive**, not a regression or pass.

The reference CLI has no `--seed` option. Both binaries received the same normalized CSV fixture generated from the cold `syntheticChain` with seed 1. To verify the input contents, a temporary C++ exporter using the actual `syntheticChain` implementation generated canonical full and single-slice fixtures (SHA-256 `3ed3cbcf454049c53b2b856691728bf37fecf20df96ed5b5273fafb62930f0a9` and `af66d8b88a399e81fee6e624f5b97f5068c5c4169f3c19ed4e8d54f4edbe91d8`); all numeric and identity fields matched the benchmark inputs across 4,000 and 100 quotes. Full-chain runs used 40 expiries × 100 quotes and nine repeated calibrations after the initial cold call; single-slice runs used one expiry × 100 quotes and 20 repeats. Each invocation performed an initial cold fit followed by the specified repeated fits with `--warm-updates 1`; the table reports repeated-fit median / p90, not the separately measured first call. Parameters were written for both binaries and compared across `theta`, `psi`, `rho`, and `phi`: maximum relative difference was zero for both fixtures, within the parent 1e-9 target. Objective-evaluation and MM-bound counts also matched exactly (48,158 / 98,610 for full chain; 1,358 / 2,687 for one slice).

Two sessions disagree on timing direction. Session 2 load averages were captured externally around each binary because the reference executable does not emit them. Timings are repeated-fit median / p90 in milliseconds.

| Size | Session | Reference median / p90 | Current median / p90 | Reference load average before → after | Current load average before → after |
|---|---|---:|---:|---|---|
| 40 × 100; 9 repeats | 1 | 54.567 / 60.852 | 62.999 / 70.187 | not captured | 0.53 1.09 1.25 → 0.53 1.09 1.25 |
| 40 × 100; 9 repeats | 2 | 111.955 / 141.458 | 97.672 / 113.701 | 0.29 0.76 1.05 → 0.29 0.76 1.05 | 0.29 0.76 1.05 → 0.59 0.82 1.07 |
| 1 × 100; 20 repeats | 1 | 2.350 / 2.727 | 2.656 / 2.816 | not captured | 0.49 1.06 1.24 → 0.49 1.06 1.24 |
| 1 × 100; 20 repeats | 2 | 2.506 / 3.354 | 3.651 / 4.229 | 0.59 0.82 1.07 → 0.59 0.82 1.07 | 0.59 0.82 1.07 → 0.59 0.82 1.07 |

The first full-chain session makes the current binary 15.5% slower than the reference; the second makes it 12.8% faster. Single-slice sessions show +13.1% and +45.7%, but the reference itself moves from a 2.350 ms to a 2.506 ms median, crossing the 2.5 ms threshold. Current medians exceed the 55 ms full-chain and 2.5 ms single-slice thresholds in both sessions; the second-session reference also exceeds both. This dispersion prevents a measurement-noise judgment. The first session’s reference load was not captured; session 2 has matching system load averages around the paired runs. No cold-path code was changed: provenance is incomplete and the repeated sessions do not establish a stable regression. The measured parameter outputs are identical, but this does not resolve the missing source revision.

The exact benchmark shape/repeat commands were the corresponding binary with `--expiries {40|1} --quotes-per-expiry 100 --repeats {9|20} --warm-updates 1 --input <shared-seed-1-csv> --parameters-out <binary-specific.csv> --timings-out <binary-specific.json>`. The old executable’s lack of seed support is why the same generated input file, rather than separate synthetic generation, was used.

## Five-seed acceptance run — superseded 620-snapshot scenario profile

This historical run used the 620-snapshot WP-B7 profile and predates the refreshed 739-snapshot profile above. Both corrections described here remain in the current implementation: the shortcut threshold maps pooled fast-scenario p95 tilt to the mean-absolute-mid-IV metric, and `evolveWarmScenario` applies one shared IV-floor correction to bid, ask, and mid to preserve quote width. Focused regressions failed before each fix and passed afterward.

Command: `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh`. All 20 cases completed; every `--check` returned 2 for target misses and the script exited 1. Values are five-seed medians with `[minimum, maximum]` ranges.

| Scenario | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | RMS excess ratio, warm / cold | Warm/cold band loss | RMS excess IV delta (vol pt) |
|---|---:|---:|---:|---:|---:|---:|
| quiet | 1.57377 [1.46914, 1.86285] | 88.7447 [73.5194, 111.078] | 23.5 [8.5, 31] | 0.00506518 / 0.00001791 | unavailable (zero cold loss in some seeds) | 0.00492870 |
| normal | 74.0016 [68.7923, 78.8912] | 162.712 [144.191, 290.799] | 94 [90, 95.5] | 2.12155 / 2.12138 | 0.999994 | -0.00000391 |
| fast | 51.8106 [46.2765, 64.7232] | 185.213 [130.591, 293.289] | 78.5 [69, 93] | 548.512 / 548.279 | 1.00251 | 0.0532054 |
| front | 82.8518 [74.9025, 88.7928] | 309.551 [157.811, 423.184] | 93.5 [89.5, 95.5] | 2.15199 / 2.15185 | 1.00000 | -0.00000149 |

All 20 updates had zero warm/cold failures, invalid surfaces, and dense-grid violations. Every scenario still misses its p99 and fallback hard limits; quiet also regresses its 49fed2b median/p99. The warm/cold summed-loss ratio is undefined when cold loss is zero. The normal, fast, and front cold references themselves have RMS excess ratios far above the 0.05 hard limit, and the warm RMS is nearly identical to cold. The current 200-update synthetic path applies independently signed measured-size moves cumulatively, so its late snapshots produce quote sets that the approved eSSVI fit/objective cannot represent at the stated quality limits. Changing that temporal model would change the accepted workload; no such unapproved change was made.


## Five-seed acceptance run — superseded 719-snapshot profile

This historical run used the prior 719-snapshot profile; the latest 739-snapshot capture and scenario results follow. Command: `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh`. All 20 scenario/seed runs completed at 40 expiries × 100 quotes for 200 updates; each `--check` returned 2 for target misses, and the script returned 1 after the full matrix. Values are five-seed medians with `[minimum, maximum]` ranges.

| Scenario | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | Cap-share delta (pp) |
|---|---:|---:|---:|---:|
| quiet | 1.67084 [0.798161, 2.01678] | 63.9807 [53.9157, 103.957] | 20 [9.5, 34] | 0 [0, 0] |
| normal | 69.7051 [64.0588, 93.8719] | 233.6 [113.836, 288.432] | 93 [89.5, 95.5] | -0.0125 [-0.02225, 0.0265] |
| fast | 41.0258 [38.8692, 43.023] | 130.645 [121.114, 239.327] | 82 [71.5, 91] | 0.0696429 [0.00932203, 0.186082] |
| front | 77.0258 [69.5392, 80.794] | 251.987 [145.964, 287.147] | 92.5 [90, 94] | -0.017875 [-0.0295, 0.018375] |

| Scenario | Maximum excess ratio, warm / cold | RMS excess ratio, warm / cold | Summed band loss, warm / cold | RMS excess-IV delta (vol pt) |
|---|---:|---:|---:|---:|
| quiet | 0.0998846 [0.0997549, 0.0999925] / 0.00657002 [0, 0.0448181] | 0.00600215 [0.00344901, 0.00683066] / 0.0000106394 [0, 0.0000519783] | unavailable (zero cold reference) | 0.00581645 [0.00342804, 0.0067951] |
| normal | 16.209 [4.04282, 102.559] / 16.209 [4.04282, 102.559] | 0.88507 [0.350834, 2.3031] / 0.885043 [0.350846, 2.30311] | 1 [0.999978, 1.00027] | -0.00000710488 [-0.00000927445, 0.0000275216] |
| fast | 13,517.1 [11,975.9, 19,207.8] / 13,517.1 [11,975.9, 19,207.8] | 605.423 [365.616, 825.275] / 605.36 [365.115, 825.221] | 1.00159 [0.9997, 1.00337] | 0.0708032 [0.0252837, 0.0834204] |
| front | 17.287 [3.89513, 102.892] / 17.287 [3.89513, 102.892] | 0.896722 [0.355116, 2.0576] / 0.896709 [0.355131, 2.05761] | 0.999994 [0.99998, 1.00009] | -0.0000059915 [-0.0000160868, 0.00000990809] |

All 20 runs had zero warm/cold failures, invalid surfaces, and dense-grid violations. Every scenario missed warm p99 and fallback targets and hard limits; quiet also missed both 49fed2b median/p99 non-regression limits. Normal, fast, and front exceed the 0.05 RMS excess-ratio hard limit for both warm and cold references; fast also exceeds the 0.05 RMS excess-IV-delta hard limit. Cap-share, maximum-ratio versus cold, and summed-loss checks pass where defined; quiet loss ratio is unavailable because some cold reference losses are zero. The normal, fast, and front cold-reference RMS values are themselves far above the specified limit, and warm results are nearly identical. This is consistent with cumulative independent measured-size quote moves producing late scenario snapshots that the approved eSSVI objective cannot fit to the specified absolute RMS limit. Changing that trajectory or the objective would alter the approved workload/model, so neither was changed.

Load averages (1/5/15-minute, before → after):

| Scenario | Seed | Load average before → after |
|---|---:|---|
| quiet | 1 | 1.32 2.13 2.58 → 1.64 2.16 2.58 |
| quiet | 2 | 1.64 2.16 2.58 → 2.75 2.38 2.65 |
| quiet | 3 | 2.75 2.38 2.65 → 3.43 2.57 2.70 |
| quiet | 4 | 3.43 2.57 2.70 → 4.32 2.87 2.80 |
| quiet | 5 | 4.32 2.87 2.80 → 4.44 2.99 2.84 |
| normal | 1 | 4.44 2.99 2.84 → 5.01 3.30 2.95 |
| normal | 2 | 5.01 3.30 2.95 → 5.09 3.53 3.04 |
| normal | 3 | 5.09 3.53 3.04 → 4.61 3.56 3.07 |
| normal | 4 | 4.61 3.56 3.07 → 4.62 3.66 3.12 |
| normal | 5 | 4.62 3.66 3.12 → 4.16 3.65 3.13 |
| fast | 1 | 4.16 3.65 3.13 → 3.25 3.47 3.08 |
| fast | 2 | 3.25 3.47 3.08 → 3.48 3.52 3.10 |
| fast | 3 | 3.48 3.52 3.10 → 4.08 3.65 3.15 |
| fast | 4 | 4.08 3.65 3.15 → 3.95 3.64 3.16 |
| fast | 5 | 3.95 3.64 3.16 → 3.82 3.62 3.17 |
| front | 1 | 3.82 3.62 3.17 → 4.25 3.76 3.23 |
| front | 2 | 4.25 3.76 3.23 → 4.35 3.84 3.27 |
| front | 3 | 4.35 3.84 3.27 → 4.37 3.91 3.32 |
| front | 4 | 4.37 3.91 3.32 → 3.65 3.78 3.29 |
| front | 5 | 3.65 3.78 3.29 → 3.37 3.70 3.28 |

Fallback/rejection reason histograms (five-seed totals):

| Scenario | Infeasible local result | Loss above gate | Calendar repair failed | Bound check failed | Budget exhausted |
|---|---:|---:|---:|---:|---:|
| quiet | 0 | 3,412 | 0 | 0 | 0 |
| normal | 0 | 5,635 | 0 | 0 | 531 |
| fast | 0 | 15,233 | 677 | 397,377 | 4,423 |
| front | 0 | 5,605 | 0 | 0 | 519 |

## Five-seed acceptance run — superseded 726-snapshot profile

Command: `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh`. All 20 scenario/seed runs completed at 40 expiries × 100 quotes for 200 updates; every `--check` returned 2 for target misses, and the script exited 1 after the full matrix. Values are five-seed medians with `[minimum, maximum]` ranges.

| Scenario | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | Cap-share delta (pp) |
|---|---:|---:|---:|---:|
| quiet | 1.76051 [0.794526, 2.00409] | 61.8479 [53.3973, 202.84] | 27 [6.5, 30.5] | 0 [0, 0] |
| normal | 83.9052 [70.9068, 101.775] | 279.302 [141.406, 307.037] | 93.5 [89.5, 95.5] | -0.01425 [-0.027125, 0.004875] |
| fast | 45.5597 [26.8635, 49.1836] | 153.94 [79.9754, 338.423] | 84.5 [68, 93] | 0.0626263 [0.00860656, 0.176238] |
| front | 85.7315 [68.6614, 87.3181] | 266.05 [118.956, 430.608] | 93 [90, 95.5] | -0.013125 [-0.02725, 0.01125] |

| Scenario | Maximum excess ratio, warm / cold | RMS excess ratio, warm / cold | Summed band loss, warm / cold | RMS excess-IV delta (vol pt) |
|---|---:|---:|---:|---:|
| quiet | 0.0999683 [0.0994678, 0.0999946] / 0.00569392 [0, 0.0273015] | 0.00658321 [0.00335673, 0.00691196] / 0.00000858338 [0, 0.0000312984] | unavailable (zero cold reference) | 0.00652201 [0.00337092, 0.00668774] |
| normal | 15.6117 [3.82401, 102.018] / 15.6117 [3.82401, 102.018] | 0.85026 [0.347461, 2.18318] / 0.850253 [0.347493, 2.18318] | 0.999996 [0.999939, 1.00007] | -0.00000425322 [-0.0000243249, 0.00000583243] |
| fast | 16,069.9 [11,831.5, 18,944] / 16,069.9 [11,831.5, 18,944] | 616.886 [413.294, 833.123] / 616.849 [412.971, 832.817] | 1.00103 [0.999992, 1.00405] | 0.0390601 [0.0158866, 0.0744161] |
| front | 16.5906 [3.87935, 98.3092] / 16.5906 [3.87935, 98.3092] | 0.864797 [0.352312, 1.93749] / 0.864785 [0.352316, 1.93749] | 0.999996 [0.999983, 1.00016] | -0.00000455697 [-0.0000140499, 0.0000121843] |

All 20 runs had zero unpaired updates, warm/cold failures, invalid surfaces, and dense-grid violations. Every scenario missed p99 and fallback targets and hard limits; quiet also missed both 49fed2b median/p99 non-regression limits. Normal, fast, and front exceed the 0.05 RMS excess-ratio hard limit for warm and cold; some fast seeds also exceed the 0.05 RMS excess-IV-delta hard limit. Cap-share and max-ratio-vs-cold checks pass; summed band loss passes where defined. Quiet loss ratio is unavailable because the cold reference loss is zero for some seeds. The normal, fast, and front cold-reference RMS values are themselves far above the limit, and warm results remain nearly identical. Cumulative independent measured-size moves produce late scenario snapshots the approved eSSVI objective cannot fit to the absolute RMS threshold; changing that workload/model would exceed scope.

Load averages (1/5/15-minute, before → after):

| Scenario | Seed | Load average before → after |
|---|---:|---|
| quiet | 1 | 1.11 2.10 2.69 → 1.78 2.21 2.72 |
| quiet | 2 | 1.78 2.21 2.72 → 2.05 2.25 2.72 |
| quiet | 3 | 2.05 2.25 2.72 → 2.55 2.35 2.74 |
| quiet | 4 | 2.55 2.35 2.74 → 4.32 2.81 2.88 |
| quiet | 5 | 4.32 2.81 2.88 → 4.82 3.05 2.96 |
| normal | 1 | 4.82 3.05 2.96 → 5.06 3.35 3.07 |
| normal | 2 | 5.06 3.35 3.07 → 5.97 3.84 3.25 |
| normal | 3 | 5.97 3.84 3.25 → 5.11 3.85 3.28 |
| normal | 4 | 5.11 3.85 3.28 → 4.26 3.79 3.28 |
| normal | 5 | 4.26 3.79 3.28 → 3.70 3.70 3.26 |
| fast | 1 | 3.70 3.70 3.26 → 3.52 3.66 3.26 |
| fast | 2 | 3.52 3.66 3.26 → 3.15 3.56 3.24 |
| fast | 3 | 3.15 3.56 3.24 → 3.36 3.60 3.25 |
| fast | 4 | 3.36 3.60 3.25 → 3.76 3.68 3.29 |
| fast | 5 | 3.76 3.68 3.29 → 3.50 3.62 3.28 |
| front | 1 | 3.50 3.62 3.28 → 3.72 3.65 3.30 |
| front | 2 | 3.72 3.65 3.30 → 4.49 3.85 3.38 |
| front | 3 | 4.49 3.85 3.38 → 4.64 4.00 3.45 |
| front | 4 | 4.64 4.00 3.45 → 4.23 3.96 3.46 |
| front | 5 | 4.23 3.96 3.46 → 4.40 4.02 3.49 |

Fallback/rejection reason histograms (five-seed totals):

| Scenario | Infeasible local result | Loss above gate | Calendar repair failed | Bound check failed | Budget exhausted |
|---|---:|---:|---:|---:|---:|
| quiet | 0 | 3,365 | 0 | 0 | 0 |
| normal | 0 | 5,626 | 0 | 0 | 515 |
| fast | 0 | 14,214 | 504 | 377,765 | 4,099 |
| front | 0 | 5,618 | 0 | 0 | 516 |

## Five-seed 200-update synthetic stress matrix — 739-snapshot profile

This historical 200-update matrix used the 739-snapshot profile, the refreshed shortcut threshold 0.10340351909729481, 40 expiries × 100 quotes, and 200 updates per scenario/seed. All 20 runs completed with zero invalid warm surfaces and zero dense-grid violations. Fast seeds 3, 4, and 5 had 149, 109, and 107 unpaired/unscored updates and 144, 106, and 94 warm failures; seeds 4 and 5 also had 104 and 101 cold failures. Every scenario family breached warm p99 and fallback hard limits in every seed; quiet also missed the 49fed2b median/p99 non-regression limits. The earlier --check statuses used the former absolute RMS criterion and are not treated as current session-scale acceptance. Fast's five-seed median RMS excess-IV delta was 0.0660718 vol point, above its unchanged 0.05 hard limit, with the seed range crossing that limit. Summed band loss met the 1.20 hard limit for normal, fast, and front; the quiet ratio is unavailable where cold reference loss was zero. Values are five-seed medians with [minimum, maximum] ranges; load averages are 1/5/15-minute values before → after each run.

| Scenario | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | Cap-share delta (pp) | Maximum excess ratio, warm / cold | RMS excess ratio, warm / cold | Summed band-loss ratio | RMS excess-IV delta (vol pt) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| quiet | 1.47791 [0.704516, 1.7976] | 65.5706 [47.581, 118.117] | 26 [8, 30.5] | 0 [0, 0] | 0.0999268 [0.0996076, 0.0999942] / 0.00645331 [0, 0.0383648] | 0.00639927 [0.0030859, 0.00693541] / 1.02629e-05 [0, 4.40601e-05] | unavailable (zero cold reference in some seeds) | 0.00644993 [0.00308443, 0.00660808] |
| normal | 67.7245 [59.9524, 81.5539] | 217.006 [113.642, 361.601] | 93 [89, 96] | -0.0105 [-0.018625, 0.017375] | 18.3615 [3.84666, 98.8054] / 18.3615 [3.84666, 98.8054] | 0.980526 [0.355711, 2.59097] / 0.980511 [0.355721, 2.59098] | 1.00001 [0.999978, 1.00016] | -4.08511e-06 [-1.62301e-05, 1.55582e-05] |
| fast | 35.3656 [23.3659, 45.2901] | 121.154 [97.6346, 160.493] | 81 [68, 91] | 0.0607143 [0.0147059, 0.200806] | 13,638.2 [11,350.3, 19,680.9] / 13,638.2 [11,350.3, 19,680.9] | 531.402 [373.712, 948.382] / 531.361 [373.263, 948.375] | 1.0017 [1.00062, 1.00851] | 0.0660718 [0.0408365, 0.152395] |
| front | 61.4091 [57.79, 63.4585] | 119.928 [105.157, 249.686] | 93 [89.5, 95.5] | -0.016875 [-0.01925, 0.01625] | 19.7617 [3.93533, 98.818] / 19.7617 [3.93533, 98.818] | 0.995472 [0.360896, 2.31676] / 0.995455 [0.360922, 2.31676] | 0.999998 [0.999948, 1.0002] | -4.18469e-06 [-2.28223e-05, 1.78394e-05] |

Fallback/rejection reason histograms (five-seed totals):

| Scenario | Infeasible local result | Loss above gate | Calendar repair failed | Bound check failed | Budget exhausted |
|---|---:|---:|---:|---:|---:|
| quiet | 0 | 3,636 | 0 | 0 | 0 |
| normal | 0 | 5,511 | 0 | 0 | 511 |
| fast | 0 | 16,699 | 679 | 444,326 | 4,740 |
| front | 0 | 5,571 | 0 | 0 | 516 |

Load averages (1/5/15-minute, before → after):

| Scenario | Seed | Load average before → after |
|---|---:|---|
| quiet | 1 | 1.34 1.30 1.10 → 1.86 1.42 1.14 |
| quiet | 2 | 1.86 1.42 1.14 → 2.26 1.53 1.18 |
| quiet | 3 | 2.26 1.53 1.18 → 2.89 1.71 1.25 |
| quiet | 4 | 2.89 1.71 1.25 → 3.15 1.82 1.30 |
| quiet | 5 | 3.15 1.82 1.30 → 3.26 1.91 1.33 |
| normal | 1 | 3.26 1.91 1.33 → 3.17 2.01 1.38 |
| normal | 2 | 3.17 2.01 1.38 → 3.30 2.13 1.44 |
| normal | 3 | 3.30 2.13 1.44 → 3.46 2.27 1.51 |
| normal | 4 | 3.46 2.27 1.51 → 3.69 2.47 1.61 |
| normal | 5 | 3.69 2.47 1.61 → 4.14 2.72 1.72 |
| fast | 1 | 4.14 2.72 1.72 → 3.66 2.69 1.73 |
| fast | 2 | 3.66 2.69 1.73 → 3.74 2.75 1.76 |
| fast | 3 | 3.74 2.75 1.76 → 3.35 2.71 1.77 |
| fast | 4 | 3.35 2.71 1.77 → 4.19 2.94 1.86 |
| fast | 5 | 4.19 2.94 1.86 → 4.01 2.96 1.89 |
| front | 1 | 4.01 2.96 1.89 → 3.91 3.03 1.94 |
| front | 2 | 3.91 3.03 1.94 → 3.86 3.09 1.99 |
| front | 3 | 3.86 3.09 1.99 → 3.53 3.07 2.02 |
| front | 4 | 3.53 3.07 2.02 → 4.07 3.23 2.10 |
| front | 5 | 4.07 3.23 2.10 → 3.75 3.22 2.13 |

### Latest full-session replay quality — 739-snapshot capture after warm-only seed refinement

Run against the current code after the warm-only ATM seed refinement: `export PATH="$PWD/.venv/bin:$PATH"; python/run_snapshot_replay.py --data-root /home/zhangy/vol-rv/data --output-dir /tmp/cal-recalib-replay --bench build/essvi_bench`. The capture contains 739 snapshots (738 usable, one unusable), 720 warm pairs, 18 cold starts, 8 gaps, 16 missing intervals, and 20 locked / 0 crossed / 274,878 other quote exclusions. All ten usable asset-days had zero warm/cold failures, invalid surfaces, and dense-grid violations. European Black-Scholes IV is a calibration-path stand-in; these data do not establish American pricing or consolidated NBBO accuracy.

The longest uninterrupted session chain is 77 warm pairs (SPY, 2026-10-13 through 2026-10-16). The four synthetic verification scenarios now use 77 updates; the prior 200-update matrix is retained above as long-run stress. RMS excess ratio acceptance is the warm-minus-cold increase with unchanged 0.02/0.05 target/hard margins. Current cold-reference RMS ratios range from 1.718674 to 3.112787, so the former absolute 0.05 criterion was below every measured cold reference. No latency or fallback limit was relaxed: full-session fallback remains 94.118–100%, and warm p99 ranges from 6.951 to 125.139 ms (cold p99 6.738–92.820 ms). The calibration removes a cold-floor false failure; it does not turn the observed speed/fallback misses into passes.

| Asset | Session | Warm p50 / p99 (ms) | Cold p50 / p99 (ms) | Fallback (%) | Maximum W/C | RMS W/C | RMS Δ W−C | Loss W/C | RMS excess-IV Δ (vol pt) |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| IBIT | 2026-10-12 | 29.656 / 49.601 | 29.413 / 43.324 | 94.118 | 28.536 / 28.536 | 2.574 / 2.581 | -0.007273 | 0.981783 | -0.001869 |
| IBIT | 2026-10-13 | 27.665 / 47.898 | 25.235 / 36.987 | 96.000 | 64.134 / 64.134 | 2.459 / 2.461 | -0.001922 | 0.983265 | -0.001185 |
| IBIT | 2026-10-14 | 29.731 / 54.406 | 29.357 / 41.201 | 96.000 | 19.308 / 19.308 | 2.249 / 2.252 | -0.002774 | 0.986992 | -0.001919 |
| IBIT | 2026-10-15 | 27.539 / 54.772 | 26.383 / 46.583 | 96.000 | 20.038 / 20.038 | 2.408 / 2.411 | -0.002621 | 0.978544 | -0.001872 |
| IBIT | 2026-10-16 | 23.846 / 49.803 | 23.610 / 42.613 | 96.000 | 21.245 / 21.245 | 1.719 / 1.719 | +0.000036 | 0.977135 | -0.001496 |
| SPY | 2026-10-12 | 2.277 / 6.951 | 2.290 / 6.738 | 100.000 | 8.148 / 8.148 | 1.943 / 1.943 | 0.000000 | 1.000000 | 0.000000 |
| SPY | 2026-10-13 | 2.663 / 15.578 | 2.644 / 9.710 | 100.000 | 9.634 / 9.634 | 1.982 / 1.982 | 0.000000 | 1.000000 | 0.000000 |
| SPY | 2026-10-14 | 2.940 / 11.762 | 2.950 / 9.078 | 100.000 | 6.106 / 6.106 | 1.752 / 1.752 | +0.000001 | 0.999999 | -0.00000003 |
| SPY | 2026-10-15 | 3.025 / 18.219 | 3.002 / 20.559 | 100.000 | 11.093 / 11.093 | 3.113 / 3.113 | -0.000003 | 1.000002 | +0.00000009 |
| SPY | 2026-10-16 | 4.105 / 125.139 | 3.269 / 92.820 | 100.000 | 10.795 / 10.795 | 2.451 / 2.451 | -0.000003 | 1.000001 | -0.00000027 |

RMS Δ is warm RMS excess ratio minus cold RMS excess ratio. The maximum increase is 0.000036, below the 0.02 target. At cap 0.1, warm-minus-cold share changes remain between 0 and +0.131 percentage points; band-loss ratios remain 0.977135–1.000002. These quality comparisons pass the revised relative limits while the p99/fallback objectives remain unmet.

| Asset | Session | Cap 0.05: W/C %, Δpp | Cap 0.1: W/C %, Δpp | Cap 0.2: W/C %, Δpp |
|---|---|---:|---:|---:|
| IBIT | 2026-10-12 | 73.45 / 73.38, +0.070 | 71.14 / 71.05, +0.090 | 66.86 / 66.70, +0.161 |
| IBIT | 2026-10-13 | 74.09 / 74.07, +0.027 | 72.04 / 72.01, +0.039 | 67.36 / 67.30, +0.054 |
| IBIT | 2026-10-14 | 74.60 / 74.52, +0.076 | 72.17 / 72.09, +0.081 | 67.83 / 67.75, +0.074 |
| IBIT | 2026-10-15 | 76.51 / 76.49, +0.019 | 74.44 / 74.40, +0.037 | 70.46 / 70.43, +0.028 |
| IBIT | 2026-10-16 | 70.46 / 70.34, +0.118 | 67.86 / 67.73, +0.131 | 63.06 / 62.94, +0.118 |
| SPY | 2026-10-12 | 80.03 / 80.03, +0.000 | 76.61 / 76.61, +0.000 | 71.71 / 71.71, +0.000 |
| SPY | 2026-10-13 | 80.13 / 80.13, +0.000 | 77.05 / 77.05, +0.000 | 71.56 / 71.56, +0.000 |
| SPY | 2026-10-14 | 76.10 / 76.10, +0.000 | 73.49 / 73.49, +0.000 | 69.14 / 69.14, +0.000 |
| SPY | 2026-10-15 | 80.98 / 80.98, +0.000 | 78.48 / 78.48, +0.000 | 74.17 / 74.17, +0.000 |
| SPY | 2026-10-16 | 79.85 / 79.85, +0.000 | 77.51 / 77.51, +0.000 | 73.15 / 73.15, +0.000 |


## Five-seed 77-update session-scale synthetic matrix — 739-snapshot profile

Command: `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh`. Pre-refinement baseline in the current checkout at `e2bfe2c`: all 20 cases (quiet, normal, fast, front × seeds 1–5) completed at 40 expiries × 100 quotes and 77 updates per case. Every `--check` returned 2; the script returned 1 for target misses. Warm p50/p99 are milliseconds, other entries are five-seed medians [minimum, maximum]. RMS Δ is the current warm-minus-cold acceptance metric (target 0.02, hard limit 0.05).

| Scenario | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | Cap-share Δ (pp) | RMS Δ (warm − cold) | Summed band-loss ratio | RMS excess-IV Δ (vol pt) |
|---|---:|---:|---:|---:|---:|---:|---:|
| quiet | 1.1265 [0.536961, 2.01023] | 56.8329 [2.21747, 77.2721] | 16.8831 [0, 35.0649] | 0 [0, 0] | 0.00663570 [0.000772862, 0.00891233] | unavailable (zero cold reference in some seeds) | 0.00648086 [0.000883023, 0.00863326] |
| normal | 69.2037 [59.1676, 73.7056] | 132.795 [113.199, 264.957] | 81.8182 [71.4286, 89.6104] | -0.0272727 [-0.0483766, 0.0451299] | -0.00000673532 [-0.000135945, 0.000202363] | 1.00027 [0.998972, 1.00652] | -0.00000853157 [-0.000182786, 0.000199936] |
| fast | 44.081 [41.421, 57.355] | 125.587 [104.016, 198.609] | 87.013 [64.9351, 90.9091] | 0.0844595 [0.0157609, 0.315455] | 0.138624 [0.0497174, 0.724323] | 1.00223 [1.00055, 1.01748] | 0.0702967 [0.0568585, 0.222875] |
| front | 60.1868 [58.0428, 77.0039] | 130.049 [104.735, 261.519] | 81.8182 [72.7273, 88.3117] | -0.0438312 [-0.05, 0.0422078] | -0.00000790792 [-0.000113117, 0.000238674] | 0.999997 [0.999535, 1.00809] | -0.0000120131 [-0.0000955679, 0.00023594] |

All cases had zero invalid warm surfaces and zero dense-grid violations. Fast seeds 1–5 had 69 unpaired/unscored updates, 63 warm failures, and 55 cold failures; their RMS Δ values ranged from 0.0497174 to 0.724323, so four breached the 0.05 hard limit and the fifth missed the 0.02 target. Quiet has a zero cold band-loss reference in some seeds. The full-session replay remains the workload-faithful quality comparison; this matrix is a synthetic session-length stress check.

Load averages (1/5/15-minute, before → after):

| Scenario | Seed | Load average before → after |
|---|---:|---|
| quiet | 1 | 0.99 1.50 1.25 → 1.44 1.58 1.28 |
| quiet | 2 | 1.44 1.58 1.28 → 1.80 1.66 1.31 |
| quiet | 3 | 1.80 1.66 1.31 → 1.74 1.64 1.31 |
| quiet | 4 | 1.74 1.64 1.31 → 2.00 1.70 1.33 |
| quiet | 5 | 2.00 1.70 1.33 → 2.08 1.72 1.34 |
| normal | 1 | 2.08 1.72 1.34 → 2.22 1.77 1.36 |
| normal | 2 | 2.22 1.77 1.36 → 2.17 1.78 1.37 |
| normal | 3 | 2.17 1.78 1.37 → 2.29 1.82 1.39 |
| normal | 4 | 2.29 1.82 1.39 → 2.48 1.88 1.41 |
| normal | 5 | 2.48 1.88 1.41 → 2.64 1.93 1.43 |
| fast | 1 | 2.64 1.93 1.43 → 2.59 1.93 1.44 |
| fast | 2 | 2.59 1.93 1.44 → 2.67 1.97 1.45 |
| fast | 3 | 2.67 1.97 1.45 → 2.61 1.97 1.46 |
| fast | 4 | 2.61 1.97 1.46 → 2.52 1.97 1.46 |
| fast | 5 | 2.52 1.97 1.46 → 2.81 2.05 1.50 |
| front | 1 | 2.81 2.05 1.50 → 2.77 2.07 1.51 |
| front | 2 | 2.77 2.07 1.51 → 2.90 2.12 1.53 |
| front | 3 | 2.90 2.12 1.53 → 3.00 2.16 1.55 |
| front | 4 | 3.00 2.16 1.55 → 3.36 2.29 1.60 |
| front | 5 | 3.36 2.29 1.60 → 3.31 2.31 1.62 |


## Five-seed 77-update session-scale synthetic matrix — post local quality selection

Command: `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh`. This run used the working tree at `e2bfe2c` with the quote-cap-aware local candidate refinement. All 20 scenario/seed cases completed at 40 expiries × 100 quotes and 77 updates each. Every `--check` returned 2, so the script exited 1 for target misses; no execution failures occurred. Timings are milliseconds; other entries are five-seed medians [minimum, maximum].

| Scenario | Warm p50 (ms) | Warm p99 (ms) | Fallback updates (%) | Cap-share Δ (pp) | RMS Δ (warm − cold) | Summed band-loss ratio | RMS excess-IV Δ (vol pt) |
|---|---:|---:|---:|---:|---:|---:|---:|
| quiet | 1.18009 [0.558002, 1.46559] | 61.6793 [1.19237, 81.3492] | 15.5844 [0, 24.6753] | 0 [0, 0] | 0.00649647 [0.000772862, 0.0073946] | unavailable (zero cold reference in some seeds) | 0.00645228 [0.000883023, 0.00722631] |
| normal | 65.4761 [62.5949, 71.0207] | 129.166 [112.791, 423.679] | 81.8182 [71.4286, 89.6104] | -0.0272727 [-0.0967532, 0.0886364] | -0.00000673532 [-0.000225006, 0.000355152] | 1.00027 [0.998634, 1.01007] | -0.00000853157 [-0.00027962, 0.000329237] |
| fast | 47.0763 [38.7107, 53.0169] | 119.206 [98.7199, 125.544] | 87.013 [64.9351, 90.9091] | 0.0844595 [0.0157609, 0.315455] | 0.138624 [0.0497174, 0.724323] | 1.00223 [1.00055, 1.01748] | 0.0702967 [0.0568585, 0.222875] |
| front | 69.8919 [60.3024, 82.0072] | 140 [101.92, 371.471] | 81.8182 [72.7273, 88.3117] | -0.0438312 [-0.0480519, 0.0422078] | -0.00000703597 [-0.000113117, 0.000238674] | 0.999999 [0.999535, 1.00809] | -0.0000120131 [-0.0000955679, 0.00023594] |

Quiet, normal, and front had zero warm failures. Fast had 63 warm failures, 55 cold failures, and 69 unpaired updates across its five seeds. All runs had zero invalid warm surfaces and dense-grid violations. Quiet cold band-loss references were zero in some seeds. Warm p99 and fallback targets remain unmet in every non-quiet scenario; the fast RMS Δ spans 0.0497174–0.724323, above the 0.02 target in all five seeds. The focused refinement does not resolve the Addendum B scenario hard-limit misses.

Cumulative fallback/rejection reason counts across the five seeds:

| Scenario | Infeasible local result | Loss above gate | Calendar repair failed | Bound check failed | Budget exhausted |
|---|---:|---:|---:|---:|---:|
| quiet | 0 | 904 | 0 | 0 | 0 |
| normal | 0 | 5537 | 0 | 0 | 512 |
| fast | 0 | 5855 | 175 | 125511 | 1658 |
| front | 0 | 5579 | 0 | 0 | 518 |

Load averages (1/5/15-minute, before → after):

| Scenario | Seed | Load average before → after |
|---|---:|---|
| quiet | 1 | 1.79 1.84 1.55 → 1.82 1.84 1.55 |
| quiet | 2 | 1.82 1.84 1.55 → 1.91 1.86 1.56 |
| quiet | 3 | 1.91 1.86 1.56 → 1.84 1.85 1.56 |
| quiet | 4 | 1.84 1.85 1.56 → 2.17 1.92 1.58 |
| quiet | 5 | 2.17 1.92 1.58 → 2.38 1.97 1.60 |
| normal | 1 | 2.38 1.97 1.60 → 2.70 2.05 1.63 |
| normal | 2 | 2.70 2.05 1.63 → 2.75 2.09 1.65 |
| normal | 3 | 2.75 2.09 1.65 → 2.62 2.08 1.65 |
| normal | 4 | 2.62 2.08 1.65 → 2.64 2.11 1.67 |
| normal | 5 | 2.64 2.11 1.67 → 2.62 2.12 1.68 |
| fast | 1 | 2.62 2.12 1.68 → 2.76 2.17 1.70 |
| fast | 2 | 2.76 2.17 1.70 → 2.78 2.18 1.70 |
| fast | 3 | 2.78 2.18 1.70 → 2.97 2.25 1.73 |
| fast | 4 | 2.97 2.25 1.73 → 3.11 2.30 1.75 |
| fast | 5 | 3.11 2.30 1.75 → 3.18 2.33 1.77 |
| front | 1 | 3.18 2.33 1.77 → 3.07 2.33 1.77 |
| front | 2 | 3.07 2.33 1.77 → 2.75 2.30 1.77 |
| front | 3 | 2.75 2.30 1.77 → 2.87 2.34 1.79 |
| front | 4 | 2.87 2.34 1.79 → 3.34 2.47 1.84 |
| front | 5 | 3.34 2.47 1.84 → 3.51 2.54 1.87 |
