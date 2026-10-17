# CAL-B6 — Cold-path regression A/B

## State

- Current: implemented
- Last transition: `ready` → `baseline-passed` → `implementing` → `implemented`
- At: 2026-10-16
- Reason/evidence: cold-path A/B evidence recorded in `docs/perf-results.md`; reference source provenance is unavailable and sessions disagree, so the approved outcome is inconclusive and no cold-path code change is justified.
- Blocker: None

## Claim

None

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Compare the current cold path against `build/wp5-parallel` on the same machine, with matched seed, benchmark arguments, compiler/flags, and reported load averages. Verify the comparison binary's provenance first. If an actual regression beyond measurement noise is shown, restore cold latency to the approved single-slice and full-chain thresholds without violating the parent output tolerance.

## Acceptance criteria

- Record compiler, effective flags, thread cap, seeds, benchmark dimensions, repeats, and load averages for both binaries; mark the A/B inconclusive if provenance or flags cannot be established.
- Run 40 expiries × 100 quotes for nine repeats and one expiry × 100 quotes for 20 repeats.
- If the current cold path regresses beyond measurement noise, identify and fix the cause; single-slice median is ≤2.5 ms and full-chain median ≤55 ms, with output within the parent spec tolerance.
- If no regression is found, make no unrelated cold-path edits and record measurements and uncertainty.
- Preserve deterministic behavior, the four-thread maximum, and all existing correctness checks.

## Out of scope

- Warm gate/solver tuning except a minimal change necessary to restore the measured cold path.
- Unverified comparison claims based solely on an old binary with unknown build settings.

## Test plan

- Verify `build/wp5-parallel` provenance and flags, then run the paired A/B sizes and repeats on the same machine/seed with before/after load averages.
- Run cold regression and dense-grid correctness tests if code changes are necessary.
- Run configured full CTest and `.venv/bin/python -m pytest -q` at the ticket boundary.

## Blocked by

- CAL-B5 — final Addendum B regression coverage.

## Expected seam

Cold calibration path and benchmark reporting, likely `cpp/calibrator.cpp`, `bench/essvi_bench.cpp`, and `docs/perf-results.md`. Do not edit warm behavior unless required by the measured cold-path fix.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-005, DEC-006, DEC-007

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-16 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `c3e39c0e1bb4a1d9a4234e93ca22da1d6a932ecd` | passed | Release build succeeded; CTest 5/5 passed and pytest 10/10 passed. |
| 2026-10-16 | Provenance | `file build/wp5-parallel/essvi_bench; stat build/wp5-parallel/essvi_bench build/wp5-parallel/CMakeFiles/essvi_core.dir/cpp/calibrator.cpp.o; compare CMakeCache.txt and compile_commands.json` | `/home/zhangy/git/ql-essvi` | `c3e39c0e1bb4a1d9a4234e93ca22da1d6a932ecd` | partial; comparison inconclusive | GCC 15.2.0, Release/effective `-O2`, thread cap 4, native/profile options off match. The reference build has no recorded source SHA; its object is dated 2026-10-15 before current source modifications, and the historical tree was uncommitted. |
| 2026-10-16 | A/B session 1 | Reference/current `essvi_bench`; 40 × 100, 9 repeats, then 1 × 100, 20 repeats; shared seed-1 CSV fixtures | `/home/zhangy/git/ql-essvi` | `c3e39c0e1bb4a1d9a4234e93ca22da1d6a932ecd` | measured; insufficient to classify | Repeated full-chain medians 54.567 / 62.999 ms (reference/current); single-slice 2.350 / 2.656 ms. Reference-side load was not captured. |
| 2026-10-16 | A/B session 2 | Same binaries, CSVs, dimensions, repeats; external `/proc/loadavg` before/after each invocation | `/home/zhangy/git/ql-essvi` | `c3e39c0e1bb4a1d9a4234e93ca22da1d6a932ecd` | inconclusive | Full-chain medians 111.955 / 97.672 ms; single-slice 2.506 / 3.651 ms (reference/current). Timing direction reverses for full-chain; session 2 reference itself misses the 55 ms and 2.5 ms medians. Parameter max relative difference was zero on both sizes. No code change justified. |
| 2026-10-16 | Configured verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `c3e39c0e1bb4a1d9a4234e93ca22da1d6a932ecd` | passed | No product code changed; CTest 5/5 and pytest 10/10 passed. |
| 2026-10-16 | Documentation commit | Record verified fixture equivalence, provenance, and paired A/B outcome in `docs/perf-results.md` | `/home/zhangy/git/ql-essvi` | `0e1f41739833d7b012028a760ed436bace7eef67` | recorded; inconclusive | The reference source revision is unavailable and the sessions disagree; no cold-path code change was justified. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |

## Delivery record

- Base commit: `c3e39c0e1bb4a1d9a4234e93ca22da1d6a932ecd`
- Baseline status: passed
- Baseline verification: configured CMake build, CTest 5/5, pytest 10/10 passed at base commit `c3e39c0e1bb4a1d9a4234e93ca22da1d6a932ecd`.
- Baseline failure evidence: None
- Unconfirmed commands: typecheck/lint not applicable; no configured linter
- Branch: `delivery/calibrator-speedup-addendum-b/CAL-B6`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: `0e1f41739833d7b012028a760ed436bace7eef67`
- GitHub Issue: none (local tracker)
- Tests: configured CTest 5/5 and pytest 10/10 passed. Paired A/B measured both sizes/repeat counts on shared seed-1 inputs; timing result is inconclusive because reference source revision is unavailable and repeated sessions disagree.
- Diff review: waived at user request; no independent review performed.
