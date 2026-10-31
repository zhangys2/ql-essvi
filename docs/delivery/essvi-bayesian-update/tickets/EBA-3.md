# EBA-3 — Constrained posterior mode from current evidence

## State

- Current: changes-requested
- Last transition: reviewing -> changes-requested
- At: 2026-10-31T15:07:19Z
- Reason/evidence: Round 2 matched base f2335ed4b26e4c9275f8f85a27f163502740df0b, HEAD ac824419f95014026932c225d31ef1ff71878c42, snapshot SHA-256 07aaecf3f426bf88343e30af9abb83fa760b36bcd10ff5397556e62e26d6e7a5, and spec SHA-256 5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea. Independent review found one spec P2, one standards blocker plus one standards P2, and two verification P2 coverage gaps; all are accepted below. Implement and verify before round 3, the final available round.
- Blocker: None

## State history

| At | Transition | Evidence |
|---|---|---|
| 2026-10-31T14:06:25Z | draft -> ready | Approved parent spec; EBA-2 finished at `f2335ed4b26e4c9275f8f85a27f163502740df0b`; EBA-3 acceptance criteria and test plan are settled. |
| 2026-10-31T14:07:20Z | ready -> baseline-checking | Claimed branch `delivery/essvi-bayesian-update/EBA-3`; baseline worktree `/tmp/ql-essvi-eba3-baseline` is detached at the declared base. |
| 2026-10-31T14:08:18Z | baseline-checking -> baseline-passed | Exact-base checkout at `f2335ed4b26e4c9275f8f85a27f163502740df0b` passed the configured Release build, CTest 6/6, and pytest 81/81. |
| 2026-10-31T14:08:51Z | baseline-passed -> implementing | Baseline timeline row recorded; public `calibrateWarm`/`CalibrationResult` seam confirmed. |
| 2026-10-31T14:43:34Z | implementing -> implemented | Implementation commit `6cc0213415f5885e0f6a5e970d123cae3f50b9e3`; focused and configured final verification passed. |
| 2026-10-31T14:44:58Z | implemented -> reviewing | Round 1 input pinned to base f2335ed4b26e4c9275f8f85a27f163502740df0b, HEAD 8639750b4879406f0ece579164c74ebb46611c9c, snapshot SHA-256 ca9a84873d2c557f9e10ddf039cc185a4ada083d02e39c55aee135ec6214db4c, and spec SHA-256 5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea. |
| 2026-10-31T14:47:30Z | reviewing -> changes-requested | Round 1 invalidated after identifying half-scaled data gradient/Hessian; old base/HEAD/snapshot identity is recorded in the preceding transition and the round-1 input manifest. New review requires a committed correction and fresh final verification. |
| 2026-10-31T14:59:16Z | changes-requested -> reviewing | Corrections committed; final configured verification passed; round-1 provisional findings dispositioned; request round 2 under the three-round review cap. |
| 2026-10-31T15:07:19Z | reviewing -> changes-requested | Round 2 input identity matched; independent spec, standards, and verification axes require corrections; round 3 is the last available review round. |

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
- Assert the predicted covariance is carried across successive warm results and malformed covariance cold-starts.
- Cross the large-move threshold using only historical quote mids; verify the MAP route depends on the prior state and current snapshot, not historical quote values.
- Cover valid covariance on an unmatched expiry and assert predicted covariance is retained through a fallback result.
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
| 2026-10-31T14:44:58Z | Review round 1 input | Base f2335ed4b26e4c9275f8f85a27f163502740df0b; HEAD 8639750b4879406f0ece579164c74ebb46611c9c; snapshot SHA-256 ca9a84873d2c557f9e10ddf039cc185a4ada083d02e39c55aee135ec6214db4c; spec SHA-256 5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea. | primary checkout | 8639750b4879406f0ece579164c74ebb46611c9c | review requested | Frozen snapshot /tmp/EBA-3-review-round1.patch; zero untracked files; Final timeline row is the verification pointer. |
| 2026-10-31T14:47:30Z | Review round 1 invalidated | Posterior score/gradient scaling correction required | primary checkout | 8639750b4879406f0ece579164c74ebb46611c9c | invalidated | Round 1 snapshot ca9a84873d2c557f9e10ddf039cc185a4ada083d02e39c55aee135ec6214db4c is stale; rerun focused/full verification and freeze a fresh round-2 input after correction. |
| 2026-10-31T14:55:59Z | Red | `cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | primary checkout | `8639750b4879406f0ece579164c74ebb46611c9c` + reproduced half-scale source and scalar-mode regression working tree | expected failure (exit 8) | One-quote ATM posterior-mode oracle failed with data gradient/Hessian scaled by 0.5/loss_scale; expected formula follows DEC-001/DEC-002. |
| 2026-10-31T14:55:59Z | Red | Same focused warm CTest command | primary checkout | `8639750b4879406f0ece579164c74ebb46611c9c` + covariance-carry regression working tree | expected failure (exit 8) | Warm result covariance reset to cold initialization instead of carrying the predicted prior state; repeated warm updates could not propagate uncertainty. |
| 2026-10-31T14:55:59Z | Green | Same focused warm CTest command | primary checkout | `8639750b4879406f0ece579164c74ebb46611c9c` + correction working tree | passed | Scalar posterior mode matches the independently solved normalized-band-plus-Gaussian-prior equation; warm outputs retain the predicted covariance and add process noise once per update. |
| 2026-10-31T14:58:08Z | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | primary checkout | `4bb24e04e8b495e5dc4afcb187bfccefd19853f3` | passed | Release build; CTest 6/6; pytest 81/81 after score-scaling and covariance-carry corrections. |
| 2026-10-31T15:07:19Z | Review round 2 input | Base f2335ed4b26e4c9275f8f85a27f163502740df0b; HEAD ac824419f95014026932c225d31ef1ff71878c42; snapshot SHA-256 07aaecf3f426bf88343e30af9abb83fa760b36bcd10ff5397556e62e26d6e7a5; spec SHA-256 5e634c44225a6aa8ac7124a18ba64343f0ad08fcddf567112828a4b54a7b8cea. | primary checkout | ac824419f95014026932c225d31ef1ff71878c42 | frozen | Three isolated axes matched the pinned snapshot; readiness and diff helpers passed; no untracked files. User-owned EBA-2 metadata edit excluded from this range. |
| 2026-10-31T15:07:19Z | Review round 2 | Three independent axes; full configured verification rerun on rails | primary checkout | base f2335ed4b26e4c9275f8f85a27f163502740df0b to HEAD ac824419f95014026932c225d31ef1ff71878c42 | changes required | Spec P2: historical mids can bypass posterior refinement. Standards: restore strict-safe fallback parameter assertions; update warm covariance docs. Verification: full suite passed CTest 6/6 and pytest 81/81; add unmatched-expiry and fallback-covariance regressions. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|
| 1 (invalidated; provisional spec) | MISSING P2 — warm output reset covariance instead of carrying the prior state | accepted | EBA-3 criterion 42 requires covariance state on every returned slice; output now carries the once-predicted covariance. EBA-4 remains responsible for active quote-information refresh; the focused covariance-chain regression passes. |
| 1 (invalidated; provisional standards) | Blocking — 251 added non-fixture lines | rejected | The provisional count included test additions; the deterministic helper sizes implementation/non-fixture additions separately. The fresh round-2 snapshot will be checked with `check_diff_spec.py`; its result is the gate evidence. |
| 1 (invalidated; provisional verification) | UNTESTED — valid covariance attached to a nonmatching expiry | accepted | EBA-4 will test a valid covariance attached to a different expiry and require the exact-expiry cold-start path, as required by the parent spec. |
| 2 (spec P2) | Historical `previous_quotes` mids can bypass posterior refinement through the large-move shortcut | accepted | Replace the old-snapshot movement input with current-quote displacement from the prior surface and add a public threshold-crossing test that changes only historical mids. |
| 2 (standards blocker) | Failed calendar-repair fallback no longer asserts retention of the prior strict-safe slice | accepted | Restore exact `theta`, `psi`, and `rho` assertions against `old_next` in the NaN-mid no-ATM fixture; retain the calendar and dense-grid checks. |
| 2 (standards P2) | README says warm results always use cold diagonal covariance | accepted | Document cold initialization separately from warm predicted covariance; state that EBA-3 warm outputs add one process-noise step and do not yet refresh from quote information. |
| 2 (verification P2) | Valid covariance on an unmatched expiry is not tested | accepted | Add a public-seam case with a valid covariance vector attached to a different expiry and assert cold-start behavior. |
| 2 (verification P2) | Fallback covariance propagation is checked only for SPD, not retained prior state | accepted | Assert fallback outputs equal the expected one-step prediction from non-default covariance for each exact-expiry slice. |

## Delivery record

- Base commit: `f2335ed4b26e4c9275f8f85a27f163502740df0b`
- Baseline status: passed
- Branch: `delivery/essvi-bayesian-update/EBA-3`
- Worktree: primary checkout
- GitHub Issue: none (local tracker)
