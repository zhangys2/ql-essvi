# CAL-B2 — Per-quote cap gate and reuse drift

## State

- Current: ready
- Last transition: `draft` → `ready`
- At: 2026-10-16T11:24:10Z
- Reason/evidence: parent implementation spec approved by user; `docs/delivery/calibrator-speedup-addendum-b/implementation-spec.md`
- Blocker: CAL-B7

## Claim

None

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Replace the `loss_tol * bandLossScale` acceptance and reuse tests with the approved per-quote total-variance excess-ratio cap. Cache a same-snapshot full-cold reference only when a candidate fails the base cap, apply the per-quote `max(cap, cold_ratio + 0.01)` threshold, and bound unchanged-slice reuse by the approved drift limit. Preserve `loss_tol` only as deprecated compatibility state; remove the scheduled `cold_check_interval` path as approved in the spec.

## Acceptance criteria

- `WarmOptions::quote_cap` defaults to 0.1 and supports sensitivity values 0.05, 0.1, and 0.2.
- Width is `max(ask_variance - bid_variance, 1e-6)`; per-quote excess and ratio follow the parent spec. A candidate passes the base gate only when each included quote is within its own cap.
- If a local/reuse candidate fails the base cap, the restricted then full-cold sequence and per-quote adaptive threshold follow the approved spec; same-snapshot full-cold ratios are cached and never substituted by prior-snapshot ratios.
- Reuse also passes the per-quote gate, calendar/predecessor checks, and the maximum 12 consecutive-update drift limit.
- `loss_tol` remains available but cannot affect gate/reuse acceptance; the obsolete periodic cold-check option is removed and callers migrated.
- Existing test expectations are not changed silently. If an existing assertion conflicts with the approved gate, the conflict is recorded and handled only as a named, separately justified change.

## Out of scope

- Snapshot-derived scenario measurements/replay (CAL-B7), solver quality improvements (CAL-B3), and staged/degraded fallback (CAL-B4).

## Test plan

- Add focused public-seam tests for per-quote boundary equality, a single outlier quote, width-floor handling, adaptive cold-reference behavior/cache, and reuse at the drift limit.
- Run `ctest --test-dir build -R essvi_warm_calibrator_tests --output-on-failure` after the build and at the ticket boundary.
- Run the configured CTest and `.venv/bin/python -m pytest -q` suites at the ticket boundary.

## Blocked by

- CAL-B7 — measured scenario calibration and replay.

## Expected seam

`cpp/calibrator.hpp`, `cpp/calibrator.cpp`, all in-repository `calibrateWarm` callsites, and `tests/test_warm_calibrator.cpp`. Tests use `calibrateWarm` through its public result/state transition.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-007, DEC-011, DEC-012

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
