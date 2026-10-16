# CAL-B1 — Honest quality metrics and `49fed2b` baseline

## State

- Current: implemented
- Last transition: `changes-requested` → `implemented`
- At: 2026-10-16T12:26:34Z
- Reason/evidence: all three review-round-1 findings corrected; focused C++ aggregation and ratio tests, both CLI exit-path regressions, full CTest, and pytest pass.
- Blocker: None

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Extend the market-like benchmark to report the Addendum B quality metrics for warm and cold results, produce target/hard-limit status with `--check`, and run the four deterministic scenarios for five fixed seeds and 200 updates per seed. Record the `49fed2b` baseline before changing the solver or gate. Baseline reporting must distinguish the currently implemented loss-scale gate from post-hoc per-quote cap metrics; it must not imply that the new gate is implemented in this slice.

## Acceptance criteria

- Per-quote ratio share above 0.1, maximum and RMS ratio, warm/cold summed band loss, and per-quote RMS excess in IV points are reported with quote counts.
- `--check` prints each Addendum B §3 target and hard-limit result and exits non-zero if any target is missed; hard-limit breaches are separately identified.
- `scripts/verify_scenarios.sh` runs quiet, normal, fast, and front for five fixed/reportable seeds, 200 updates each, and reports median/range plus load averages.
- Baseline output records the old gate result and the new cap metrics side-by-side on the approved base tree (identical to `49fed2b`); no solver/gate behavior is changed here.
- Existing CSV headers and D17-D22 behavior remain unchanged.

## Out of scope

- Measuring snapshot-derived scenario sizes or adding full-session replay (CAL-B7).
- Changing the calibrator gate or solver (CAL-B2/CAL-B3/CAL-B4).

## Test plan

- Build from the repository root using configured CMake commands.
- Test per-quote variance-ratio/IV-point calculations with exact synthetic inputs and test target/hard-limit status through the benchmark quality seam.
- Exercise `--check` on deterministic synthetic scenarios and report any target-miss exit honestly.
- Run `scripts/verify_scenarios.sh` with all four scenarios and five seeds.
- Run `ctest --test-dir build --output-on-failure` and `.venv/bin/python -m pytest -q` at the ticket boundary.

## Blocked by

None

## Expected seam

`bench/essvi_bench.cpp`, a shared benchmark quality-metrics header, `tests/test_benchmark_quality.cpp`, a new `scripts/verify_scenarios.sh`, and `docs/perf-results.md`. The externally observable seam is the benchmark CLI/report and script exit status.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-003, DEC-007, DEC-010

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-16T11:26Z | Baseline attempt | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` (initial invocation set `.venv/bin` only for the first command) | `/home/zhangy/git/ql-essvi` | `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536` | failed (invocation) | `ctest: command not found`, exit 127; no test ran. Corrected by exporting `.venv/bin` for the complete command. |
| 2026-10-16T11:27Z | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536` | passed | Build succeeded; CTest 4/4 passed; pytest 5/5 passed. Configured full command is proven able to fail. |
| 2026-10-16T11:27Z | Baseline attempt | `ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536` | failed (invocation) | `ctest: command not found`, exit 127; no test ran. Corrected by putting `.venv/bin` on `PATH`. |
| 2026-10-16T11:28Z | Baseline | `PATH="$PWD/.venv/bin:$PATH" ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536` | passed | Focused warm calibrator CTest passed 1/1. Configured focused command is proven able to fail. |
| 2026-10-16T11:29Z | Baseline measurement | `build/essvi_bench --scenario fast --scenario-updates 1 --seed 2` | `/home/zhangy/git/ql-essvi` | `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536` | passed | Unchanged benchmark reported one fallback update/one sample, 29 fallback slices, warm loss `0.00016033723793002313`, cold loss `0`; demonstrates a target-miss fixture and zero-denominator edge for loss ratio. |
| 2026-10-16T11:31Z | Red | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build --target essvi_benchmark_quality_tests -j2` | `/home/zhangy/git/ql-essvi` | `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536` | expected failure | New public-seam test failed to compile because `quality_metrics.hpp` does not exist; red checkpoint for the per-quote metrics API. |
| 2026-10-16 | Green | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_bench essvi_benchmark_quality_tests -j2 && ctest --test-dir build -R essvi_benchmark_quality_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | working tree | passed | Benchmark and focused metrics test built; CTest 1/1 passed, including exact variance-ratio, IV-point, width-floor, quote-validity, paired-count, target/hard-limit, and zero-reference cases. |
| 2026-10-16 | Pairing correction | Reviewer noted aggregate equal counts do not prove same-update/same-quote populations and empty comparisons were not guarded. | `/home/zhangy/git/ql-essvi` | working tree | resolved | Metrics now merge only when both valid surfaces cover the full valid two-sided quote set for that update; `paired_updates` is tracked, empty comparisons are unavailable, and `--check` fails unpaired or unscored updates. Reviewer recheck found no remaining population mismatch. |
| 2026-10-16 | Scenario matrix | `./scripts/verify_scenarios.sh` | `/home/zhangy/git/ql-essvi` | working tree | completed with expected target/coverage misses | All 20 cases completed; each individual `--check` returned 2, with no execution failures. 3,993/4,000 updates had full paired quote coverage; fast seeds 2 and 3 had 197/200 and 196/200. The script returned 1 after printing all metrics and load averages; exact results are in `docs/perf-results.md`. |
| 2026-10-16 | Review round 1 | Base-to-HEAD review `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536..3fd4fe3`; three independent axes | `/home/zhangy/git/ql-essvi` | `3fd4fe3` | changes requested | Spec flagged positive-over-zero as an unavailable ratio instead of a definite breach; standards flagged missing aggregation-seam coverage; verification flagged no passing `--check` CLI case. Its configured full command passed CTest 5/5 and pytest 5/5. |
| 2026-10-16 | Review corrections | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_bench essvi_benchmark_quality_tests -j2 && ctest --test-dir build -R essvi_benchmark_quality_tests --output-on-failure && .venv/bin/python -m pytest -q tests/test_benchmark_cli.py` | `/home/zhangy/git/ql-essvi` | working tree | passed | Focused CTest 1/1 and CLI pytest 2/2 passed. Tests cover incomplete/asymmetric paired counts, zero/zero ratio 1, positive/zero `inf` as `MISS | BREACH` with JSON `null`, `--check` exit 0 for a passing one-quote quiet case, and exit 2 for a fast target miss. |
| 2026-10-16 | Final verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release build succeeded; CTest 5/5 passed; pytest 7/7 passed. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |
| Pre-review | P2: equal aggregate warm/cold quote counts could still compare different updates or populations; empty-pair comparisons were not consistently unavailable. | Resolved | Per-update full-quote-set pairing, paired-update coverage enforcement, and empty-comparison guards added; reviewer recheck confirmed the same-update/same-quote mismatch is closed. |
| 1 — Spec P2 | Positive warm loss over zero cold loss was marked unavailable, hiding a definite breach. | Resolved | `checkRatioAtMost` now returns available `+inf` for positive/zero; check table reports target miss and hard breach. The CLI regression verifies `inf | MISS | BREACH`; JSON preserves valid syntax using `null`; zero/zero remains 1. |
| 1 — Standards P2 | Pairing guard and benchmark aggregation lacked automated test coverage. | Resolved | `mergePairedQuoteMetrics`, called by `record()`, rejects equal-but-incomplete and asymmetric update counts without mutating aggregates; CTest regression verifies coverage, paired-update count, and no partial merge. |
| 1 — Verification P2 | Successful `--check` CLI path and exit 0 were untested. | Resolved | Pytest invokes the built CLI for a deterministic passing one-quote quiet scenario and asserts exit 0/all PASS; a separate fast case asserts exit 2 on a target miss. |

## Delivery record

- Base commit: `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536`
- Baseline status: passed
- Baseline verification:
  - Full configured CMake build, CTest, and pytest command with `.venv/bin` on `PATH` — passed (CTest 4/4; pytest 5/5)
  - Focused `essvi_warm_calibrator_tests` with `.venv/bin` on `PATH` — passed (1/1)
- Baseline failure evidence: two initial invocations lacked `.venv/bin` on `PATH`; both stopped with `ctest: command not found` before tests ran; corrected runs passed.
- Unconfirmed commands: none (typecheck/lint not applicable)
- Branch: `delivery/calibrator-speedup-addendum-b/CAL-B1`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: pending review corrections
- GitHub Issue: none (local tracker)
- Tests: final CTest 5/5 and pytest 7/7 passed; scenario matrix target/coverage misses are documented.
- Diff review: review round 1 changes all addressed; round 2 pending.
