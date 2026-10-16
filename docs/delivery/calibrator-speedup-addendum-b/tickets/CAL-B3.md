# CAL-B3 — Better local solver and fallback reasons

## State

- Current: implementing
- Last transition: `baseline-passed` → `implementing`
- At: 2026-10-16
- Reason/evidence: CAL-B2 completed at `00f4db21e1f778ce90b54bb12514cb9ba5dc8cb1`; configured baseline passed at that commit; the user waived independent reviews.
- Blocker: None

## State history

| At | Transition | Evidence |
| --- | --- | --- |
| 2026-10-16 | `ready` → `baseline-checking` | Claimed by omp coding assistant; primary-checkout branch `delivery/calibrator-speedup-addendum-b/CAL-B3` at the CAL-B2 commit. |
| 2026-10-16 | `baseline-checking` → `baseline-passed` | Configured full verification passed at base `00f4db21e1f778ce90b54bb12514cb9ba5dc8cb1`; CTest 5/5 and pytest 10/10. |
| 2026-10-16 | `baseline-passed` → `implementing` | Baseline evidence recorded; B3 seam confirmed; user waived independent reviews. |

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

## Out of scope

- Per-quote gate definition and reuse policy (CAL-B2).
- Budgeted staged fallback and degraded state (CAL-B4).

## Test plan

- Add deterministic behavior tests for all three starts, ATM initialization, early stop, evaluation bound, fallback reason categories, feasibility rejection, and repeatability through the calibrator seam.
- Run focused warm CTest and configured full CTest/Python suites.
- Re-run the four-scenario/five-seed benchmark to show reason histograms and quality/latency after the local solver change.

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

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |

## Delivery record

- Base commit: `00f4db21e1f778ce90b54bb12514cb9ba5dc8cb1`
- Baseline status: passed
- Baseline verification:
  - `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` (cwd: `/home/zhangy/git/ql-essvi`) — passed (CTest 5/5; pytest 10/10)
- Baseline failure evidence: None
- Unconfirmed commands: typecheck/lint not applicable; no configured linter
- Branch: `delivery/calibrator-speedup-addendum-b/CAL-B3`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: pending
- GitHub Issue: none (local tracker)
- Tests: pending
- Diff review: pending
