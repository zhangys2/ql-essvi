# CAL-B3 — Better local solver and fallback reasons

## State

- Current: implemented
- Last transition: `implementing` → `implemented`
- At: 2026-10-16
- Reason/evidence: implementation committed at `c5a7aca2bf565bd78aa0e81baa1231fa923b2aed`; configured verification passed (CTest 5/5, pytest 10/10); scenario target misses are documented; the user waived further independent reviews.
- Blocker: None

## State history

| At | Transition | Evidence |
| --- | --- | --- |
| 2026-10-16 | `ready` → `baseline-checking` | Claimed by omp coding assistant; primary-checkout branch `delivery/calibrator-speedup-addendum-b/CAL-B3` at the CAL-B2 commit. |
| 2026-10-16 | `baseline-checking` → `baseline-passed` | Configured full verification passed at base `00f4db21e1f778ce90b54bb12514cb9ba5dc8cb1`; CTest 5/5 and pytest 10/10. |
| 2026-10-16 | `baseline-passed` → `implementing` | Baseline evidence recorded; B3 seam confirmed; user waived independent reviews. |
| 2026-10-16 | `implementing` → `implemented` | CAL-B3 committed at `c5a7aca2bf565bd78aa0e81baa1231fa923b2aed`; configured CTest and pytest suites passed; benchmark completed with explicit target misses; independent review waived by user. |

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Improve changed-slice local fitting with deterministic multi-start in the bounded previous-rho window, an ATM-anchored theta start, Levenberg-Marquardt damping, zero-loss early stop, and an approximately 25-objective-evaluation per-slice budget. Count and expose the specified fallback/rejection reasons. Never accept an infeasible candidate, and preserve deterministic reduction with no more than four threads.

## Acceptance criteria

- The local search uses three deterministic starts within previous rho ±0.05 and respects the feasible psi interval and all existing butterfly/calendar constraints.
- Theta starts from the nearest ATM quote as specified; damping and zero-loss early termination are active; objective evaluations are bounded at about 25 per slice.
- Result/bench output includes counts for infeasible local result, loss above gate, calendar repair failed, and bound check failed; scenario reports include reason histograms.
- Repeated identical inputs return identical parameters and diagnostics relevant to search ordering; no infeasible point is accepted.
- Local candidate quality and all gate decisions continue to satisfy CAL-B2; no D17-D22 semantics change.
- Within the existing local-evaluation budget, prefer the lowest raw-`bandLoss` candidate that passes CAL-B2's per-quote cap over lower-loss gate failures; keep the reported objective as raw `bandLoss`.

## Out of scope

- Per-quote gate definition and reuse policy (CAL-B2).
- Budgeted staged fallback and degraded state (CAL-B4).

## Test plan

- Add deterministic behavior tests for all three starts, ATM initialization, early stop, evaluation bound, fallback reason categories, feasibility rejection, and repeatability through the calibrator seam.
- Add a conflicting-band fixture proving a near-optimal cap-passing local trial avoids fallback, has higher raw band loss than the relaxed-cap local minimum but stays within 5%, and satisfies the dense-grid constraints.
- Run focused warm CTest and configured full CTest/Python suites.
- Re-run the four-scenario/five-seed benchmark to show reason histograms and quality/latency after the local solver change.

## Named test expectation adjustments

- `localWarmRefitHonorsBudgetAndQuality` replaces its former `<=15` local-evaluation assertion with exact 2 for this zero-loss fixture: one feasible incumbent objective plus one ATM-anchored start, then the required early stop. The separate noisy multi-start test requires all three starts and enforces the 25-evaluation cap; the existing quality comparison remains unchanged.

## Blocked by

- CAL-B2 — per-quote gate and reuse semantics (completed at `00f4db21e1f778ce90b54bb12514cb9ba5dc8cb1`).

## Expected seam

`cpp/calibrator.cpp`, `cpp/calibrator.hpp` result observability, `tests/test_warm_calibrator.cpp`, and benchmark reason aggregation in `bench/essvi_bench.cpp`.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-005, DEC-007

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-16 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `00f4db21e1f778ce90b54bb12514cb9ba5dc8cb1` | passed | Release build succeeded; CTest 5/5 passed and pytest 10/10 passed. |
| 2026-10-16 | Red | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_warm_calibrator_tests -j2` | `/home/zhangy/git/ql-essvi` | `00f4db21e1f778ce90b54bb12514cb9ba5dc8cb1` | expected failure | Compilation failed because the public result lacks the requested local-start and fallback-reason diagnostics; focused assertions express the CAL-B3 contract. |
| 2026-10-16 | Green | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_warm_calibrator_tests essvi_bench -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | `c5a7aca2bf565bd78aa0e81baa1231fa923b2aed` | passed | Focused tests passed for three bounded starts, 25-evaluation budget, ATM zero-loss stop, repeatability, feasibility rejection, all four reason categories, and existing CAL-B2 quality checks. |
| 2026-10-16 | Scenario benchmark | `scripts/verify_scenarios.sh` | `/home/zhangy/git/ql-essvi` | `c5a7aca2bf565bd78aa0e81baa1231fa923b2aed` | target misses (exit 1) | Completed all 20 runs (four scenarios × five seeds × 200 updates); all `--check` calls returned 2 for target misses, not execution failures. The result table and reason histograms are in `docs/perf-results.md`; fast seeds 2–5 had warm failures and unpaired updates. |
| 2026-10-16 | Configured verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `c5a7aca2bf565bd78aa0e81baa1231fa923b2aed` | passed | Release build succeeded; CTest 5/5 passed; pytest 10/10 passed. |

| 2026-10-17 | Supplemental solver baseline | `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh` | `/home/zhangy/git/ql-essvi` | working tree at `e2bfe2c` | completed; target misses | Pre-refinement 739-profile matrix: all 20 cases completed with 77 updates, each `--check` returned 2, script exit 1 for target misses. No invalid warm surfaces or dense-grid violations. Baseline p99 medians [min,max]: quiet 56.833 [2.217,77.272] ms; normal 132.795 [113.199,264.957] ms; fast 125.587 [104.016,198.609] ms; front 130.049 [104.735,261.519] ms. Full matrix and load averages are in `docs/perf-results.md`. |
| 2026-10-17 | Supplemental refinement Red | `make -C build essvi_warm_calibrator_tests -j2 && build/essvi_warm_calibrator_tests` | `/home/zhangy/git/ql-essvi` | working tree at `e2bfe2c` with focused test added | expected failure | `localSolverKeepsNearOptimalCapPassingCandidate` failed at `warm.fallback_slices == 0`; local raw-loss selection retained a per-quote-cap failure and entered cold fallback. |
| 2026-10-17 | Supplemental refinement Green | `make -C build essvi_warm_calibrator_tests -j2 && build/essvi_warm_calibrator_tests` | `/home/zhangy/git/ql-essvi` | working tree with quote-cap-aware candidate selection | passed | Strict-cap candidate has higher raw `bandLoss` than the relaxed-cap local minimum but stays within 5%; both pass without fallback and satisfy dense-grid checks. Existing local budget and repeatability assertions also pass. |
| 2026-10-17 | Supplemental configured verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree with quote-cap-aware candidate selection | passed | Release configure/build passed; CTest 5/5 passed and pytest 10/10 passed. |
| 2026-10-17 | Supplemental scenario benchmark | `export PATH="$PWD/.venv/bin:$PATH"; ./scripts/verify_scenarios.sh` | `/home/zhangy/git/ql-essvi` | working tree at `e2bfe2c` with quote-cap-aware selection | target misses (exit 1) | All 20 cases completed; every `--check` returned 2, with no execution failures. Fast had 63 warm failures, 55 cold failures, and 69 unpaired updates; quiet, normal, and front had zero warm failures. No invalid warm surfaces or dense-grid violations. Updated metrics and reason histograms are in `docs/perf-results.md`. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |
| Waived | Independent review | User requested no further reviews | No independent review performed; the user said ‘no more reviews, just implement’. |

## Delivery record

- Base commit: `00f4db21e1f778ce90b54bb12514cb9ba5dc8cb1`
- Baseline status: passed
- Baseline verification:
  - `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` (cwd: `/home/zhangy/git/ql-essvi`) — passed (CTest 5/5; pytest 10/10)
- Baseline failure evidence: None
- Unconfirmed commands: typecheck/lint not applicable; no configured linter
- Branch: `delivery/calibrator-speedup-addendum-b/CAL-B3`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: `c5a7aca2bf565bd78aa0e81baa1231fa923b2aed`
- GitHub Issue: none (local tracker)
- Tests: the configured full command passed after this working-tree change: Release build, CTest 5/5, and pytest 10/10. The supplemental warm-only ATM-seed regression failed before and passed after; it asserts identical parameters when the nonbinding nearest-ATM quote's mid changes while all bands remain identical. The post-change 1 × 100 fast smoke reports RMS excess ratio 1.80174, maximum 4.04763, 1/1 fallback, and zero failures, invalid surfaces, or dense-grid violations. Addendum B hard limits remain unmet; see `docs/perf-results.md`.
- Supplemental 2026-10-17 verification: focused warm calibrator executable passed; configured Release build, CTest 5/5, and pytest 10/10 passed. All 20 739-profile 77-update scenario cases completed; every `--check` returned 2 and the script exited 1 for target misses, with no execution failures. Fast had 63 warm failures, 55 cold failures, and 69 unpaired updates; all runs had zero invalid warm surfaces and dense-grid violations. See `docs/perf-results.md` for full metrics and reason histograms.
- Diff review: waived at user request; no independent review performed.