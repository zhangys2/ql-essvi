# EBA-2 — Covariance state contract and Python snapshots

## State

- Current: draft
- Last transition: none
- At: 2026-10-31
- Reason/evidence: First sequential implementation slice under the user-approved constrained Bayesian eSSVI spec and selected ticket split.
- Blocker: None

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
- Python exposes immutable tuple snapshots, and a returned result can be passed into a later warm call without dropping native covariance state.
- Surface fitting, quote scoring, quality gates, fallback policy, CSV output, and the raw `objective` are unchanged by this state/API slice.

## Out of scope

- Using the covariance as a Gaussian prior or selecting a constrained posterior mode (EBA-3).
- Updating covariance from current quote information or changing forced-cold budget behavior (EBA-4).
- New quote-noise models, parameterization changes, or any changes listed as non-goals in the parent spec.

## Test plan

- Add public-seam C++ assertions for covariance alignment, finiteness, symmetry, and positive definiteness on cold and warm results; verify malformed state is not reused.
- Add Python coverage for immutable snapshots and consecutive-result reuse.
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

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|

## Delivery record

- Base commit: `497f0c37d27056ec2e6df631c0f570d80fe332f4` (source baseline)
- Baseline status: pending
- Branch: `delivery/essvi-bayesian-update/EBA-2`
- Worktree: primary checkout
- GitHub Issue: none (local tracker)
