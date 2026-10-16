# CAL-B6 — Cold-path regression A/B

## State

- Current: ready
- Last transition: `draft` → `ready`
- At: 2026-10-16T11:24:10Z
- Reason/evidence: parent implementation spec approved by user; `docs/delivery/calibrator-speedup-addendum-b/implementation-spec.md`
- Blocker: CAL-B5

## Claim

None

## Parent

[Implementation spec](../implementation-spec.md); source brief: none (decision-only intake)

## What to build

Compare the current cold path against `build/wp5-parallel` on the same machine, with matched seed, benchmark arguments, compiler/flags, and reported load averages. Verify the comparison binary's provenance first. If an actual regression beyond measurement noise is shown, restore cold latency to the approved single-slice and full-chain thresholds without violating the parent output tolerance.

## Acceptance criteria

- Record compiler, effective flags, thread cap, seeds, benchmark dimensions, repeats, and load averages for both binaries; mark the A/B inconclusive if provenance or flags cannot be established.
- Run 40 expiries × 100 quotes for nine repeats and one expiry × 100 quotes for 20 repeats.
- If the current cold path regresses beyond measurement noise, identify and fix the cause; single-slice median is ≤2.5 ms and full-chain median ≤55 ms, with output within the parent spec tolerance.
- If no regression is found, make no unrelated cold-path edits and record measurements and uncertainty.
- Preserve deterministic behavior, the four-thread maximum, and all existing correctness checks.

## Out of scope

- Warm gate/solver tuning except a minimal change necessary to restore the measured cold path.
- Unverified comparison claims based solely on an old binary with unknown build settings.

## Test plan

- Verify `build/wp5-parallel` provenance and flags, then run the paired A/B sizes and repeats on the same machine/seed with before/after load averages.
- Run cold regression and dense-grid correctness tests if code changes are necessary.
- Run configured full CTest and `.venv/bin/python -m pytest -q` at the ticket boundary.

## Blocked by

- CAL-B5 — final Addendum B regression coverage.

## Expected seam

Cold calibration path and benchmark reporting, likely `cpp/calibrator.cpp`, `bench/essvi_bench.cpp`, and `docs/perf-results.md`. Do not edit warm behavior unless required by the measured cold-path fix.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-005, DEC-006, DEC-007

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
