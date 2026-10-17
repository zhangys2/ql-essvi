# CAL-B7 — Measured scenarios and full-session replay

## State

- Current: implemented
- Last transition: `implementing` → `implemented`
- At: 2026-10-16
- Reason/evidence: implementation committed as `cc9e36a1831e7f9cd22d59273d88f823c55755b9`; configured final verification passed. The user explicitly waived independent reviews; no review approval is claimed.
- Blocker: None for implementation under the user's waiver.

## State history

| At | Transition | Evidence |
| --- | --- | --- |
| 2026-10-16 | `baseline-checking` → `baseline-passed` | Configured baseline command passed; CTest 5/5 and pytest 6/6. |
| 2026-10-16 | `baseline-passed` → `implementing` | Approved CAL-B7 seam; user directed implementation without further independent reviews. |
| 2026-10-16 | `implementing` → `implemented` | Commit `cc9e36a1831e7f9cd22d59273d88f823c55755b9`; final CTest 5/5 and pytest 10/10. |

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Derive representative five-minute IV level, tilt, and quote-noise changes from the available IBIT and SPY snapshot histories, then use them to set the four synthetic scenarios. Make consecutive snapshot replay the primary realistic benchmark. The replay must preserve timestamps and contract matching, count exclusions and gaps, restart cold after a missing or unusable snapshot, and label the European Black-Scholes stand-in without claiming NBBO or American pricing accuracy.

## Acceptance criteria

- Report per-asset and pooled distributions for common level, tilt, and residual per-quote noise, with the chosen weighting and data coverage documented.
- Set normal from the median absolute move, fast from the 95th percentile, quiet from the 10th percentile, and front from the observed front/back move ratio; record all measured sizes in `docs/perf-results.md`.
- Run every consecutive valid snapshot pair across available IBIT/SPY sessions; missing, skipped, and no-valid-quote snapshots break the chain, trigger a cold restart, and increment a reported restart/gap count.
- Strictly exclude crossed and locked quotes in the replay preparation path and report exclusions per snapshot; leave the general preparation/CVI workflow unchanged.
- Produce the Addendum B quality table per day and cap sensitivity at 0.05, 0.1, and 0.2. Label European Black-Scholes IV as a calibration-path stand-in; do not assert the bands are consolidated NBBO.

## Out of scope

- Replacing the warm gate or local solver (CAL-B2/CAL-B3/CAL-B4).
- Changing shared CSV headers, general `python/prepare_inputs.py`, or CVI behavior.

## Test plan

- Add deterministic tests for supported snapshot discovery, strict locked/crossed filtering and per-snapshot counts, timestamp/contract matching, and cold restart on a gap.
- Run replay against the available IBIT and SPY snapshot directories and inspect per-day outputs and counts.
- Run `scripts/verify_scenarios.sh`, configured CTest, and `.venv/bin/python -m pytest -q` at the ticket boundary.

## Blocked by

- None; CAL-B1 implementation and verification are complete for sequencing under the user's explicit review waiver.

## Expected seam

The benchmark-only snapshot preparation/replay adapter, `bench/essvi_bench.cpp`, `bench/realistic_warm.hpp`, and `docs/perf-results.md`. Keep `python/prepare_inputs.py` and the CVI workflow outside the seam.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-003, DEC-007, DEC-014, DEC-015

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-16 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `1b35f5f` | passed | Release build succeeded; CTest 5/5 and pytest 6/6 passed before B7 changes. |
| 2026-10-16 | Red | Configured full command | `/home/zhangy/git/ql-essvi` | working tree | failed | CTest 4/5; the recalibrated fast scenario no longer exceeded the legacy test's 5% loss allowance. The existing assertion was retained; its fixture now uses the pre-B7 move only as a dedicated gate stress, with zero loss tolerance. |
| 2026-10-16 | Red | Configured full command | `/home/zhangy/git/ql-essvi` | working tree | failed | After CTest passed, the CLI test's fast-scenario assumption of positive loss over a zero cold reference failed: the measured scenario produced finite ratio `1.2055741142028782`. The positive-over-zero edge remains covered by the exact C++ quality test; the CLI test now checks text/JSON ratio and status consistency. |
| 2026-10-16 | Green | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | `cc9e36a` | passed | Focused warm-calibrator CTest 1/1 passed with the dedicated legacy gate-stress fixture. |
| 2026-10-16 | Green | `.venv/bin/python -m pytest -q tests/test_benchmark_cli.py` | `/home/zhangy/git/ql-essvi` | `cc9e36a` | passed | CLI summary-consistency test passed 1/1; positive-over-zero handling remains covered by `separatesTargetAndHardLimitAndHandlesZeroReference`. |
| 2026-10-16 | Replay | `python/run_snapshot_replay.py --data-root /home/zhangy/vol-rv/data --output-dir build/snapshot-replay --bench build/essvi_bench` | `/home/zhangy/git/ql-essvi` | `cc9e36a` | passed | 620 snapshots: 619 usable, one unusable; 601 warm pairs, 18 cold starts, 8 gap events, 16 missing intervals; 16 locked, 0 crossed, 228,055 other exclusions. Per-day metrics and exclusions are recorded in `docs/perf-results.md` and `build/snapshot-replay/`. |
| 2026-10-16 | Scenario matrix | `scripts/verify_scenarios.sh` | `/home/zhangy/git/ql-essvi` | `cc9e36a` | completed with expected target misses | All 20 cases completed; every `--check` returned 2 for target misses. The script returned 1 after printing the full matrix; results and load averages are recorded in `docs/perf-results.md`. |
| 2026-10-16 | Review waiver | User directive: “no more reviews, just implement” | `/home/zhangy/git/ql-essvi` | n/a | waived | No independent review was requested or performed; no review approval is claimed. |
| 2026-10-16 | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `cc9e36a` | passed | Release build succeeded; CTest 5/5 passed and pytest 10/10 passed. |
| 2026-10-16 | Replay (739-snapshot capture) | `python/run_snapshot_replay.py --data-root /home/zhangy/vol-rv/data --output-dir build/snapshot-replay --bench build/essvi_bench` | `/home/zhangy/git/ql-essvi` | working tree (uncommitted) | completed | Capture contained 739 snapshots (738 usable, one unusable), 720 warm pairs, 18 cold starts, 8 gaps, 16 missing intervals, and 20 locked / 0 crossed / 274,878 other exclusions. Per-day quality and cap-sensitivity values are recorded in `docs/perf-results.md`. This replay predates the final shortcut-threshold refresh; the 739-profile five-seed matrix and configured suite below ran after that source change. |
| 2026-10-16 | Scenario matrix (739 profile) | `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh` | `/home/zhangy/git/ql-essvi` | working tree (uncommitted) | completed; expected target/hard-limit misses | All 20 runs completed and every `--check` returned 2; script exit 1. Fast seeds had 483 unpaired/unscored updates, 456 warm failures, and 451 cold failures. All runs had zero invalid warm surfaces and dense-grid violations; full results and load averages are in `docs/perf-results.md`. |
| 2026-10-16 | Supplemental configured verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree (uncommitted) | passed | Final source configured build succeeded; CTest 5/5 passed and pytest 10/10 passed. Performance-target misses remain documented; no review approval is claimed. |

| 2026-10-17 | Calibration red | `cmake --build build --target essvi_benchmark_quality_tests -j2` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | Added a behavior test for warm-minus-cold RMS acceptance; the focused target failed to compile until the new acceptance helper existed. |
| 2026-10-17 | Calibration green | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_benchmark_quality_tests -j2 && ctest --test-dir build -R essvi_benchmark_quality_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | working tree | passed | CTest 1/1 passed for cold-floor equality, target miss, hard-limit breach, and unpaired quality. |
| 2026-10-17 | Current full-session replay | `export PATH="$PWD/.venv/bin:$PATH"; python/run_snapshot_replay.py --data-root /home/zhangy/vol-rv/data --output-dir /tmp/cal-recalib-replay --bench build/essvi_bench` | `/home/zhangy/git/ql-essvi` | working tree | completed; performance misses remain | 739 snapshots, 720 warm pairs, 18 cold starts, 8 gaps; ten usable days had zero failures/invalid surfaces/dense-grid violations. Fallback remained 94.118–100%, warm p99 6.951–125.139 ms; full table in `docs/perf-results.md`. |
| 2026-10-17 | Scenario matrix (77-update profile) | `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh` | `/home/zhangy/git/ql-essvi` | working tree | completed; expected misses | All 20 cases completed with 77 updates each; every `--check` returned 2 and script exited 1. No invalid warm surfaces or dense-grid violations; fast family had 69 unpaired updates, 63 warm failures, 55 cold failures. Full five-seed medians and load averages are recorded in `docs/perf-results.md`. |
| 2026-10-17 | Scenario matrix rerun (RMS-delta summary) | `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh` | `/home/zhangy/git/ql-essvi` | working tree | completed; expected target misses | All 20 cases completed with 77 updates; every `--check` returned 2 and the script returned 1 for target misses. Output now reports the RMS warm-minus-cold delta. Latest timing and load-average records are in `docs/perf-results.md`. |
| 2026-10-17 | Final configured suite | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release configure/build succeeded; CTest 5/5 passed and pytest 10/10 passed after the 77-update/RMS calibration changes. The scenario matrix retains documented target/hard misses. |
| 2026-10-17 | Final configured suite after RMS-summary update | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Final configure/build succeeded after the scenario summary change; CTest 5/5 passed and pytest 10/10 passed. Scenario target misses remain recorded separately. |
| 2026-10-17 | Default-length smoke | `./build/essvi_bench --scenario quiet --expiries 1 --quotes-per-expiry 100 --seed 1 --timings-out <tmp>` | `/home/zhangy/git/ql-essvi` | working tree | passed | With no `--scenario-updates` argument, timings JSON reported `updates=77`. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |

## Delivery record

- Base commit: `1b35f5f71838ae6000bc5b593323ff6f44a952e8`
- Baseline status: passed
- Baseline verification:
  - Configured CMake build, CTest, and pytest command — passed (CTest 5/5; pytest 6/6)
- Baseline failure evidence: None
- Unconfirmed commands: typecheck/lint not applicable; no configured linter
- Branch: `delivery/calibrator-speedup-addendum-b/CAL-B7`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: `cc9e36a1831e7f9cd22d59273d88f823c55755b9`
- GitHub Issue: none (local tracker)
- Tests: final configured CTest 5/5 and pytest 10/10 pass; scenario target misses documented.
- Diff review: explicitly waived by user; no independent review or approval claimed.
