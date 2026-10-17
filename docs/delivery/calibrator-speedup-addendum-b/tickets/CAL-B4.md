# CAL-B4 — Bounded staged fallback and degraded results

## State

- Current: implemented
- Last transition: `implementing` → `implemented`
- At: 2026-10-16
- Reason/evidence: implementation and configured verification completed in commit `f0682e0c3012be976e327273ef12b4488217e33d`; user waived further independent reviews.
- Blocker: None

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Implement the local → restricted cold → full cold stages with a per-update budget of at most eight full cold searches by default. On budget exhaustion, return the best feasible validated surface as `path=degraded`, report achieved quality and degraded-slice ages/counts, and refit pending slices oldest-first. Force a full cold refit for a slice after three consecutive degraded returned updates, regardless of the ordinary budget.

## Acceptance criteria

- A candidate failing the local gate receives the restricted cold search around prior rho ±0.15 and within the feasible psi interval; full cold runs only when the restricted stage still fails the approved gate.
- At most eight full cold searches run per update, except a forced refit for a slice whose degraded age exceeded the approved limit; no optional wall-clock deadline is enabled by default.
- Budget exhaustion returns the best feasible surface found with `path=degraded`, `degraded_slices`, achieved loss/quality, worst-slice max/RMS ratio, and per-slice age; the full result passes `validate()` and dense-grid checks.
- Pending slices are refit oldest-first. A slice degraded for three consecutive outputs is forced through full cold on the following update, even beyond the ordinary budget; age resets only when the slice passes the gate.
- If no feasible validated surface can be returned, report failure with a non-empty error; never label an invalid surface successful.
- A valid result following the bounded policy has `converged=true` and `path=degraded`; the shared CSV headers remain unchanged.

## Out of scope

- Replacing the gate/reuse test (CAL-B2) or local solver core (CAL-B3).
- The broader four-scenario × 50-update regression sequence (CAL-B5).

## Test plan

- Add deterministic tests for stage order, full-cold budget boundaries, valid degraded return, exact counts/ages, oldest-first refit, forced full cold after three degraded outputs, and infeasible/no-result failure.
- Check each returned surface through `validate()` and the existing dense-grid butterfly/calendar scan.
- Run focused warm CTest and the configured full CTest/Python suites.

## Blocked by

- CAL-B3 — bounded deterministic local solver and reason observability.

## Expected seam

`cpp/calibrator.hpp`, `cpp/calibrator.cpp`, `tests/test_warm_calibrator.cpp`, benchmark aggregation in `bench/essvi_bench.cpp`, and `docs/perf-results.md`.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-005, DEC-007, DEC-013

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-16 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `74d3a0e4eb65c021c3bd8b00685b6c51a963830a` | passed | Release build succeeded; CTest 5/5 passed and pytest 10/10 passed. |
| 2026-10-16 | Red | `cmake --build build --target essvi_warm_calibrator_tests -j2` | `/home/zhangy/git/ql-essvi` | `74d3a0e4eb65c021c3bd8b00685b6c51a963830a` | failed as expected | New regression coverage could not compile before implementation: `CalibrationPath::degraded` and budget/quality result diagnostics were not yet present. |
| 2026-10-16 | Green focused | `cmake --build build --target essvi_warm_calibrator_tests essvi_bench -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | `f0682e0c3012be976e327273ef12b4488217e33d` | passed | Focused warm-calibrator coverage passed, including budget exhaustion, degraded validity, forced full-cold recovery, shortcut, reset, and no-result failure. |
| 2026-10-16 | Scenario smoke | `build/essvi_bench --scenario fast --scenario-updates 5 --expiries 10 --quotes-per-expiry 100 --seed 11 --timings-out /tmp/cal-b4-smoke.json` | `/home/zhangy/git/ql-essvi` | `f0682e0c3012be976e327273ef12b4488217e33d` | passed; acceptance not claimed | Five outputs converged; 2 fallback + 3 degraded; 30 restricted, 44 full-cold, 20 forced searches; six budget denials; zero failures, invalid surfaces, or dense-grid violations. JSON summary parsed. Small smoke only, not the 40 × 100 / 200-update acceptance scenario. |
| 2026-10-16 | Legacy JSON smoke | `build/essvi_bench --expiries 2 --quotes-per-expiry 100 --repeats 1 --warm-updates 2 --timings-out /tmp/cal-b4-legacy.json` | `/home/zhangy/git/ql-essvi` | `f0682e0c3012be976e327273ef12b4488217e33d` | passed | Legacy warm JSON parsed; `degraded_ages` aligned with two slices as `[0, 0]`; quality diagnostics present and zero for this unchanged-slice sample. |
| 2026-10-16 | Configured verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `f0682e0c3012be976e327273ef12b4488217e33d` | passed | Release build succeeded; CTest 5/5 passed and pytest 10/10 passed. |
| 2026-10-16 | Supplemental 739-profile verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree (uncommitted) | passed; acceptance misses remain | The high-fallback shortcut threshold is calibrated to `0.10340351909729481` from the 739-snapshot pooled fast-tilt p95. Configured build passed, CTest 5/5 passed, and pytest 10/10 passed; current scenario acceptance misses are recorded in `docs/perf-results.md`. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |

## Delivery record

- Base commit: `74d3a0e4eb65c021c3bd8b00685b6c51a963830a`
- Baseline status: passed
- Baseline verification:
  - `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` (cwd: `/home/zhangy/git/ql-essvi`) — passed (CTest 5/5; pytest 10/10)
- Baseline failure evidence: None
- Unconfirmed commands: typecheck/lint not applicable; no configured linter
- Branch: `delivery/calibrator-speedup-addendum-b/CAL-B4`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: `f0682e0c3012be976e327273ef12b4488217e33d`
- GitHub Issue: none (local tracker)
- Tests: focused warm-calibrator CTest passed; configured CTest 5/5 and pytest 10/10 passed; fast 10 × 100 / 5-update and legacy JSON smokes passed. The smoke is not Addendum B performance acceptance.
- Diff review: waived at user request; no independent review performed.
