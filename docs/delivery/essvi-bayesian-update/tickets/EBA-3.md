# EBA-3 — Constrained posterior mode from current evidence

## State

- Current: implementing
- Last transition: baseline-passed -> implementing
- At: 2026-10-31T14:08:51Z
- Reason/evidence: Baseline passed; `calibrateWarm` and `CalibrationResult` public seam confirmed for prior influence, feasibility, and raw-objective behavior.
- Blocker: None

## State history

| At | Transition | Evidence |
|---|---|---|
| 2026-10-31T14:06:25Z | draft -> ready | Approved parent spec; EBA-2 finished at `f2335ed4b26e4c9275f8f85a27f163502740df0b`; EBA-3 acceptance criteria and test plan are settled. |
| 2026-10-31T14:07:20Z | ready -> baseline-checking | Claimed branch `delivery/essvi-bayesian-update/EBA-3`; baseline worktree `/tmp/ql-essvi-eba3-baseline` is detached at the declared base. |
| 2026-10-31T14:08:18Z | baseline-checking -> baseline-passed | Exact-base checkout at `f2335ed4b26e4c9275f8f85a27f163502740df0b` passed the configured Release build, CTest 6/6, and pytest 81/81. |
| 2026-10-31T14:08:51Z | baseline-passed -> implementing | Baseline timeline row recorded; public `calibrateWarm`/`CalibrationResult` seam confirmed. |

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); [EBA-2](EBA-2.md)

## What to build

Use the exact-expiry previous mean and usable covariance as a Gaussian prior in transformed eSSVI coordinates. Refine accepted warm candidates to the constrained posterior mode using the complete current quote snapshot once, with the existing normalized bid/ask band loss as generalized-Bayes evidence. Keep hard butterfly/calendar feasibility, quote caps, and the legacy raw band-loss `objective` intact.

## Acceptance criteria

- With the same informative current snapshot, changing the prior covariance changes the fitted mode; the update is not merely a warm start for raw band loss.
- The posterior objective is the existing weighted band energy normalized by `bandLossScale` plus the Gaussian prior quadratic in `(log(theta), atanh(rho), log(psi))`.
- `previous_quotes` affects identity/reuse decisions only; varying it independently while holding prior result and current quotes fixed does not add likelihood evidence.
- Every posterior-refined candidate passes existing butterfly/calendar hard checks and the existing quote-cap gate; rejected candidates continue through the established fallback sequence.
- `CalibrationResult::objective`, diagnostics, and CSV contracts continue to report the existing raw band loss and remain compatible.
- The covariance state is carried for every returned slice; covariance information refresh is completed in EBA-4.

## Out of scope

- Replacing band loss with a generative interval-censored model or using mid/band-center fitting.
- Updating posterior covariance from current observations (EBA-4).
- Changing fallback policy, no-arbitrage bounds, quality thresholds, or schemas.

## Test plan

- Add public-seam tests that vary the prior under fixed informative current quotes and independently vary `previous_quotes` while keeping the prior and current quotes fixed.
- Assert raw-objective consistency, quote-cap behavior, deterministic output, and dense-grid feasibility for posterior-refined results.
- Run focused warm CTest and the configured full verification command before review.

## Blocked by

[EBA-2](EBA-2.md)

## Expected seam

`calibrateWarm` and `CalibrationResult`, exercised through public warm updates and existing quality/feasibility checks.

## Evidence and decision references

- Decisions: DEC-001, DEC-002

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|
| 2026-10-31T14:08:18Z | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | `/tmp/ql-essvi-eba3-baseline` | `f2335ed4b26e4c9275f8f85a27f163502740df0b` | passed | Release build; CTest 6/6; pytest 81/81. |
| 2026-10-31T14:09:51Z | Red | `cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | primary checkout | `f2335ed4b26e4c9275f8f85a27f163502740df0b` + test-only working tree | expected failure (exit 8) | `constrainedPosteriorModeUsesGaussianPrior` failed because diffuse prior movement did not exceed concentrated movement; covariance currently does not influence the fitted surface (DEC-002, with DEC-001 band evidence). |
| 2026-10-31T14:15:50Z | Green | `cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | primary checkout | `f2335ed4b26e4c9275f8f85a27f163502740df0b` + implementation working tree | passed | Constrained warm MAP now responds to concentrated versus diffuse transformed-state covariance; current-only band evidence is used and `CalibrationResult::objective` matches independently computed raw band loss. Prior covariance prediction adds one process-noise step; hard feasibility and quote cap gate every accepted step. |
| 2026-10-31T14:15:50Z | Green | Same focused warm CTest command | primary checkout | implementation working tree | passed | Varying only `previous_quotes` bid/ask/mid values while holding the prior result and current quotes fixed leaves the fitted surface and raw objective unchanged. |
| 2026-10-31T14:15:50Z | Refactor | Same focused warm CTest command | primary checkout | implementation working tree | passed | Replaced stale exact-reuse/raw-loss-versus-cold assertions with public invariants: cap acceptance, slice accounting, strict calendar feasibility, dense-grid feasibility, deterministic repeats, and raw-objective consistency. |
| 2026-10-31T14:38:05Z | Refactor/Green | `cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | primary checkout | implementation working tree | passed | Reused the existing constrained LM for Gaussian posterior scoring; covariance influence, previous_quotes independence, raw objective, quote-cap, and feasibility regressions passed. |
| 2026-10-31T14:40:39Z | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | primary checkout | `6cc0213415f5885e0f6a5e970d123cae3f50b9e3` | passed | Release build; CTest 6/6; pytest 81/81. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|

## Delivery record

- Base commit: `f2335ed4b26e4c9275f8f85a27f163502740df0b`
- Baseline status: passed
- Branch: `delivery/essvi-bayesian-update/EBA-3`
- Worktree: primary checkout
- GitHub Issue: none (local tracker)
