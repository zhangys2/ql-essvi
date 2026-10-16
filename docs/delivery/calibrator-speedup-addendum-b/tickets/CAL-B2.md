# CAL-B2 — Per-quote cap gate and reuse drift

## State

- Current: implementing
- Last transition: `baseline-passed` → `implementing`
- At: 2026-10-16
- Reason/evidence: exact B2 base `21e531b` passed configured verification; the per-quote gate/reuse seam is confirmed. The user explicitly waived independent reviews; no review approval is claimed.
- Blocker: None under the user's review waiver.

## State history

| At | Transition | Evidence |
| --- | --- | --- |
| 2026-10-16T11:24:10Z | `draft` → `ready` | Approved implementation spec and ticket acceptance criteria. |
| 2026-10-16 | `ready` → `baseline-checking` | Claimed by omp coding assistant; configured primary-checkout branch `delivery/calibrator-speedup-addendum-b/CAL-B2`. |
| 2026-10-16 | `baseline-checking` → `baseline-passed` | Configured full verification passed at declared base `21e531b`. |
| 2026-10-16 | `baseline-passed` → `implementing` | Baseline passed; B2 seam confirmed; user waived independent reviews. |

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Replace the `loss_tol * bandLossScale` acceptance and reuse tests with the approved per-quote total-variance excess-ratio cap. Cache a same-snapshot full-cold reference only when a candidate fails the base cap, apply the per-quote `max(cap, cold_ratio + 0.01)` threshold, and bound unchanged-slice reuse by the approved drift limit. Preserve `loss_tol` only as deprecated compatibility state; remove the scheduled `cold_check_interval` path as approved in the spec.

## Acceptance criteria

- `WarmOptions::quote_cap` defaults to 0.1 and supports sensitivity values 0.05, 0.1, and 0.2.
- Width is `max(ask_variance - bid_variance, 1e-6)`; per-quote excess and ratio follow the parent spec. A candidate passes the base gate only when each included quote is within its own cap.
- If a local/reuse candidate fails the base cap, the restricted then full-cold sequence and per-quote adaptive threshold follow the approved spec; same-snapshot full-cold ratios are cached and never substituted by prior-snapshot ratios.
- Reuse also passes the per-quote gate, calendar/predecessor checks, and the maximum 12 consecutive-update drift limit.
- `loss_tol` remains available but cannot affect gate/reuse acceptance; the obsolete periodic cold-check option is removed and callers migrated.
- Existing test expectations are not changed silently. If an existing assertion conflicts with the approved gate, the conflict is recorded and handled only as a named, separately justified change.


## Named test expectation adjustments

- The former `qualityGateFallsBackToColdSearch` assertion required `loss_tol * bandLossScale`; that gate is retired by DEC-002. `perQuoteGateChecksSingleOutlierAgainstColdReference` now tests the single-quote cap failure, restricted/cold-reference sequence, adaptive per-quote threshold, and invariance under a different `loss_tol`.
- `warmUpdatesReuseAndRefitSafely` now carries the previous `CalibrationResult` state and expects unchanged slices to refit after 12 consecutive reuses. This is the explicit DEC-012 limit, not a relaxed expectation.
- `localWarmRefitHonorsBudgetAndQuality` uses `quote_cap=0.5` to isolate its one-slice local-refit budget from unrelated slices rejected by the default 0.1 gate. `failedJacobiPrefitFallsBackWithoutRetry` uses `quote_cap=1.0` so an unchanged slice does not add an unrelated local fit. Default-cap, boundary, outlier, adaptive-reference, width-floor, and drift behavior remain covered separately.

## Out of scope

- Snapshot-derived scenario measurements/replay (CAL-B7), solver quality improvements (CAL-B3), and staged/degraded fallback (CAL-B4).

## Test plan

- Add focused public-seam tests for per-quote boundary equality, a single outlier quote, width-floor handling, adaptive cold-reference behavior/cache, and reuse at the drift limit.
- Run `ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` after the build and at the ticket boundary.
- Run the configured CTest and `.venv/bin/python -m pytest -q` suites at the ticket boundary.

## Blocked by

- None; CAL-B7 is implemented and committed as `21e531b` with the user's review waiver.

## Expected seam

`cpp/calibrator.hpp`, `cpp/calibrator.cpp`, all in-repository `calibrateWarm` callsites, and `tests/test_warm_calibrator.cpp`. Tests use `calibrateWarm` through its public result/state transition.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-007, DEC-011, DEC-012

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-16 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `21e531b` | passed | Release build succeeded; CTest 5/5 passed and pytest 10/10 passed. Configured full command is proven able to fail. |
| 2026-10-16 | Red | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_warm_calibrator_tests -j2` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | The new single-outlier per-quote gate test failed to compile because `WarmOptions::quote_cap` was absent; the missing public cap is the behavior seam fixed by DEC-002/DEC-011. |
| 2026-10-16 | Green | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_warm_calibrator_tests essvi_bench -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | working tree | passed | Focused warm tests passed after per-quote, restricted-stage, adaptive-reference, and reuse-age changes. |
| 2026-10-16 | Sensitivity smoke | `build/essvi_bench --scenario quiet --scenario-updates 15 --expiries 3 --quotes-per-expiry 100 --seed 7 --quote-cap 0.05 --timings-out build/wp-b2-cap-0.05.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | 15/15 fallback-path updates; zero warm/cold failures, invalid surfaces, and dense-grid violations. Recorded in `docs/perf-results.md`. |
| 2026-10-16 | Sensitivity smoke | `build/essvi_bench --scenario quiet --scenario-updates 15 --expiries 3 --quotes-per-expiry 100 --seed 7 --quote-cap 0.1 --timings-out build/wp-b2-cap-0.1.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | 15/15 fallback-path updates; zero warm/cold failures, invalid surfaces, and dense-grid violations. Recorded in `docs/perf-results.md`. |
| 2026-10-16 | Sensitivity smoke | `build/essvi_bench --scenario quiet --scenario-updates 15 --expiries 3 --quotes-per-expiry 100 --seed 7 --quote-cap 0.2 --timings-out build/wp-b2-cap-0.2.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | 15/15 fallback-path updates; zero warm/cold failures, invalid surfaces, and dense-grid violations. Recorded in `docs/perf-results.md`. |
| 2026-10-16 | Configured verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release build; CTest 5/5 passed; pytest 10/10 passed. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |

## Delivery record

- Base commit: `21e531b753b18af4ff74a3a10f5d78b3bcbe44d8`
- Baseline status: passed
- Baseline verification:
  - `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` (cwd: `/home/zhangy/git/ql-essvi`) — passed (CTest 5/5; pytest 10/10)
- Baseline failure evidence: None
- Unconfirmed commands: typecheck/lint not applicable; no configured linter
- Branch: `delivery/calibrator-speedup-addendum-b/CAL-B2`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: pending
- GitHub Issue: none (local tracker)
- Tests: configured CTest 5/5 and pytest 10/10 passed; focused warm test and 0.05/0.1/0.2 cap smoke runs passed.
- Diff review: explicitly waived by user; no independent review or approval claimed.
