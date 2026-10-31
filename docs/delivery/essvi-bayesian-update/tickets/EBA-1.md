# EBA-1 — Constrained Bayesian eSSVI warm update

## State

- Current: `changes-requested`
- Last transition: `reviewing` → `changes-requested`
- At: 2026-10-31T13:00:22+00:00
- Reason/evidence: Round 1 independent review found a blocking diff-budget violation and additional cold-start/documentation corrections; full review findings are recorded below.
- Blocker: Delivery must be decomposed to meet the review size budget; a cold-start budget edge and fallback/MAP documentation need correction.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | 2026-10-31T11:29:15Z | User approved `implementation-spec.md`; ticket is scoped to DEC-001 and DEC-002. |
| `ready` | `baseline-checking` | 2026-10-31T11:31:33+00:00 | Ticket claimed; configured primary-checkout branch created from base commit `497f0c37d27056ec2e6df631c0f570d80fe332f4`. |
| `baseline-checking` | `baseline-passed` | 2026-10-31T11:32:55+00:00 | Exact-base Release build passed; CTest 6/6 and pytest 80/80 passed. |
| `baseline-passed` | `implementing` | 2026-10-31T11:33:38+00:00 | Baseline passed; public C++ warm-result seam confirmed. |
| `implementing` | `implemented` | 2026-10-31T12:47:57+00:00 | TDD checkpoints and final verification passed; implementation commit `12581d21a1aefa4e17a2fff84bf4eca69e9ea895`. |
| `implemented` | `reviewing` | 2026-10-31T12:50:28+00:00 | Round 1 requested for base `497f0c37d27056ec2e6df631c0f570d80fe332f4` to HEAD `10b75070e402ce207f745e7f6a9dc6047ac10295`; preflight size finding recorded; input manifest linked in timeline. |
| `reviewing` | `changes-requested` | 2026-10-31T13:00:22+00:00 | Round 1: independent spec/standards/verification reviews completed; blocking diff budget plus cold-start correctness and fallback documentation findings recorded; one assertion/NDEBUG finding rejected with source counter-evidence. |

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Implement a warm calibration state transition that combines the previous result’s per-expiry posterior state with the current complete quote snapshot, selects the constrained posterior mode using the existing normalized total-variance bid/ask band loss as generalized-Bayes evidence, and returns covariance state for the next call. Keep butterfly/calendar feasibility hard and preserve existing quality gates, fallback behavior, raw objective reporting, and CSV contracts.

## Acceptance criteria

- A warm update uses the previous result’s mean and covariance as its prior and the current quote snapshot as the sole new quote evidence.
- The MAP depends on the prior under informative current quotes; it is not merely a warm start for the raw band-loss minimum.
- Cold initialization and every successful warm/fallback result have finite symmetric positive-definite covariance aligned one-to-one with surface slices.
- Every successful returned surface passes `validate()` and the existing dense-grid butterfly/calendar checks. Existing quote-quality gates and fallback contracts remain active.
- Missing/unusable priors and unmatched expiries cold-start without returning an invalid posterior state.
- Python can inspect covariance snapshots and pass a result into a subsequent warm call without losing native posterior state.
- The existing raw `objective`, diagnostics, and CSV contracts remain unchanged; README and parameterization documentation explain the generalized-Bayes interpretation and limits.
- A held-out snapshot replay reports one-step-ahead band loss, surface movement, feasibility, fallback rate, and latency against existing warm and cold paths.

## Out of scope

- A generative interval-censored quote-noise model or mid/band-center fitting.
- Changes to eSSVI parameterization, no-arbitrage conditions, CSV schemas, quote preprocessing, CVI, or QuantLib.
- New calendar-expiry matching or snapshot timestamp fields.
- Pushing, merging, or opening a GitHub issue.

## Test plan

- Add C++ public-seam tests in `tests/test_warm_calibrator.cpp` for prior influence, covariance initialization/propagation, current-snapshot updates, exact-expiry cold starts, feasibility, and unchanged fallback behavior.
- Add Python result-state coverage in `tests/test_python_bindings.py` for read-only covariance snapshots and consecutive-result reuse.
- Run focused CTest `essvi_warm_calibrator_tests` and Python binding tests after the build.
- Run the configured full build/CTest/pytest command from `docs/delivery/DELIVERY.md`.
- Run a bounded warm-scenario or real snapshot replay with held-out next-snapshot scoring; record quality and feasibility without weakening existing thresholds.

## Blocked by

None

## Expected seam

`essvi::calibrateWarm` and its returned `CalibrationResult`, exercised from C++ tests and the Python binding. This catches posterior state transitions, feasibility, and fallback behavior; it does not validate a market-noise interpretation, which this ticket does not claim.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-31T11:32:55+00:00 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `497f0c37d27056ec2e6df631c0f570d80fe332f4` | passed | Release build succeeded; CTest 6/6 passed; pytest 80/80 passed. |
| 2026-10-31 | Red | `cmake --build /tmp/essvi-baseline-check/build --target essvi_warm_calibrator_tests -j2` with current regression source copied into a disposable exact-base worktree | disposable baseline worktree | `497f0c37d27056ec2e6df631c0f570d80fe332f4` | expected failure (exit 2) | Compiler rejected missing CalibrationResult::posterior_covariances; original baseline test source restored. |
| 2026-10-31 | Green | `cmake --build build -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure && .venv/bin/python -m pytest -q tests/test_python_bindings.py` | repository root | working tree | passed | Focused CTest 1/1; Python binding tests 19/19. |
| 2026-10-31 | Refactor | `cmake --build build -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure && .venv/bin/python -m pytest -q tests/test_python_bindings.py` | repository root | working tree | passed | Cleaned posterior score/refinement helpers and state cleanup; focused checks passed. |
| 2026-10-31 | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | repository root | working tree | passed | Release build; CTest 6/6; pytest 80/80. |
| 2026-10-31 | Holdout | `./build/essvi_bench --scenario normal --scenario-updates 10 --expiries 10 --quotes-per-expiry 50 --seed 987 --timings-out /tmp/essvi-bayesian-normal.json` and exact-base binary, five interleaved pairs | repository root | base `497f0c37d27056ec2e6df631c0f570d80fe332f4` vs working tree | measured | Synthetic 10-update replay; score, movement, fallback, feasibility, and latency follow. |
| 2026-10-31T12:50:28+00:00 | Review round 1 input | base `497f0c37d27056ec2e6df631c0f570d80fe332f4` to HEAD `10b75070e402ce207f745e7f6a9dc6047ac10295` | repository root | `local://EBA-1-review-round1.md`, SHA-256 `432140f353271a7df0a550f899e1754f4771f35d4fa1d45c603696a1b96f5b66` | frozen | Spec SHA-256 `5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea`; verification pointer: Final row above. Snapshot is the committed implementation diff; later ticket edits are review-continuity metadata. Preflight: 9 implementation files and 628 non-fixture additions. |
| 2026-10-31T13:00:22+00:00 | Review round 1 | Three isolated axes; review scripts and rails recorded in this ticket | repository root | base `497f0c37d27056ec2e6df631c0f570d80fe332f4` to HEAD `10b75070e402ce207f745e7f6a9dc6047ac10295` | changes required | Spec P2 cold-start budget; standards P1 diff budget and P2 fallback/MAP wording; verification false-positive NDEBUG finding rejected, current-quote-only evidence remains rung 3. Full suite passed CTest 6/6 and pytest 80/80. |

## DEC-002 test expectation adaptations

The MAP update changes the valid output contract from exact previous-slice reuse to a constrained posterior mode (DEC-002). Tests that previously pinned incidental reuse or strict raw-loss ordering now retain quote-cap, no-fallback, slice-accounting, raw-objective consistency, deterministic output, and hard-feasibility checks without requiring the old parameter tuple or a strictly different raw objective. The `slices_refit == 1` assertion in `failedJacobiPrefitFallsBackWithoutRetry` remains exact. No quality threshold, fallback policy, or configured verification command was loosened.

## Measurement and additional scenario diagnostics

The seed-987 normal synthetic run used Release builds, ten updates, ten expiries, 50 quotes per expiry, the same generated snapshots for warm and cold fits, and five interleaved current/base process pairs (50 per-update timing samples per path). The baseline binary was built from `497f0c37d27056ec2e6df631c0f570d80fe332f4`.

- One-step summed raw band loss: current warm `5.32018e-6`, cold `5.59984e-6`, ratio `0.95006`; exact-base warm `4.84554e-6`, cold `5.59984e-6`, ratio `0.86530`. This synthetic holdout does not support a predictive-improvement claim over the baseline warm path.
- Mean per-update RMS total-variance movement over matched expiries and 101 log-moneyness points (`k=-0.5..0.5`): current warm `0.00128595`, current cold `0.00134211`; baseline warm `0.00128561`, baseline cold `0.00131670`.
- Warm/cold latency percentiles from five paired runs, milliseconds (p50/p95/p99; observed sample min/max): current warm `27.64 / 44.95 / 49.38` (20.26–50.95), baseline warm `28.86 / 43.28 / 44.32` (14.93–44.75); current cold `15.34 / 21.92 / 28.16` (11.10–32.10), baseline cold `14.81 / 21.52 / 22.39` (11.79–22.71). Distributions overlap; no latency win is claimed.
- Current and base each used fallback/degraded paths on 10/10 updates. Warm failures, invalid surfaces, and dense-grid violations were zero. Current warm quality: 129/5000 quotes above cap (2.58%), maximum excess ratio 0.775802, RMS excess ratio 0.047595; baseline warm: 127/5000 (2.54%), 0.775802, 0.047444. Cold: 140/5000 (2.80%), 0.773750, 0.048184.
- The unregistered `essvi_warm_scenario_regression` executable failed existing scenario gates on both base and implementation; it is not part of configured CTest. Quiet-path baseline/implementation fallback rates were 14%/32% and median latencies 1.377/15.86 ms; normal fallback was 100% on both, with p99 154/314 ms. Fast/front gates also failed on both. This remains a known risk, not a passing configured check.
- A `perf stat` CPU profile was unavailable (`perf_event_paranoid=4`; no `CAP_PERFMON`); latency cause is unknown. The fixed seed-987 holdout was not used for tuning.

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|
| 1 | Spec P2: invalid covariance can lose forced cold fit after full-cold budget | accepted | `cpp/calibrator.cpp:1518-1524,1544-1547,1736-1739,1919-1929`; decouple forced-cold selection from Bayesian warm-start pointer and add >8-slice regression. |
| 1 | Standards P1: 9 implementation files and 628 non-fixture additions exceed budgets | accepted | Preflight helper and isolated standards review; review rubric requires sequential ticket split. |
| 1 | Standards P2: docs describe every successful result as a MAP although fallback candidates bypass MAP refinement | accepted | `cpp/calibrator.cpp:1953-1969`, `README.md:46`, `docs/parameterization.md:65`; qualify fallback/degraded semantics. |
| 1 | Verification UNTESTED: Release `-DNDEBUG` disables assertions | rejected | `tests/test_warm_calibrator.cpp:5-8` undefines `NDEBUG` before including `<cassert>`; generated flags at `build/CMakeFiles/essvi_warm_calibrator_tests.dir/flags.make:9` confirm the test source resets the macro. |
| 1 | Verification rung-3 gap: previous quote snapshot is not varied independently of current evidence | accepted | Independent verification review evidence ladder; add a focused test holding prior result/current quotes fixed while changing `previous_quotes`. |
| 1 | Verification UNPROVEN: MAP hard-feasibility and cap checks are not active in Release | rejected | Assertions are active; `tests/test_warm_calibrator.cpp:525-581` checks cap for the local posterior path and `checkDenseGrid` validates returned surfaces. |

## Review round 1 report

### Spec review — isolated

**P2 — MISSING: cold-start budget can reject a required cold update.** The approved inference requires cold start for missing/unusable covariance and unmatched expiry (`implementation-spec.md:57,71`). `calibrateWarm` nulls `plan.warm_start` when exact-expiry covariance is unusable (`cpp/calibrator.cpp:1544-1547`), while `fullColdReference` only bypasses the full-cold budget when `plan.warm_start` exists and prior degraded age is at least three (`cpp/calibrator.cpp:1518-1524`). After eight searches, a later degraded slice with invalid covariance can hit the budget-exhausted failure instead of the required cold fit (`cpp/calibrator.cpp:1736-1739,1919-1929`). Correction: decouple forced-cold selection from Bayesian warm-start pointer and add a >8-slice invalid-covariance regression.

### Standards review — isolated

**P1 — Oversized diff.** The frozen snapshot contains 9 implementation files and 628 non-fixture added lines, above both review limits (5 files, 250 lines). Correction: split into sequential test-first tickets under the approved parent spec; preserve the full approved behavior.

**P2 — Fallback/MAP overclaim.** `candidate_from_warm` gates posterior refinement (`cpp/calibrator.cpp:1953-1965`); successful restricted/cold fallback candidates bypass it, but covariance is still returned (`cpp/calibrator.cpp:1967-1969`). `README.md:46` and `docs/parameterization.md:65` describe the returned surface without that qualification. Correction: document that fallback/degraded candidates may be restricted/cold fits and are not guaranteed posterior modes.

No unrelated mixed-responsibility work or suppressions were found. The DEC-002 test expectation changes preserve quote caps, fallback gates, accounting, deterministic output, and feasibility.

### Verification review — isolated

The on-rails full Release suite passed: CTest 6/6 and pytest 80/80. The seed-987 scenario observed 10 updates, warm summed loss `5.3201787768977063e-06` vs cold `5.5998358734795546e-06`; all 10 updates were fallback/degraded (9 fallback, 1 degraded), with zero warm failures, invalid surfaces, or dense-grid violations. It was a single current-process run, not a base comparison; the recorded five-pair base/current measurements were not independently reproduced by the allowed command. No quantitative win is claimed.

The reported assertion-disabled finding is rejected: the test source executes `#undef NDEBUG` before `#include <cassert>` (`tests/test_warm_calibrator.cpp:5-8`), while CMake Release flags contain `-DNDEBUG` (`build/CMakeFiles/essvi_warm_calibrator_tests.dir/flags.make:9`). Thus the new C++ assertions remain active. The prior-result covariance affects the constrained mode through the active prior-influence test; the separate claim that `previous_quotes` does not add likelihood evidence has only code-trace evidence (rung 3), because no test varies that snapshot independently. Hard feasibility and cap checks are exercised by the active cap test and `checkDenseGrid`; Python covariance reuse is covered by the passing consecutive-result binding test.

### Gate candidates

- Mechanical: make the deterministic `diff_budget` preflight a required blocking delivery gate for more than 250 non-fixture added/modified lines or more than 5 implementation files. The frozen input fails it.

**Round 1 outcome: changes required.** Findings remain unresolved; no approval is granted.

## Delivery record

- Base commit: `497f0c37d27056ec2e6df631c0f570d80fe332f4`
- Baseline status: passed
- Baseline verification: configured Release build, CTest 6/6, pytest 80/80 at the declared base
- Branch: `delivery/essvi-bayesian-update/EBA-1`
- Worktree: primary checkout `/home/zhangy/git/ql-essvi`
- Commit: `12581d21a1aefa4e17a2fff84bf4eca69e9ea895`
- GitHub Issue: none (local tracker)
- Tests: baseline 6/6 CTest and 80/80 pytest; Red reproduced; focused 1/1 CTest and 19/19 Python; final 6/6 CTest and 80/80 pytest
- Diff review: changes required in round 1; isolated spec, standards, and verification outcomes are recorded above


## Split disposition

The user selected sequential tickets after round 1. This EBA-1 review record remains the historical review of the oversized implementation range; do not reuse its implementation or approval. The accepted behavior is being rebuilt as the dependent EBA-2 (covariance state/API), EBA-3 (constrained MAP), and EBA-4 (posterior covariance refresh, cold-start budget correction, and fallback documentation) ticket sequence under the approved parent spec. The original changes-requested outcome remains in force for this range.
