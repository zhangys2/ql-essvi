# EBA-3 — Constrained posterior mode from current evidence

## State

- Current: draft
- Last transition: none
- At: 2026-10-31
- Reason/evidence: Sequential implementation slice under the user-approved parent spec; depends on EBA-2 covariance state.
- Blocker: EBA-2

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

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|

## Delivery record

- Base commit: set when EBA-2 is implemented and committed
- Baseline status: pending
- Branch: `delivery/essvi-bayesian-update/EBA-3`
- Worktree: primary checkout
- GitHub Issue: none (local tracker)
