# CAL-C1-REVIEW — Re-accept the complete IBIT C1 deliverable

## State

- Current: `implemented`
- Last transition: `implementing` → `implemented`
- At: `2026-10-17T14:17:41Z`
- Reason/evidence: review-only deliverable is committed at `bfe35f735fd007f137368e6e9326085d1dea553b`; final configured verification passed at this HEAD. The original C1 analysis and F1 zero-excess correction are included for whole-deliverable review.
- Blocker: None for this replacement ticket. WP-C5 remains gated on explicit cleaning-threshold approval.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17` | User approved a replacement ticket to review the complete C1 deliverable on the fixed branch under a fresh ticket. Scope and review plan are recorded here. |
| `ready` | `baseline-checking` | `2026-10-17` | Claimed by `omp coding assistant`; branch `delivery/calibrator-speedup-addendum-c/CAL-C1-REVIEW` created from the finished CAL-C1-F1 branch. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T14:12:18Z` | Reused exact-base baseline evidence at `ef6c788b8e2479b9aa74a1b595fc9844d9094082` from CAL-C1: Release build passed, CTest 5/5, pytest 10/10. |
| `baseline-passed` | `implementing` | `2026-10-17T14:14:50Z` | Review-only scope confirmed; source code already exists on the fixed branch. Fresh configured verification, real-data recheck, and full-diff review are being performed. |
| `implementing` | `implemented` | `2026-10-17T14:17:41Z` | Review-only full-deliverable scope confirmed; current committed deliverable passed fresh configured verification at `bfe35f735fd007f137368e6e9326085d1dea553b`. No code edits are in this ticket. |

## Claim

`omp coding assistant`

## Parent and predecessor records

- Supersedes the acceptance/review attempt in [CAL-C1](CAL-C1.md); the original ticket remains blocked as an append-only historical record.
- Includes the fix from [CAL-C1-F1](CAL-C1-F1.md), which was independently reviewed and finished.
- [Addendum C implementation spec](../implementation-spec.md); source brief: [Addendum C](../../../calibrator-speedup-addendum-c.md)

## What to accept

Re-accept the complete C1 deliverable against the full diff from the original base `ef6c788b8e2479b9aa74a1b595fc9844d9094082`: supplied spec copy, IBIT-only cold-fit diagnosis, reproducible analyzer, report and candidate thresholds, tests, and the zero-total-excess contribution correction. No new source behavior is being added by this ticket; the already committed output is being re-reviewed as one complete deliverable after the original ticket reached its review cap.

## Acceptance criteria

- The source spec copy `docs/calibrator-speedup-addendum-c.md` is preserved; its SHA-256 matches the supplied file.
- `docs/fit-quality.md` and the analyzer report the IBIT Oct 12–14 cold-fit breakdown by expiry, moneyness, premium, relative total-variance band width, option type, and fetch position, plus the underlying-move check.
- Oct 15–16 remain unused for threshold development and reserved for the one-time WP-C7 evaluation; SPY remains deferred.
- Cleaning policies remain proposals, not selected/implemented thresholds; WP-C5 waits for explicit user approval.
- The analyzer completes when global squared total-variance excess is zero, returning `null` contribution shares; positive-total contributions preserve the previous formula.
- The report and script make no production filtering, fitting-objective, or live collector changes.

## Out of scope

- Selecting or implementing WP-C5 filters.
- Vega-weighting, parity forwards, synchrony changes, American inversion, capacity work, or WP-C7/C8 implementation.
- Changing the previously reviewed source code or report during this acceptance/review ticket.

## Test plan

This ticket replaces a completed implementation with a fresh whole-deliverable review; TDD code edits are not applicable. Red/Green evidence for the analyzer and its correction remains recorded in CAL-C1 and CAL-C1-F1.

- Run the configured full verification command at this ticket boundary.
- Run the real IBIT training diagnosis on Oct 12–14 and compare with the checked-in report; do not read Oct 15–16.
- Freeze the full diff from `ef6c788b8e2479b9aa74a1b595fc9844d9094082` through this ticket's HEAD, then run the three independent review axes.

## Decisions

- User decisions retained: IBIT first, SPY later; mandatory vega-weighted loss; vega error aggregation; report-only A/B; WP-C7 approved.
- DEC-006 remains unresolved until the user selects cleaning thresholds.

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|
| 2026-10-17 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `ef6c788b8e2479b9aa74a1b595fc9844d9094082` | passed | Reused from CAL-C1's exact-base baseline: Release build succeeded; CTest 5/5; pytest 10/10. |
| 2026-10-17 | Spec-copy checksum | `sha256sum /home/zhangy/work/ql-essvi-addendum-c.md docs/calibrator-speedup-addendum-c.md` | `/home/zhangy/git/ql-essvi` | `0f625dba8d2cac43efe2cac6c4641f78c6522570` | passed | Both files have SHA-256 `e504c545c1e7b8074c9149e297bd14df1fd581d89327597b0f09076a8621010a`. |
| 2026-10-17T14:16:23Z | Full verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `0f625dba8d2cac43efe2cac6c4641f78c6522570` | passed | Release build succeeded; CTest 5/5 passed; pytest 15/15 passed. |
| 2026-10-17 | Real-data C1 recheck | `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c1-review.json` | `/home/zhangy/git/ql-essvi` | `0f625dba8d2cac43efe2cac6c4641f78c6522570` | passed | 212/212 cold fits converged on Oct 12–14; 113,965 valid quotes reconciled against 259,978 source contracts. Overall above-cap share 71.71%, RMS 2.410. Relative total-variance width groups contain 110,355 / 3,597 / 13 quotes; Oct 15–16 were not read. |
| 2026-10-17T14:17:41Z | Final verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `bfe35f735fd007f137368e6e9326085d1dea553b` | passed | Release build succeeded; CTest 5/5 passed; pytest 15/15 passed. |

## Delivery record

- Review base: `ef6c788b8e2479b9aa74a1b595fc9844d9094082`
- Branch: `delivery/calibrator-speedup-addendum-c/CAL-C1-REVIEW`
- Branch start: finished CAL-C1-F1 branch at `0f625dba8d2cac43efe2cac6c4641f78c6522570`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi`
- Review: fresh three-axis review required; this does not append a fourth round to CAL-C1.
