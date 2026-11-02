# EBA-4 — Posterior covariance refresh and cold-fallback correction

## State

- Current: finished
- Last transition: review-approved -> finished
- At: 2026-11-01T01:15:54.274Z
- Reason/evidence: Round 2 review approved with one nonblocking P3 UNPROVEN Jacobian-reference suggestion. EBA-5 finished its held-out replay and review dependency. EBA-4 base, HEAD, snapshot, spec, final verification, and approval still match; finish readiness passed.
- Blocker: None. EBA-5 finished its held-out replay verification and round-six review; the external finish dependency is cleared.

## State history

| At | Transition | Evidence |
|---|---|---|
| 2026-10-31T20:46:50.193Z | draft -> ready | Approved parent spec; EBA-3 finished at base `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6`; declared base, branch, and test plan. |
| 2026-10-31T20:47:09.432Z | ready -> baseline-checking | EBA-4 branch `delivery/essvi-bayesian-update/EBA-4` and primary worktree are established; baseline base is `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6`. |
| 2026-10-31T20:49:12.947Z | baseline-checking -> baseline-passed | Clean baseline at `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6`: configured full suite passed CTest 6/6 and pytest 81/81; focused warm CTest passed 1/1. |
| 2026-10-31T20:49:19.719Z | baseline-passed -> implementing | Baseline timeline rows passed; public warm-calibration seam confirmed. |
| 2026-10-31T21:18:56Z | implementing -> implemented | Implementation commit `868fced3b28e34c88267c035f2f41856837e8b1d`; Red/Green/Refactor and exact-commit final verification passed. |
| 2026-10-31T21:19:31Z | implemented -> reviewing | Round 1 input pinned to base `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6`, HEAD `868fced3b28e34c88267c035f2f41856837e8b1d`, snapshot `37d9b792d6a8a19ae2398d230123c1a95c406f7efb7a85d8aec1007e5639aea6`, and spec `5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea`; exact-commit Final and replay evidence above. |
| 2026-10-31T21:22:05Z | reviewing -> changes-requested | Round 1 preflight was blocked by the remaining five-file diff gate; no review axes ran. |
| 2026-10-31T22:15:04Z | user-approved scope split | User instruction: “Keep the gate and split.” Preserve the five-file cap; move held-out replay movement/reporting to EBA-5 and keep EBA-4 unfinished until EBA-5 is verified. |
| 2026-10-31T22:15:04Z | changes-requested -> implementing | Began the user-approved scope correction: remove EBA-5 replay-only changes while retaining the covariance refresh and cold-budget work. |
| 2026-10-31T22:17:50Z | implementing -> implemented | Scope-correction commit 7a3e97c293b7415d19dea7db336e2e9fa6d11d3f; exact Release verification passed CTest 6/6 and pytest 81/81. |
| 2026-10-31T23:20:30Z | implemented -> reviewing | Fresh exact-commit verification passed; froze round-two patch 346b8bf414c12fa537dc8cdc9e839bee0ff3d95b59d67566f5153906086c5d90 and spec 5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea; readiness and diff/spec preflight passed at 2026-10-31T23:21:05Z. |
| 2026-10-31T23:28:21Z | reviewing -> review-approved | Round 2: spec and standards found no issues; verification passed CTest 6/6 and pytest 81/81. Rejected the Release-assertion finding based on `#undef NDEBUG` before `<cassert>`; retained one nonblocking P3 UNPROVEN Jacobian-reference suggestion. EBA-5 remains an external finish blocker. |
| 2026-11-01T01:15:54.274Z | review-approved -> finished | The exact approved EBA-4 input and verification remain unchanged; EBA-5 blocker cleared; finish readiness passed with snapshot/spec identity checked; no new learning captured. |

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); [EBA-3](EBA-3.md)

## What to build

Refresh each returned covariance using current-snapshot active out-of-band quote information and the predicted prior covariance, including cold initialization and fallback outputs. Ensure exact-expiry slices with unusable covariance take the required cold fit even after the ordinary full-cold search budget is consumed. Qualify the documentation: warm accepted candidates receive local posterior-mode refinement, while restricted/cold fallback candidates are not guaranteed to be posterior modes.

## Acceptance criteria

- Current quote information updates covariance in transformed coordinates using the documented positive-semidefinite Gauss-Newton approximation and variance floor; process noise is applied once per warm update.
- Every successful warm, cold, and fallback result returns finite symmetric positive-definite covariance aligned with every surface slice; missing, malformed, or invalid exact-expiry covariance cold-starts safely.
- With more than eight matched slices carrying unusable covariance, later required cold starts do not fail solely because the ordinary full-cold search budget was consumed.
- Successful returned surfaces still pass `validate()` and dense-grid butterfly/calendar checks, and existing quote-quality/fallback gates remain unchanged.
- README and parameterization docs describe generalized-Bayes evidence, covariance approximation/process-noise cadence, and state explicitly that restricted/cold fallbacks are not guaranteed posterior modes.
- Held-out replay reporting (one-step-ahead band loss, movement, feasibility, fallback rate, and latency) is re-homed to EBA-5. EBA-4 must not finish until EBA-5 verifies those metrics against the existing warm and cold paths without unsupported predictive or performance claims.

## Out of scope

- New likelihoods, new timestamp/expiry matching, changes to quality/fallback policy, or changes to CSV/API schemas.
- Retuning on the held-out seed or treating the synthetic replay as market evidence.

## Test plan

- Add a regression with more than eight exact-expiry slices and invalid covariance; require successful valid cold results and aligned SPD covariance after the fallback budget is exceeded.
- Verify covariance refresh and its cold/fallback behavior through the public C++ warm API.
- Run the configured full command for EBA-4. EBA-5 owns and verifies the declared held-out replay command; do not finish EBA-4 until that ticket passes its replay verification and review.

## Blocked by

[EBA-3](EBA-3.md) (completed); [EBA-5](EBA-5.md) (finished; held-out replay verification and review completed before EBA-4 handoff)

## Expected seam

`calibrateWarm`, `CalibrationResult::posterior_covariances`, fallback-budget diagnostics, and the documented generalized-Bayes result semantics.

## Evidence and decision references

- Decisions: DEC-001, DEC-002

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|
| 2026-10-31T20:49:12.947Z | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | /tmp/ql-essvi-eba4-baseline | `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6` | passed | Clean base checkout; Release build, CTest 6/6, pytest 81/81. |
| 2026-10-31T20:49:12.947Z | Baseline focused | `ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | /tmp/ql-essvi-eba4-baseline | `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6` | passed | essvi_warm_calibrator_tests passed 1/1. |
| 2026-10-31T21:06:01Z | Red — active quote covariance | `export PATH="<repo-venv>/bin:$PATH"; cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | /tmp/ql-essvi-eba4-red | `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6` + test-only overlay | expected failure (exit 8) | On exact base, the new active-current-band covariance assertion failed: returned covariance did not contract from the predicted prior. |
| 2026-10-31T21:06:01Z | Red — invalid covariance cold budget | Same focused command, with the isolated >8-slice regression first | /tmp/ql-essvi-eba4-red | `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6` + test-only overlay | expected failure (exit 8) | On exact base, `unusableMatchedCovarianceForcesColdBeyondOrdinaryBudget` failed at `result.converged`; the required cold starts exhausted the 8-search budget. |
| 2026-10-31T21:06:01Z | Green | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | primary checkout | working tree before commit | passed | Final Release build; CTest 6/6 and pytest 83/83, including covariance refresh, floor, fallback, and >8 invalid-covariance cases. |
| 2026-10-31T21:06:01Z | Refactor | `cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | primary checkout | working tree before commit | passed | Reused the fixed-size SPD inverse for prior precision; focused warm CTest passed 1/1 after the refactor. |
| 2026-10-31T21:18:56Z | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | /home/zhangy/git/ql-essvi | `868fced3b28e34c88267c035f2f41856837e8b1d` | passed | Exact-commit Release build; CTest 6/6 and pytest 84/84, including covariance refresh/floor, process-noise cadence, >8 invalid-covariance cold starts, replay aggregation and degraded-path fallback-rate coverage. |
| 2026-10-31T21:18:56Z | Held-out replay | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_bench -j2 && .venv/bin/python python/run_snapshot_replay.py --data-root "$HOME/vol-rv/data" --output-dir build/eba4-heldout-replay --bench build/essvi_bench` | /home/zhangy/git/ql-essvi | `868fced3b28e34c88267c035f2f41856837e8b1d` | passed | 31 asset/day rows; 2,293 snapshots, 1,124 usable (IBIT only), 1,099 paired updates and movement samples. JSON/CSV record one-step band loss, warm/cold p50/p99 latency, movement, feasibility and fallback rate. Warm/cold band-loss sums 1.7150/1.7517; movement RMS 0.001858/0.001954. Invalid surfaces and dense-grid violations: 0 warm, 0 cold; warm/cold failures: 2/0. Fallback plus degraded: 1,101/1,101 warm samples (100%). SPY had no usable quotes; American IVs use the documented Black--Scholes European stand-in; no predictive/performance claim. |
| 2026-10-31T21:19:31Z | Review round 1 input | Base `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6` to HEAD `868fced3b28e34c88267c035f2f41856837e8b1d`; snapshot SHA-256 `37d9b792d6a8a19ae2398d230123c1a95c406f7efb7a85d8aec1007e5639aea6`; spec SHA-256 `5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea`; verification pointer: Final and Held-out replay above. | /home/zhangy/git/ql-essvi | `868fced3b28e34c88267c035f2f41856837e8b1d` | review requested | Frozen `/tmp/EBA-4-review-round1.patch`; exact base-to-HEAD committed implementation diff; zero untracked files. User-owned EBA-2/EBA-3 ticket edits and EBA-4 administrative tracking metadata are excluded from this implementation snapshot. |
| 2026-10-31T21:22:05Z | Review round 1 readiness preflight | `.venv/bin/python /home/zhangy/.omp/plugins/node_modules/@zhangys2/research-to-delivery/skills/finish-delivery-ticket/scripts/verify_ticket_ready.py --ticket docs/delivery/essvi-bayesian-update/tickets/EBA-4.md --stage review --diff /tmp/EBA-4-review-round1.patch` | /home/zhangy/git/ql-essvi | `868fced3b28e34c88267c035f2f41856837e8b1d` | failed (exit 1) | `verify_ticket_ready.py` returned FAIL because the frozen diff has six non-test/non-fixture files against the five-file budget. No review axis ran. |
| 2026-10-31T21:22:05Z | Review round 1 diff preflight | `.venv/bin/python /home/zhangy/.omp/plugins/node_modules/@zhangys2/research-to-delivery/skills/review-delivery-diff/scripts/check_diff_spec.py --diff /tmp/EBA-4-review-round1.patch --spec docs/delivery/essvi-bayesian-update/tickets/EBA-4.md` | /home/zhangy/git/ql-essvi | `868fced3b28e34c88267c035f2f41856837e8b1d` | failed (exit 1) | `check_diff_spec.py` counted README.md, bench/essvi_bench.cpp, cpp/calibrator.cpp, docs/delivery/DELIVERY.md, docs/parameterization.md, and python/run_snapshot_replay.py; no reviewer axis ran. Snapshot identity unchanged. |
| 2026-10-31T22:15:04Z | Review round 1 input invalidated | User-approved scope split after preflight; replay-only changes are being removed from the EBA-4 range. | /home/zhangy/git/ql-essvi | `868fced3b28e34c88267c035f2f41856837e8b1d` | invalidated | Old snapshot SHA-256 `37d9b792d6a8a19ae2398d230123c1a95c406f7efb7a85d8aec1007e5639aea6` is superseded; no review axis ran. EBA-5 will own the held-out movement and fallback-rate reporting. |
| 2026-10-31T22:17:50Z | Scope correction and Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | /home/zhangy/git/ql-essvi | `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f` | passed | Replay-only reporting was removed from EBA-4 in the approved split correction; exact-commit Release build, CTest 6/6, pytest 81/81. EBA-5 replay verification remains a blocker. |
| 2026-10-31T23:20:30Z | Final after scope correction | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | /home/zhangy/git/ql-essvi | `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f` | passed | Fresh exact-commit Release configure/build; CTest 6/6; pytest 81/81. |
| 2026-10-31T23:20:30Z | Review round 2 input | Base `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6` to HEAD `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f`; frozen patch /tmp/EBA-4-review-round2.patch; SHA-256 `346b8bf414c12fa537dc8cdc9e839bee0ff3d95b59d67566f5153906086c5d90`; spec SHA-256 `5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea`; verification pointer: fresh Final row above. |
| 2026-10-31T23:21:05Z | Review round 2 readiness and diff/spec preflight | verify_ticket_ready.py --stage review and check_diff_spec.py | /home/zhangy/git/ql-essvi | `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f` | passed | Ticket readiness PASS; diff/spec PASS; frozen patch SHA-256 `346b8bf414c12fa537dc8cdc9e839bee0ff3d95b59d67566f5153906086c5d90`; three implementation files (limit five), one test file. |
| 2026-10-31T23:28:21Z | Review round 2 | Three independent axes; Verification reran the DELIVERY.md full-suite command | /home/zhangy/git/ql-essvi | base `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6` to HEAD `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f` | approved with suggestions | Frozen patch SHA-256 `346b8bf414c12fa537dc8cdc9e839bee0ff3d95b59d67566f5153906086c5d90`; spec SHA-256 `5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea`. Spec: none. Standards: none. Verification full suite exit 0, CTest 6/6, pytest 81/81. Rejected the `-DNDEBUG` assertion finding: `tests/test_warm_calibrator.cpp:5-8` undefines NDEBUG before `<cassert>`; generated flags show `-DNDEBUG` at `build/CMakeFiles/essvi_warm_calibrator_tests.dir/flags.make:9`. Nonblocking P3: independently finite-difference the covariance Jacobian. EBA-5 finish dependency remains unresolved. |

| 2026-11-01T01:15:01.605Z | EBA-5 finish dependency cleared | EBA-5 round-six review: base 7a3e97c293b7415d19dea7db336e2e9fa6d11d3f to HEAD e2d9f7a8b3821e8ba06528c372bc911aafef9461; patch SHA-256 5194231eb0b60f6f125eb2d3071b893e806c5d90f22c16fa6795aca521ab8e23. Exact Release suite CTest 6/6, pytest 92/92; held-out replay passed. | /home/zhangy/git/ql-essvi | `e2d9f7a8b3821e8ba06528c372bc911aafef9461` | completed | EBA-5 is finished and round six approved. Its final ticket record is retained in local stash `883eae843287d8203dd37a05ad6002aa7f76e79e`; earlier user stashes were not dropped. |

| 2026-11-01T01:15:54.274Z | Finish readiness | `python /home/zhangy/.omp/plugins/node_modules/@zhangys2/research-to-delivery/skills/finish-delivery-ticket/scripts/verify_ticket_ready.py --ticket docs/delivery/essvi-bayesian-update/tickets/EBA-4.md --stage finish --diff /tmp/EBA-4-review-round2.patch` | /home/zhangy/git/ql-essvi | `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f` | passed | State review-approved; timeline checkpoints, dispositions, parent spec, and exact frozen diff checked. EBA-5 dependency is finished. |
| 2026-11-02T00:44:42Z | PR review correction verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | /home/zhangy/git/ql-essvi | `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f` + review-correction working tree | passed | Release build; CTest 6/6; pytest 81/81. Includes large-prior retention/cold twin and independent finite-difference covariance-refresh checks for warm, fallback, and cold results. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|
| 1 (preflight) | Blocking — six non-test files exceed the five-file budget | accepted | User instruction: “Keep the gate and split.” Replay reporting is re-homed to EBA-5; EBA-4 remains unfinished until EBA-5 verification. |
| 2 (spec) | None | accepted | Independent spec review found all narrowed EBA-4 acceptance criteria met; EBA-5 replay reporting remains an external dependency. |
| 2 (standards) | None | accepted | Independent standards/code review found no correctness, maintainability, weakened-check, or file-budget issues. |
| 2 (verification) | P2 — Release -DNDEBUG disables cold-budget assertions | rejected | `tests/test_warm_calibrator.cpp:5-8` executes `#undef NDEBUG` before `<cassert>`, while `build/CMakeFiles/essvi_warm_calibrator_tests.dir/flags.make:9` confirms the Release flag. The test assertions remain active; the on-rails full suite passed CTest 6/6 and pytest 81/81. |
| 2 (verification) | P3 UNPROVEN — no independent finite-difference reference for covariance Jacobian | accepted | Nonblocking rung-3 suggestion under the review rubric: the public test exercises active quote information and covariance refresh, but an independently computed Jacobian/covariance value would raise this fact to rung 4. No acceptance criterion requires that additional reference. |
| PR review | Medium — large-prior regression passed when prior was discarded or cold-started | accepted | `covarianceStateIsAlignedAndInvalidStateColdStarts` now asserts the in-band large prior remains above `1e100`, checks an invalid-covariance twin against cold initialization, and independently checks the out-of-band refreshed covariance. |
| PR review | Low — covariance shrink checks accepted one-ulp movement in any diagonal | accepted | Replaced all three OR assertions with full packed covariance comparisons against an independent finite-difference/information-matrix calculation for warm, fallback, and cold results. |
| PR review | Low — EBA-4 replay acceptance text still claimed replay as its own deliverable | accepted | EBA-4 acceptance/test plan assigns held-out replay reporting to EBA-5; this branch includes the finished EBA-5 ticket with verification, replay results, and review record. |

## Delivery record

- Base commit: `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6`
- Baseline status: passed
- Baseline commit: `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6`
- Baseline verification:
  - `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` (cwd: `/tmp/ql-essvi-eba4-baseline`) — passed
  - `ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` (cwd: `/tmp/ql-essvi-eba4-baseline`) — passed
- Baseline failure evidence: None
- Unconfirmed commands: None (typecheck/lint is not applicable)
- Branch: `delivery/essvi-bayesian-update/EBA-4`
- Worktree: primary checkout
- Implementation commit: `868fced3b28e34c88267c035f2f41856837e8b1d`
- Scope correction commit: `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f` (replay reporting returned to base for EBA-5).
- Final verification after scope correction: exact-commit `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f`; configured Release build, CTest 6/6, pytest 81/81. The historical replay on `868fced3b28e34c88267c035f2f41856837e8b1d` is outside the narrowed EBA-4 scope; EBA-5 must provide fresh replay evidence.
- Latest verification: exact commit 7a3e97c293b7415d19dea7db336e2e9fa6d11d3f; fresh configured Release build, CTest 6/6, pytest 81/81.
- EBA-5 dependency: finished at code commit `e2d9f7a8b3821e8ba06528c372bc911aafef9461`; round-six review approved on snapshot SHA-256 `5194231eb0b60f6f125eb2d3071b893e806c5d90f22c16fa6795aca521ab8e23`. Exact Release verification passed CTest 6/6 and pytest 92/92; held-out replay passed with 31 asset/day rows, 2,293 snapshots, 1,124 usable, path movement totals 1,099/1,101, fallback+degraded 1,101/1,101, and zero invalid surfaces/dense-grid violations. EBA-5 final ticket record is preserved in local stash `883eae843287d8203dd37a05ad6002aa7f76e79e`; pre-existing stash entries were preserved.
- Review result: round 2 approved with suggestions for the frozen base 3a381aefbd04b84c8ecf6468d8b20f9222f51dd6 to HEAD 7a3e97c293b7415d19dea7db336e2e9fa6d11d3f; patch SHA-256 346b8bf414c12fa537dc8cdc9e839bee0ff3d95b59d67566f5153906086c5d90.
- Nonblocking suggestion: add an independent finite-difference reference for the transformed-coordinate covariance Jacobian.
- EBA-5 dependency is resolved; EBA-4’s approved base, HEAD, review snapshot, and final verification remain unchanged.
- Review outcome retained: round 2 approved with suggestions for exact base `3a381aefbd04b84c8ecf6468d8b20f9222f51dd6` to HEAD `7a3e97c293b7415d19dea7db336e2e9fa6d11d3f`; patch SHA-256 `346b8bf414c12fa537dc8cdc9e839bee0ff3d95b59d67566f5153906086c5d90`; one nonblocking P3 UNPROVEN Jacobian-reference suggestion remains.
- Finish readiness: passed with exact frozen diff and parent spec checked; no EBA-4 code or verification input changed after approval.
- Learning capture: none; no additional reusable repository-specific constraint met the capture bar.
- Handoff: EBA-4 finished on its approved local branch; PR #15 carries the implementation. The PR follow-up adds review-correction tests and the finished EBA-5 ticket/evidence; latest working-tree verification is recorded in the timeline above.
- GitHub Issue: none (local tracker)
