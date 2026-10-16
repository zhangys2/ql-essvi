# CAL-B5 — Addendum B regression coverage

## State

- Current: ready
- Last transition: `draft` → `ready`
- At: 2026-10-16T11:24:10Z
- Reason/evidence: parent implementation spec approved by user; `docs/delivery/calibrator-speedup-addendum-b/implementation-spec.md`
- Blocker: CAL-B4

## Claim

None

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Add a deterministic four-scenario sequence of 50 updates per scenario that fails when the Addendum B hard quality limits are exceeded, plus direct behavior coverage for valid degraded results and their reported counts/ages. Preserve all existing test expectations unless a named conflict is explicitly resolved.

## Acceptance criteria

- The scripted quiet, normal, fast, and front sequences each run 50 updates deterministically and compare observable quality against Addendum B hard limits.
- Regression failure reports the scenario, seed, update, metric, observed value, and applicable hard limit.
- Degraded-result tests assert `path=degraded`, valid surface, correct degraded counts/ages, and bounded fallback behavior; no test merely checks non-throw or incidental defaults.
- Existing expectations are preserved; any conflict with the approved new gate is named, justified against DEC-007, and separately resolved before changing that expectation.

## Out of scope

- Changing solver behavior or acceptance thresholds (CAL-B2/CAL-B3/CAL-B4).
- Five-seed performance runner and full-session replay (CAL-B1/CAL-B7).

## Test plan

- Run the focused warm CTest and deterministic 4 × 50 regression sequence; verify a deliberately failing-quality fixture causes the named hard-limit assertion to fail only if the test setup itself is a distinct, controlled scenario.
- Run configured full CTest and `.venv/bin/python -m pytest -q` at the ticket boundary.
- Run the five-seed verification and dense-grid safety scan after the new suite passes.

## Blocked by

- CAL-B4 — staged fallback and degraded result semantics.

## Expected seam

`tests/test_warm_calibrator.cpp` and a focused scenario-sequence test seam; only small benchmark adjustments in `bench/essvi_bench.cpp` if required to expose the accepted metric API.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-002, DEC-007, DEC-011, DEC-012, DEC-013

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
