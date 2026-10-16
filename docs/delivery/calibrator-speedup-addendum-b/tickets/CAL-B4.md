# CAL-B4 — Bounded staged fallback and degraded results

## State

- Current: ready
- Last transition: `draft` → `ready`
- At: 2026-10-16T11:24:10Z
- Reason/evidence: parent implementation spec approved by user; `docs/delivery/calibrator-speedup-addendum-b/implementation-spec.md`
- Blocker: CAL-B3

## Claim

None

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Implement the local → restricted cold → full cold stages with a per-update budget of at most eight full cold searches by default. On budget exhaustion, return the best feasible validated surface as `path=degraded`, report achieved quality and degraded-slice ages/counts, and refit pending slices oldest-first. Force a full cold refit for a slice after three consecutive degraded returned updates, regardless of the ordinary budget.

## Acceptance criteria

- A candidate failing the local gate receives the restricted cold search around prior rho ±0.15 and within the feasible psi interval; full cold runs only when the restricted stage still fails the approved gate.
- At most eight full cold searches run per update, except a forced refit for a slice whose degraded age exceeded the approved limit; no optional wall-clock deadline is enabled by default.
- Budget exhaustion returns the best feasible surface found with `path=degraded`, `degraded_slices`, achieved loss/quality, worst-slice max/RMS ratio, and per-slice age; the full result passes `validate()` and dense-grid checks.
- Pending slices are refit oldest-first. A slice degraded for three consecutive outputs is forced through full cold on the following update, even beyond the ordinary budget; age resets only when the slice passes the gate.
- If no feasible validated surface can be returned, report failure with a non-empty error; never label an invalid surface successful.
- A valid result following the bounded policy has `converged=true` and `path=degraded`; the shared CSV headers remain unchanged.

## Out of scope

- Replacing the gate/reuse test (CAL-B2) or local solver core (CAL-B3).
- The broader four-scenario × 50-update regression sequence (CAL-B5).

## Test plan

- Add deterministic tests for stage order, full-cold budget boundaries, valid degraded return, exact counts/ages, oldest-first refit, forced full cold after three degraded outputs, and infeasible/no-result failure.
- Check each returned surface through `validate()` and the existing dense-grid butterfly/calendar scan.
- Run focused warm CTest and the configured full CTest/Python suites.

## Blocked by

- CAL-B3 — bounded deterministic local solver and reason observability.

## Expected seam

`cpp/calibrator.hpp`, `cpp/calibrator.cpp`, `tests/test_warm_calibrator.cpp`, benchmark aggregation in `bench/essvi_bench.cpp`, and `docs/perf-results.md`.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-005, DEC-007, DEC-013

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
