# CAL-C1-REVIEW — Re-accept the complete IBIT C1 deliverable

## State

- Current: `reviewing`
- Last transition: `changes-requested` → `reviewing`
- At: `2026-10-17T14:31:18Z`
- Reason/evidence: round 1's accepted P2 test-coverage finding was corrected in commit `72b4b427fa50e9e6d2c5ad76a154565927796175`. The new synthetic analyzer-output regression passed; configured verification passed (CTest 5/5, pytest 16/16); the real IBIT training run remains 212/212 and matches the report. Requesting fresh round 2 review of the complete deliverable.
- Blocker: None external. WP-C5 remains gated on explicit cleaning-threshold approval.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17` | User approved a replacement ticket to review the complete C1 deliverable on the fixed branch under a fresh ticket. Scope and review plan are recorded here. |
| `ready` | `baseline-checking` | `2026-10-17` | Claimed by `omp coding assistant`; branch `delivery/calibrator-speedup-addendum-c/CAL-C1-REVIEW` created from the finished CAL-C1-F1 branch. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T14:12:18Z` | Reused exact-base baseline evidence at `ef6c788b8e2479b9aa74a1b595fc9844d9094082` from CAL-C1: Release build passed, CTest 5/5, pytest 10/10. |
| `baseline-passed` | `implementing` | `2026-10-17T14:14:50Z` | Review-only scope confirmed; source code already exists on the fixed branch. Fresh configured verification, real-data recheck, and full-diff review are being performed. |
| `implementing` | `implemented` | `2026-10-17T14:17:41Z` | Review-only full-deliverable scope confirmed; current committed deliverable passed fresh configured verification at `bfe35f735fd007f137368e6e9326085d1dea553b`. No code edits are in this ticket. |
| `implemented` | `reviewing` | `2026-10-17T14:19:02Z` | Fresh three-axis review requested on the full C1 deliverable after configured verification passed. |
| `reviewing` | `changes-requested` | `2026-10-17T14:25:33Z` | Round 1 found a P2 permanent-test gap for the analyzer's aggregate/JSON output path; see Review round 1 and disposition. |
| `changes-requested` | `reviewing` | `2026-10-17T14:31:18Z` | Accepted correction committed in `72b4b427fa50e9e6d2c5ad76a154565927796175`; focused analyzer tests 6/6, Release build passed, CTest 5/5, pytest 16/16, and the real-data C1 recheck passed. A fresh full-deliverable review is requested. |

## Claim

`omp coding assistant`

## Parent and predecessor records

- Supersedes the acceptance/review attempt in [CAL-C1](CAL-C1.md); the original ticket remains blocked as an append-only historical record.
- Includes the fix from [CAL-C1-F1](CAL-C1-F1.md), which was independently reviewed and finished.
- [Addendum C implementation spec](../implementation-spec.md); source brief: [Addendum C](../../../calibrator-speedup-addendum-c.md)

## What to accept

Re-accept the complete C1 deliverable against the full diff from the original base `ef6c788b8e2479b9aa74a1b595fc9844d9094082`: supplied spec copy, IBIT-only cold-fit diagnosis, reproducible analyzer, report and candidate thresholds, tests, and the zero-total-excess contribution correction. No new production behavior is being added; the already committed output is being re-reviewed as one complete deliverable after the original ticket reached its review cap.

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
- Changing production analyzer behavior or the checked-in report during this acceptance/review ticket; behavior-preserving regression coverage for accepted C1 output is in scope.

## Test plan

This ticket re-accepts a completed implementation; production-code TDD is not applicable. Red/Green evidence for the analyzer and its F1 correction remains recorded in CAL-C1 and CAL-C1-F1. The round-1 verification correction adds permanent synthetic coverage for analyzer aggregation and zero-total JSON output, without changing production behavior.

- Run the configured full verification command at this ticket boundary.
- Run the real IBIT training diagnosis on Oct 12–14 and compare with the checked-in report; do not read Oct 15–16.
- Add a synthetic fixture that exercises `analyze()` with zero total excess, serializes contributions as JSON `null`, and demonstrates only Oct 12–14 files are consumed.
- Freeze the full diff from `ef6c788b8e2479b9aa74a1b595fc9844d9094082` through this ticket's HEAD, then run the three independent review axes.

## Expected seam

`docs/fit-quality.md` is the user-facing output. `scripts/analyze_fit_quality.py` reuses `python/convert_ibit_snapshots.py`, `python/replay_snapshots.py`, and the existing cold `essvi_fit` CLI; this ticket does not alter the production quote filter or fitting objective.

## Evidence and decision references

- Claims: none (decision-only intake).
- Decisions: DEC-001, DEC-005, DEC-006. DEC-006 remains unresolved pending the user's cleaning-threshold selection.

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
| 2026-10-17 | Real-data C1 recheck | `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c1-review.json` | `/home/zhangy/git/ql-essvi` | `0f625dba8d2cac43efe2cac6c4641f78c6522570` | passed | 212/212 cold fits converged on Oct 12–14; 113,965 valid quotes reconciled against 259,978 source contracts. Overall above-cap share 71.71%, RMS 2.410. Relative total-variance width groups contain 110,355 / 3,597 / 13 quotes; Oct 15–16 were not read. Saved JSON metrics match the checked-in report at displayed precision. |
| 2026-10-17T14:17:41Z | Final verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `bfe35f735fd007f137368e6e9326085d1dea553b` | passed | Release build succeeded; CTest 5/5 passed; pytest 15/15 passed. |
| 2026-10-17T14:19:02Z | Review round 1 start | `review-delivery-diff`; base-to-HEAD `ef6c788b8e2479b9aa74a1b595fc9844d9094082..9004baa22953b70aecf9dc7e782966279e4a4098`; three isolated axes | `/home/zhangy/git/ql-essvi` | `9004baa22953b70aecf9dc7e782966279e4a4098` | reviewing | Frozen snapshot `/tmp/cal-c1-review-round1.diff`, SHA-256 `8b02dfba2bc0712a29488386280322d2b7fd257b1659acbde5bdcabedac51ddc`; no untracked files. |
| 2026-10-17T14:25:33Z | Review round 1 | Same full base-to-HEAD range; three isolated axes | `/home/zhangy/git/ql-essvi` | `9004baa22953b70aecf9dc7e782966279e4a4098` | changes required | Spec: approved, no findings. Standards: no findings. Verification: P2 UNTESTED at `scripts/analyze_fit_quality.py:388–425`; no permanent analyzer aggregation/JSON-output test. Reviewer did not run the configured command due a higher-priority restriction. Implementer full verification passed at `bfe35f7`; the real-data JSON artifact was inspected and matches the report at displayed precision. Snapshot SHA-256 `8b02dfba2bc0712a29488386280322d2b7fd257b1659acbde5bdcabedac51ddc`. |
| 2026-10-17 | Review correction regression | `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` | `/home/zhangy/git/ql-essvi` | working tree | passed | 6/6 passed. The new synthetic test drives `analysis.main()` over temporary Oct 12–14 inputs plus holdout sentinels; all grouped contributions serialize as JSON `null`, and only the three training files are consumed. No production behavior changed. |
| 2026-10-17 | Final verification after review correction | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release build succeeded; CTest 5/5 passed; pytest 16/16 passed. |
| 2026-10-17 | Real-data recheck after review correction | `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c1-review.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | Saved JSON confirms 212/212 cold fits, 113,965 valid quotes, and only training files for Oct 12–14. Overall and width-group metrics match the report at displayed precision; Oct 15–16 were not read. |

## Review record

### Spec review — isolated

Approved. No findings. C1 uses IBIT cold fits on Oct 12–14 only; reports the required breakdowns, relative total-variance width, fetch-position and underlying-move checks, ranked diagnosis, and unselected cleaning candidates. Oct 15–16 remain reserved for WP-C7, SPY is deferred, and production behavior is unchanged.

### Standards review — isolated

No findings. Metric and bucket counts were internally consistent; source quote identity and raw positions follow existing converter/replay conventions. No weakened checks, suppressions, or configured-command changes. Gate candidates: None.

### Verification review — isolated

**Changes required.** P2 UNTESTED — add a permanent synthetic regression through `analyze()` and the report JSON serialization path for zero total excess, and assert the fixed Oct 12–14 training-file selection. Evidence: `scripts/analyze_fit_quality.py:388–425` aggregates the public report from training files; existing `tests/test_analyze_fit_quality.py` only covers helper functions and does not exercise `analyze()` or its report output. The reviewer inspected the recorded real-data JSON but did not execute the configured command due a higher-priority restriction. Implementer-run full verification passed; no false claim or confirmed behavior failure.

Load-bearing facts graded rung 4 from implementer-recorded real-data execution and F1 tests/JSON smoke; not independently executed by the reviewer. The all-in-band analyzer output path remains the permanent-test gap to correct.

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |
| 1 — Verification P2 (UNTESTED) — Analyzer output path lacks a permanent regression | accepted | Add synthetic coverage through `analyze()` for zero-total JSON output and fixed training dates; focused command and correction commit will be recorded in the round-2 correction evidence. |

## Correction evidence — round 1

The accepted finding was a permanent test-coverage gap; the behavior itself already existed in the F1-corrected analyzer. No production edit or Red/Green claim was needed. The synthetic test now covers the public CLI output plus the holdout exclusion; it passed with the configured CTest/pytest suite and the real IBIT training rerun (timeline rows above). Correction commit: `72b4b427fa50e9e6d2c5ad76a154565927796175`.

## Gate candidates

None.

## Delivery record

- Review base: `ef6c788b8e2479b9aa74a1b595fc9844d9094082`
- Branch: `delivery/calibrator-speedup-addendum-c/CAL-C1-REVIEW`
- Branch start: finished CAL-C1-F1 branch at `0f625dba8d2cac43efe2cac6c4641f78c6522570`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi`
- Correction commit: `72b4b427fa50e9e6d2c5ad76a154565927796175` (permanent analyzer output regression test; no production behavior changed).
- Review round 1 snapshot: SHA-256 `8b02dfba2bc0712a29488386280322d2b7fd257b1659acbde5bdcabedac51ddc`.
- Remaining: fresh round 2 review outcome; DEC-006 threshold approval remains a separate gate before WP-C5.
