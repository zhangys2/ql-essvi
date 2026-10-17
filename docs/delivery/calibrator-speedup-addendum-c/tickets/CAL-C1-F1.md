# CAL-C1-F1 — Handle zero total excess in contribution reporting

## State

- Current: `finished`
- Last transition: `review-approved` → `finished`
- At: `2026-10-17T14:10:17Z`
- Reason/evidence: baseline, Red/Green, JSON smoke, real-data recheck, final configured verification, and fresh review outcome are recorded below. No repository-specific learning qualified for capture.
- Blocker: None for CAL-C1-F1. CAL-C1 remains blocked pending a separate reconciliation of its ticket state.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17` | User approved a separate focused ticket for the unresolved zero-total-excess analyzer finding. Acceptance criteria and test plan are recorded here. |
| `ready` | `baseline-checking` | `2026-10-17` | Claimed by `omp coding assistant`; branch `delivery/calibrator-speedup-addendum-c/CAL-C1-F1` created from the preceding ticket's commit in the primary checkout. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T14:00:42Z` | Configured Release build passed; CTest 5/5 and pytest 13/13 passed at base `e4c76784a1a788e8f4b44dc0f3d7fd703d28382b`. |
| `baseline-passed` | `implementing` | `2026-10-17T14:00:59Z` | Baseline evidence and the focused contribution-aggregation test seam confirmed before the first red run. |
| `implementing` | `implemented` | `2026-10-17T14:03:54Z` | Commit `bc611413d699c5370007eba88fc6773f0cc2a1a4`; focused tests 5/5, zero-excess JSON smoke, real-data diagnosis 212/212, and configured full verification passed (CTest 5/5, pytest 15/15). |
| `implemented` | `reviewing` | `2026-10-17T14:04:23Z` | Fresh independent review requested on base `e4c76784a1a788e8f4b44dc0f3d7fd703d28382b` through `f391bdc1da96729ff32e03529436d5ab793fe411`. |
| `reviewing` | `review-approved` | `2026-10-17T14:09:14Z` | Review round 1: spec and standards approved with no findings; verification found no issues but did not run the command. Implementer-run full verification is recorded in the timeline. |
| `review-approved` | `finished` | `2026-10-17T14:10:17Z` | Final handoff recorded; C1 predecessor state remains separately blocked. |

## Claim

`omp coding assistant`

## Parent

- Blocking predecessor: [CAL-C1](CAL-C1.md)
- [Addendum C implementation spec](../implementation-spec.md)

## Problem and outcome

The analyzer computes each relative contribution as a bucket's squared total-variance excess divided by the global sum. If every quote is in-band, that denominator is zero and the report raises `ZeroDivisionError` instead of completing. This ticket defines the contribution share as not applicable (`None`, serialized as JSON `null`) when total squared excess is zero. Positive-total behavior remains unchanged.

## Acceptance criteria

- With a non-empty set of buckets and total squared excess equal to zero, contribution aggregation returns each bucket share as `None` and does not divide by zero.
- With positive total excess, bucket shares retain the existing `100 * bucket_excess / total_excess` semantics.
- A focused regression test covers both zero and positive totals.
- The current real IBIT training run still converges on 212/212 snapshots, and its nonzero contribution metrics remain unchanged.
- No quote filters, fitting objective, production code, or live collection behavior changes.

## Out of scope

- Reopening or re-reviewing the full CAL-C1 diff under this ticket.
- Selecting or implementing WP-C5 cleaning thresholds.
- Vega-weighted fitting, SPY, or any later work package.

## Test plan

- Add focused tests for zero-total and positive-total contribution aggregation in `tests/test_analyze_fit_quality.py`; run `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` red then green.
- Re-run `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit` on IBIT Oct 12–14; verify the actual nonzero contributions and 212/212 fits are unchanged. Oct 15–16 remain unused.
- Run the configured full verification at the ticket boundary.

## Blocked by

None for CAL-C1-F1. CAL-C1 remains blocked pending an explicit reconciliation of its blocked state after this follow-up has been reviewed.

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|
| 2026-10-17T14:00:42Z | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `e4c76784a1a788e8f4b44dc0f3d7fd703d28382b` | passed | Release build succeeded; CTest 5/5 passed; pytest 13/13 passed. |
| 2026-10-17 | Red | `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | The new zero/positive contribution tests failed at collection because `contribution_percentages` did not yet exist. |
| 2026-10-17 | Green | `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` | `/home/zhangy/git/ql-essvi` | working tree | passed | Added contribution aggregation with `None` for a zero total. Focused suite passed 5/5; tests cover zero total, positive shares, relative width, grouping, and source identity indexing. |
| 2026-10-17 | Zero-excess JSON smoke | `.venv/bin/python -c 'import json; from scripts.analyze_fit_quality import contribution_percentages; print(json.dumps(contribution_percentages({"expiry": {"near": {"sum_squared_total_variance_excess": 0.0}}}, 0.0), sort_keys=True))'` | `/home/zhangy/git/ql-essvi` | working tree | passed | Output was `{"expiry": {"near": null}}`; the not-applicable share serializes as JSON `null`. |
| 2026-10-17 | Real-data recheck | `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c1-f1.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | 212/212 cold fits converged; 113,965 quotes reconciled. Existing positive contribution metrics and relative-width bucket counts match the prior C1 run; Oct 15–16 were not read. |
| 2026-10-17T14:03:29Z | Final verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `bc61141` | passed | Release build succeeded; CTest 5/5 passed; pytest 15/15 passed. |
| 2026-10-17 | Refactor checkpoint | `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` | `/home/zhangy/git/ql-essvi` | `bc61141` | passed | Reviewed the helper and tests; no refactor was needed. Focused suite passed 5/5. |
| 2026-10-17T14:04:23Z | Review round 1 start | Base-to-HEAD review `e4c76784a1a788e8f4b44dc0f3d7fd703d28382b..f391bdc1da96729ff32e03529436d5ab793fe411`; three isolated axes | `/home/zhangy/git/ql-essvi` | `f391bdc1da96729ff32e03529436d5ab793fe411` | reviewing | Frozen diff `/tmp/cal-c1-f1-review.diff`, SHA-256 `4784684ec0067c2f8a15b05246cced2b597b684d6a9a8f834fb0b72f3c8fcdfc`; three independent reviewers. |
| 2026-10-17T14:09:14Z | Review round 1 | Same base-to-HEAD range; three isolated axes | `/home/zhangy/git/ql-essvi` | `f391bdc1da96729ff32e03529436d5ab793fe411` | approved | Spec: no findings. Standards: no findings. Verification: no findings, but reviewer did not run commands due higher-priority restriction; implementer-run configured full verification passed CTest 5/5 and pytest 15/15. External-data run remains implementer-reported. Snapshot SHA-256 `4784684ec0067c2f8a15b05246cced2b597b684d6a9a8f834fb0b72f3c8fcdfc`. |

## Review record

- Round 1 — approved. Spec and standards reviewers found no issues. Verification reviewer found no issue but did not run the configured command; the implementer-run full suite is the recorded test evidence. Reviewers did not run the external-data analysis.

## Delivery record

- Base commit: `e4c76784a1a788e8f4b44dc0f3d7fd703d28382b`
- Branch: `delivery/calibrator-speedup-addendum-c/CAL-C1-F1`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi`
- Commits: `bc61141` (zero-excess fix), `f391bdc` (verification record), final handoff-record commit follows.
- GitHub Issue: none (local tracker)
- Learning capture: none; no repository-specific reusable learning met the capture bar.

## Handoff

- Ticket: CAL-C1-F1 — Handle zero total excess in contribution reporting
- Branch: `delivery/calibrator-speedup-addendum-c/CAL-C1-F1`
- Implemented commit: `bc611413d699c5370007eba88fc6773f0cc2a1a4`
- Implemented: zero aggregate excess now reports contribution shares as JSON `null`; positive-total percentages retain the prior formula.
- Verification: focused analyzer tests 5/5; zero-excess JSON smoke passed; real-data C1 recheck 212/212; configured Release build passed, CTest 5/5, pytest 15/15.
- Diff review: approved; spec and standards no findings. Verification reviewer did not run tests due higher-priority restriction; the full suite was run by the implementer.
- Remaining risk/follow-up: original CAL-C1 remains blocked pending reconciliation; WP-C5 also requires the user's explicit cleaning-threshold approval.
- Merge danger: two-way door, blast radius `local`: analyzer diagnostic JSON only; no production calibration or collector behavior changes.
