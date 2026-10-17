# CAL-B1 — Honest quality metrics and `49fed2b` baseline

## State

- Current: blocked
- Last transition: `changes-requested` → `blocked`
- At: 2026-10-16T12:55:26Z
- Reason/evidence: Round-3 coverage findings were corrected in `a556889611a276a6cb75f47bb0e5a626b819f026`; the configured full command passes (CTest 5/5, pytest 6/6). The review cap remains reached, so these follow-up corrections have no permitted independent review and the ticket cannot be approved or finished.
- Blocker: Independent review cap reached; follow-up fixes require an allowed review path before approval.

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

Fresh independent review of the post-round-3 corrections is required, but the review procedure allows no fourth round.

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
| 2026-10-16T12:27:24Z | Review round 2 start | Final review range `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536..c3849a5`; three independent axes | `/home/zhangy/git/ql-essvi` | `c3849a5` | reviewing | All round-1 findings corrected and verified; fresh independent review pending. |
| 2026-10-16T12:36:01Z | Review round 2 | Base-to-HEAD review `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536..c3849a5`; three independent axes | `/home/zhangy/git/ql-essvi` | `c3849a5` | changes required | Spec axis approved. Standards flagged a timing-dependent exit-zero CLI test; verification flagged missing coverage for zero expected quote count. Review verification passed CTest 5/5 and pytest 7/7. |
| 2026-10-16T12:41:46Z | Review round 2 corrections | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_bench essvi_benchmark_quality_tests -j2 && ctest --test-dir build -R essvi_benchmark_quality_tests --output-on-failure && .venv/bin/python -m pytest -q tests/test_benchmark_cli.py` | `/home/zhangy/git/ql-essvi` | `439327d` | passed | Build succeeded; focused CTest 1/1 and CLI pytest 1/1 passed. The C++ test covers zero expected quote count with no aggregate mutation and deterministic pass/miss/unavailable exit-decision cases. |
| 2026-10-16T12:41:46Z | Passing CLI smoke | `./build/essvi_bench --scenario quiet --scenario-updates 1 --seed 1 --expiries 1 --quotes-per-expiry 1 --check` | `/home/zhangy/git/ql-essvi` | `439327d` | passed | Real CLI returned exit 0; all printed target and hard-limit statuses were `PASS`. Timing is recorded only as smoke evidence, not pinned by a permanent test. |
| 2026-10-16T12:41:46Z | Final verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `439327d` | passed | Release build succeeded; CTest 5/5 passed; pytest 6/6 passed. |
| 2026-10-16T12:42:32Z | Review round 3 start | Final review range `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536..c15ef8e48e0bac5a1ff2c13c7e1bb7327165756c`; three independent axes | `/home/zhangy/git/ql-essvi` | `c15ef8e` | reviewing | Round-2 findings resolved; fresh spec, standards, and verification review pending. |
| 2026-10-16T12:54:40Z | Review round 3 | Base-to-HEAD review `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536..c15ef8e48e0bac5a1ff2c13c7e1bb7327165756c`; three independent axes | `/home/zhangy/git/ql-essvi` | `c15ef8e` | changes required | Spec and standards approved. The configured full command passed CTest 5/5 and pytest 6/6. Verification identified two P2 test gaps: the production CLI all-pass exit-0 mapping and all aggregate fields on the zero-expected-count no-mutation branch. |
| 2026-10-16T12:55:26Z | Review cap | Round 3 completed | `/home/zhangy/git/ql-essvi` | `c15ef8e` | blocked | The review procedure allows at most three rounds. The round-3 P2 findings remained unreviewed; no approval or handoff recorded. |
| 2026-10-16T13:32:22Z | Post-review remediation (unreviewed) | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `a556889` | passed | Added `qualityCheckExitCode`, used by `main` and tested for all-pass 0 and target-miss 2; zero-count test now uses nonzero sentinels and asserts every aggregate field unchanged. Release build succeeded; CTest 5/5 and pytest 6/6 passed. |
| 2026-10-16T13:32:22Z | Passing CLI smoke after remediation | `./build/essvi_bench --scenario quiet --scenario-updates 1 --seed 1 --expiries 1 --quotes-per-expiry 1 --check` | `/home/zhangy/git/ql-essvi` | `a556889` | passed | Real CLI returned exit 0; all reported target and hard-limit statuses were `PASS`. The permanent regression covers the production exit-code helper deterministically. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |
| Pre-review | P2: equal aggregate warm/cold quote counts could still compare different updates or populations; empty-pair comparisons were not consistently unavailable. | Resolved | Per-update full-quote-set pairing, paired-update coverage enforcement, and empty-comparison guards added; reviewer recheck confirmed the same-update/same-quote mismatch is closed. |
| 1 — Spec P2 | Positive warm loss over zero cold loss was marked unavailable, hiding a definite breach. | Resolved | `checkRatioAtMost` now returns available `+inf` for positive/zero; check table reports target miss and hard breach. The CLI regression verifies `inf | MISS | BREACH`; JSON preserves valid syntax using `null`; zero/zero remains 1. |
| 1 — Standards P2 | Pairing guard and benchmark aggregation lacked automated test coverage. | Resolved | `mergePairedQuoteMetrics`, called by `record()`, rejects equal-but-incomplete and asymmetric update counts without mutating aggregates; CTest regression verifies coverage, paired-update count, and no partial merge. |
| 1 — Verification P2 | Successful `--check` CLI path and exit 0 were untested. | Resolved | The permanent test covers the deterministic shared target-miss decision helper for passing, missed, and unavailable checks; real CLI exit 0 was exercised as a smoke, and a separate fast case asserts exit 2 on a target miss. Machine-dependent timing is not asserted. |
| 2 — Standards P2 (judgment) | Successful CLI test asserts measured one-sample quiet timing remains under the performance target; host scheduling can cause false failures unrelated to exit-status behavior. | Resolved | Removed the timing-pinned CLI success test; `hasTargetMiss` is tested for all-pass, target-miss, and unavailable check lists. A one-off real CLI smoke returned 0 with all statuses passing. |
| 2 — Verification P2 (UNTESTED) | `mergePairedQuoteMetrics` has an `expected_quote_count == 0` rejection branch with no test; existing empty-metric tests do not call the helper with zero expected count. | Resolved | CTest now calls the helper with zero expected quote count and asserts false return, zero `paired_updates`, and unchanged warm/cold aggregate quote counts. |
| 3 — Verification P2 (UNTESTED) | No deterministic test exercises the production `--check` all-pass decision that returns exit 0. The unit test covers `hasTargetMiss(false)` but not `main`'s mapping from target status to process exit code; the CLI regression only asserts exit 2 on a miss. | Corrected, not independently re-reviewed | Commit `a556889` adds and tests `qualityCheckExitCode`; `main` calls this helper and the C++ regression verifies all-pass returns 0, target miss returns 2, and unavailable remains a nonzero miss. No fourth review allowed. |
| 3 — Verification P2 (UNTESTED) | The zero-expected-count test checks `paired_updates` and warm/cold quote counts only; it cannot detect partial mutation of other aggregate fields. | Corrected, not independently re-reviewed | Commit `a556889` seeds every aggregate field and nonzero input metrics, then asserts `checks`, `paired_updates`, and every warm/cold metric field are unchanged. No fourth review allowed. |
| 3 — Review cap | A fourth independent review would exceed the configured maximum of three rounds. | Blocked | Post-review corrections pass configured verification, but cannot receive the fresh independent review required for approval under the current cap. |

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
- Commits: `3fd4fe3` (implementation), `c3849a5` (review round-1 corrections), `439327d` (review round-2 corrections), `c15ef8e` (review record), `46a46da` (review blocker record), `a556889` (post-review test-gap corrections, not independently reviewed)
- GitHub Issue: none (local tracker)
- Tests: configured CTest 5/5 and pytest 6/6 pass; scenario matrix target/coverage misses are documented. Post-review fixes remain unreviewed.
- Diff review: blocked after round 3; no review approval or finished handoff.
