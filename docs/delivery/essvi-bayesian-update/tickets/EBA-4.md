# EBA-4 — Posterior covariance refresh and cold-fallback correction

## State

- Current: draft
- Last transition: none
- At: 2026-10-31
- Reason/evidence: Final sequential implementation slice under the user-approved parent spec; depends on EBA-3 posterior-mode behavior.
- Blocker: EBA-3

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
- The held-out replay records one-step-ahead band loss, movement, feasibility, fallback rate, and latency against existing warm and cold paths; it makes no unsupported predictive or performance claim.

## Out of scope

- New likelihoods, new timestamp/expiry matching, changes to quality/fallback policy, or changes to CSV/API schemas.
- Retuning on the held-out seed or treating the synthetic replay as market evidence.

## Test plan

- Add a regression with more than eight exact-expiry slices and invalid covariance; require successful valid cold results and aligned SPD covariance after the fallback budget is exceeded.
- Verify covariance refresh and its cold/fallback behavior through the public C++ warm API.
- Run the configured full command and the declared held-out scenario command from `docs/delivery/DELIVERY.md` before review.

## Blocked by

[EBA-3](EBA-3.md)

## Expected seam

`calibrateWarm`, `CalibrationResult::posterior_covariances`, fallback-budget diagnostics, and the documented generalized-Bayes result semantics.

## Evidence and decision references

- Decisions: DEC-001, DEC-002

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|

## Delivery record

- Base commit: set when EBA-3 is implemented and committed
- Baseline status: pending
- Branch: `delivery/essvi-bayesian-update/EBA-4`
- Worktree: primary checkout
- GitHub Issue: none (local tracker)
