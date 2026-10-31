# EBA-2 — Covariance state contract and Python snapshots

## State

- Current: implemented
- Last transition: implementing -> implemented
- At: 2026-10-31T13:34:44+00:00
- Reason/evidence: Red/green/refactor checkpoints and configured final verification passed; implementation commit `e9e4169a7fb8a7aa8aa3b244dae7c8b97c2b1203`.
- Blocker: None

## State history

| At | Transition | Evidence |
|---|---|---|
| 2026-10-31T13:26:02+00:00 | draft -> ready | Approved parent spec and user-selected sequential ticket split. |
| 2026-10-31T13:26:02+00:00 | ready -> baseline-checking | Claimed branch `delivery/essvi-bayesian-update/EBA-2` in the primary checkout. |
| 2026-10-31T13:28:46+00:00 | baseline-checking -> baseline-passed | Exact-base Release build, CTest 6/6, pytest 80/80 passed at `d7abde372760d8a16427ca4b91cbfed067f893c7`. |
| 2026-10-31T13:28:46+00:00 | baseline-passed -> implementing | Baseline passed; public C++/Python covariance result seam confirmed. |
| 2026-10-31T13:34:44+00:00 | implementing -> implemented | Red/green/refactor and final verification passed; implementation commit `e9e4169a7fb8a7aa8aa3b244dae7c8b97c2b1203`. |

## Claim

omp coding assistant

## Parent

[Implementation spec](../implementation-spec.md); split disposition: [EBA-1](EBA-1.md)

## What to build

Add the per-expiry covariance state to `CalibrationResult`, initialize a finite symmetric positive-definite diagonal state for every successful cold or warm result, validate an inherited covariance before reusing its exact-expiry surface, and expose immutable covariance snapshots through Python. This is the state/API foundation; it does not yet use the covariance to select a posterior mode or update its information from the current quotes.

## Acceptance criteria

- `CalibrationResult::posterior_covariances` is aligned one-to-one with `surface.slices`; each entry packs the symmetric covariance in `(log(theta), atanh(rho), log(psi))` order as `[00, 01, 02, 11, 12, 22]`.
- Every successful cold, warm, and fallback result has finite symmetric positive-definite covariance state; cold initialization uses a positive diagonal variance floor.
- An absent, malformed, or non-positive-definite covariance is not accepted as reusable exact-expiry state; the ordinary cold path remains available.
- Python exposes immutable tuple snapshots; cross-call posterior-state reuse is covered after EBA-3/EBA-4 make the returned state observation-dependent.
- Surface fitting, quote scoring, quality gates, fallback policy, CSV output, and the raw `objective` are unchanged by this state/API slice.

## Out of scope

- Using the covariance as a Gaussian prior or selecting a constrained posterior mode (EBA-3).
- Updating covariance from current quote information or changing forced-cold budget behavior (EBA-4).
- New quote-noise models, parameterization changes, or any changes listed as non-goals in the parent spec.

## Test plan

- Add public-seam C++ assertions for covariance alignment, finiteness, symmetry, and positive definiteness on cold and warm results; verify malformed state is not reused.
- Add Python coverage for immutable tuple snapshots and assignment rejection.
- Demonstrate the new public result field fails to compile on the exact source baseline, then run focused warm CTest and binding tests.
- Run the configured full command from `docs/delivery/DELIVERY.md` before review.

## Blocked by

None

## Expected seam

`CalibrationResult`, `calibrate`, `calibrateWarm`, and the read-only Python result properties; exercised through C++ and Python public interfaces.

## Evidence and decision references

- Decisions: DEC-001, DEC-002

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-31T13:28:46+00:00 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | repository root | `d7abde372760d8a16427ca4b91cbfed067f893c7` | passed | Release build; CTest 6/6; pytest 80/80. Source baseline ancestor: `497f0c37d27056ec2e6df631c0f570d80fe332f4`. |

| 2026-10-31 | Red | `cmake --build build --target essvi_warm_calibrator_tests -j2` | repository root | `164af54393d7e1f60888d6ddf72c2c8a4998bc9c` plus test-only working tree | expected failure (exit 2) | C++ compiler rejected the regression because `CalibrationResult::posterior_covariances` was not present. |
| 2026-10-31 | Green | `cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | repository root | working tree | passed | Warm CTest 1/1, including cold/warm covariance alignment and malformed-state cold-start assertions. |
| 2026-10-31 | Green | `uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q tests/test_python_bindings.py` | repository root | working tree | passed | Python bindings 20/20; read-only covariance snapshot behavior passed. |
| 2026-10-31 | Refactor | `cmake --build build --target essvi_warm_calibrator_tests -j2 && ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` | repository root | working tree | passed | Refactored assertions; warm CTest 1/1. |
| 2026-10-31 | Refactor | `.venv/bin/python -m pytest -q tests/test_python_bindings.py` | repository root | working tree | passed | Python bindings 20/20 after restricting this slice to immutable snapshot behavior. |
| 2026-10-31T13:34:44+00:00 | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` | repository root | `e9e4169a7fb8a7aa8aa3b244dae7c8b97c2b1203` | passed | Release build; CTest 6/6; pytest 81/81. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|

## Delivery record

- Base commit: `d7abde372760d8a16427ca4b91cbfed067f893c7`
- Source baseline commit: `497f0c37d27056ec2e6df631c0f570d80fe332f4`
- Baseline status: passed
- Branch: `delivery/essvi-bayesian-update/EBA-2`
- Worktree: primary checkout
- GitHub Issue: none (local tracker)
- Commit: `e9e4169a7fb8a7aa8aa3b244dae7c8b97c2b1203`
- Tests: baseline CTest 6/6 and pytest 80/80; focused warm CTest 1/1; Python bindings 20/20; final CTest 6/6 and pytest 81/81.
- Diff review: pending.
