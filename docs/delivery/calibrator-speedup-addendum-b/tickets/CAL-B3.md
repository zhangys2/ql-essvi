# CAL-B3 — Better local solver and fallback reasons

## State

- Current: ready
- Last transition: `draft` → `ready`
- At: 2026-10-16T11:24:10Z
- Reason/evidence: parent implementation spec approved by user; `docs/delivery/calibrator-speedup-addendum-b/implementation-spec.md`
- Blocker: CAL-B2

## Claim

None

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Improve changed-slice local fitting with deterministic multi-start in the bounded previous-rho window, an ATM-anchored theta start, Levenberg-Marquardt damping, zero-loss early stop, and an approximately 25-objective-evaluation per-slice budget. Count and expose the specified fallback/rejection reasons. Never accept an infeasible candidate, and preserve deterministic reduction with no more than four threads.

## Acceptance criteria

- The local search uses three deterministic starts within previous rho ±0.05 and respects the feasible psi interval and all existing butterfly/calendar constraints.
- Theta starts from the nearest ATM quote as specified; damping and zero-loss early termination are active; objective evaluations are bounded at about 25 per slice.
- Result/bench output includes counts for infeasible local result, loss above gate, calendar repair failed, and bound check failed; scenario reports include reason histograms.
- Repeated identical inputs return identical parameters and diagnostics relevant to search ordering; no infeasible point is accepted.
- Local candidate quality and all gate decisions continue to satisfy CAL-B2; no D17-D22 semantics change.

## Out of scope

- Per-quote gate definition and reuse policy (CAL-B2).
- Budgeted staged fallback and degraded state (CAL-B4).

## Test plan

- Add deterministic behavior tests for all three starts, ATM initialization, early stop, evaluation bound, fallback reason categories, feasibility rejection, and repeatability through the calibrator seam.
- Run focused warm CTest and configured full CTest/Python suites.
- Re-run the four-scenario/five-seed benchmark to show reason histograms and quality/latency after the local solver change.

## Blocked by

- CAL-B2 — per-quote gate and reuse semantics.

## Expected seam

`cpp/calibrator.cpp`, `cpp/calibrator.hpp` result observability, `tests/test_warm_calibrator.cpp`, and benchmark reason aggregation in `bench/essvi_bench.cpp`.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-005, DEC-007

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |

## Delivery record

- Base commit: `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536`
- Baseline status: not started
- Baseline verification: pending
- Baseline failure evidence: None
- Unconfirmed commands: typecheck/lint not applicable; no configured linter
- Branch: pending claim
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi` (configured checkout form)
- Commit: pending
- GitHub Issue: none (local tracker)
- Tests: pending
- Diff review: pending
