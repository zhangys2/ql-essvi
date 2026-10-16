# CAL-B5 — Addendum B regression coverage

## State

- Current: implementing
- Last transition: `ready` → `baseline-passed` → `implementing`
- At: 2026-10-16
- Reason/evidence: configured baseline passed at B4 completion commit `186d03f63dcf0d495052742a81c5648fdfbe36ec`; CAL-B4 dependency complete.
- Blocker: None

## Claim

None

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Add a deterministic four-scenario sequence of 50 updates per scenario that fails when the Addendum B hard quality limits are exceeded, plus direct behavior coverage for valid degraded results and their reported counts/ages. Preserve all existing test expectations unless a named conflict is explicitly resolved.

## Acceptance criteria

- The scripted quiet, normal, fast, and front sequences each run 50 updates deterministically and compare observable quality against Addendum B hard limits.
- Regression failure reports the scenario, seed, update, metric, observed value, and applicable hard limit.
- Degraded-result tests assert `path=degraded`, valid surface, correct degraded counts/ages, and bounded fallback behavior; no test merely checks non-throw or incidental defaults.
- Existing expectations are preserved; any conflict with the approved new gate is named, justified against DEC-007, and separately resolved before changing that expectation.

## Out of scope

- Changing solver behavior or acceptance thresholds (CAL-B2/CAL-B3/CAL-B4).
- Five-seed performance runner and full-session replay (CAL-B1/CAL-B7).

## Test plan

- Run the focused warm CTest and deterministic 4 × 50 regression sequence; verify a deliberately failing-quality fixture causes the named hard-limit assertion to fail only if the test setup itself is a distinct, controlled scenario.
- Run configured full CTest and `.venv/bin/python -m pytest -q` at the ticket boundary.
- Run the five-seed verification and dense-grid safety scan after the new suite passes.

## Blocked by

- CAL-B4 — staged fallback and degraded result semantics.

## Expected seam

`tests/test_warm_calibrator.cpp` and a focused scenario-sequence test seam; only small benchmark adjustments in `bench/essvi_bench.cpp` if required to expose the accepted metric API.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-007, DEC-011, DEC-012, DEC-013

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-16 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `186d03f63dcf0d495052742a81c5648fdfbe36ec` | passed | Release build succeeded; CTest 5/5 passed; pytest 10/10 passed. |
| 2026-10-16 | Red baseline sequence | `build/essvi_bench --scenario all --scenario-updates 50 --seed 1 --check` | `/home/zhangy/git/ql-essvi` | `186d03f63dcf0d495052742a81c5648fdfbe36ec` | expected nonzero; hard misses observed | All four 50-update scenarios completed; the checker returned 2 on target/hard-limit misses. This established the existing quality gaps before adding the regression runner. |
| 2026-10-16 | Focused degraded checks | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_warm_calibrator_tests essvi_warm_scenario_regression -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | pending | passed | Warm tests assert `path=degraded`, valid dense-grid surfaces, exact nonzero-age/degraded-count agreement, and `full_cold_searches <= 8 + forced_full_cold_searches`; focused CTest 1/1 passed. |
| 2026-10-16 | 4 × 50 hard-limit regression | `build/essvi_warm_scenario_regression` | `/home/zhangy/git/ql-essvi` | pending | completed; hard misses reported (exit 1) | Four scenarios × 50 updates, 40 × 100 quotes, seed 1. Each first violation reports scenario, seed, update, metric, observed value, and hard limit. All four scenarios currently breach one or more Addendum B limits; details and measured misses are in `docs/perf-results.md` WP-B5. No warm invalid surface or dense-grid violation was reported. |
| 2026-10-16 | Configured verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | pending | passed | Release build succeeded; CTest 5/5 passed; pytest 10/10 passed. The opt-in hard-limit runner is not part of default CTest. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |

## Delivery record

- Base commit: `186d03f63dcf0d495052742a81c5648fdfbe36ec`
- Baseline status: passed
- Baseline verification: configured CMake build, CTest 5/5, pytest 10/10 passed at base commit `186d03f63dcf0d495052742a81c5648fdfbe36ec`.
- Baseline failure evidence: None
- Unconfirmed commands: typecheck/lint not applicable; no configured linter
- Branch: `delivery/calibrator-speedup-addendum-b/CAL-B5`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: pending
- GitHub Issue: none (local tracker)
- Tests: focused warm CTest 1/1, configured CTest 5/5, pytest 10/10 passed. The opt-in 4 × 50 hard-limit runner completed and returned 1 because measured Addendum B hard limits are still exceeded; see `docs/perf-results.md` WP-B5.
- Diff review: waived at user request; no independent review performed.
