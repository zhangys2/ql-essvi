# CAL-C1 — Decompose IBIT cold-fit misses and propose cleaning rules

## State

- Current: `changes-requested`
- Last transition: `reviewing` → `changes-requested`
- At: `2026-10-17T12:43:30Z`
- Reason/evidence: round 2 spec review found a missing breakdown of relative total-variance band width; see the frozen review snapshot and disposition below. The correction is implemented and verified; awaiting commit and fresh review.
- Blocker: None for CAL-C1. WP-C5 remains gated on the user's threshold approval.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17` | Approved implementation scope; acceptance criteria and test plan recorded here. |
| `ready` | `baseline-checking` | `2026-10-17` | Claimed by `omp coding assistant`; primary-checkout branch created at the declared base. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T11:55:05Z` | Configured full verification passed at the declared base: CTest 5/5 and pytest 10/10. |
| `baseline-passed` | `implementing` | `2026-10-17T12:25:43Z` | Administrative transition recorded after the analysis run; the ticket remains analysis-only and changes no production behavior. |
| `implementing` | `implemented` | `2026-10-17T12:26:11Z` | Commit `f15bc16`; real-data runner passed; final configured build and test command passed. The full external dataset is not repository-resident, so the reproducible data run is the acceptance test; no production behavior changed. |
| `implemented` | `reviewing` | `2026-10-17T12:26:56Z` | Fresh review requested on base `ef6c788b8e2479b9aa74a1b595fc9844d9094082` through committed HEAD `ad3b8425855cb3001442163951f65c2b2b694ea7`; frozen snapshot included the ticket-only review-start update. |
| `reviewing` | `changes-requested` | `2026-10-17T12:31:07Z` | Isolated round 1 review: spec approved; standards requested two P2 corrections and one P3 clarification; verification passed the configured command. |
| `changes-requested` | `reviewing` | `2026-10-17T12:37:59Z` | Corrections committed in `cdbdfc3`; focused regression and configured full verification passed; fresh round 2 requested. |
| `reviewing` | `changes-requested` | `2026-10-17T12:43:30Z` | Isolated round 2 review found the required relative total-variance band-width breakdown missing; see review disposition. |

## Claim

`omp coding assistant`

## Parent

[Implementation spec](../implementation-spec.md); source handoff: [Addendum C](../../../calibrator-speedup-addendum-c.md)

## What to build

Produce a reproducible read-only diagnosis of cold-fit misses on the IBIT training snapshots (2026-10-12 through 2026-10-14). Decompose current-pipeline quality by expiry, moneyness, premium, relative band width, option type, and contract fetch position; analyze the signed residual against fetch position and underlying movement during collection. Use these training-day distributions to propose, not implement, cleaning thresholds. Preserve Oct 15–16 for WP-C7's one-time holdout evaluation.

## Acceptance criteria

- `docs/fit-quality.md` records cold-only baseline results for IBIT Oct 12–14, with quote counts and share-above-cap/RMS excess-ratio breakdowns by all required dimensions.
- The report includes the fetch-position and within-fetch underlying-move analysis, with sample counts and method stated; it does not claim causality.
- It proposes data-derived candidates for minimum bid, minimum premium, maximum relative spread, and strike/delta window, with each rule's individual and combined exclusion/retention counts on training days.
- No proposed threshold is selected using Oct 15–16; those days are not used in this ticket's tuning or results table.
- `docs/calibrator-speedup-addendum-c.md` is present in the repository as the supplied spec copy.
- No production code or live collector behavior changes in this analysis ticket.

## Out of scope

- Implementing or selecting the final cleaning thresholds; the user approves the final list before CAL-C5.
- SPY analysis before IBIT is settled.
- Any live IB collector change.

## Test plan

- `tests/test_analyze_fit_quality.py` covers malformed raw-contract identity indexing, preserving original positions, the relative total-variance band-width normalization, and grouped quote-quality metrics. The complete historical dataset is external to the repository, so the analysis runner is also exercised end-to-end on the real IBIT training data.
- Run from the repository root: `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit`. It reads only Oct 12–14, runs one cold CLI fit per usable snapshot, verifies convergence and quote-count reconciliation, and prints JSON to stdout; `docs/fit-quality.md` is the checked-in report transcribed from that output.
- Run the configured full verification at the ticket boundary.

## Blocked by

None for CAL-C1.

## Expected seam

The user-visible result is `docs/fit-quality.md`, with reproducible JSON output from `scripts/analyze_fit_quality.py` using `python/convert_ibit_snapshots.py`, `python/replay_snapshots.py`, and the existing cold calibration CLI. This ticket adds no second production filtering convention.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-005, DEC-006

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-17 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `ef6c788b8e2479b9aa74a1b595fc9844d9094082` | passed | Release build succeeded; CTest 5/5 passed; pytest 10/10 passed. |
| 2026-10-17 | Real-data diagnosis | `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit` | `/home/zhangy/git/ql-essvi` | `f15bc16` | passed | 212/212 independent cold fits converged on 2026-10-12–14; 113,965 valid quotes; 259,978 source contracts reconciled; holdout dates not read. Results and methods are in `docs/fit-quality.md`. |
| 2026-10-17 | Review round 1 start | Base-to-HEAD review `ef6c788b8e2479b9aa74a1b595fc9844d9094082..ad3b8425855cb3001442163951f65c2b2b694ea7`; three isolated axes | `/home/zhangy/git/ql-essvi` | `ad3b8425855cb3001442163951f65c2b2b694ea7` | reviewing | Frozen diff SHA-256 `d8b11f8d4cbf7d5dd22c29174b7a74ddbf5fc1315f663bdf87efead3199e3949`; three independent reviewers. |
| 2026-10-17T12:31:07Z | Review round 1 | Base-to-HEAD snapshot `ef6c788b8e2479b9aa74a1b595fc9844d9094082..ad3b8425855cb3001442163951f65c2b2b694ea7`; three isolated axes | `/home/zhangy/git/ql-essvi` | `ad3b8425855cb3001442163951f65c2b2b694ea7` | changes required | Spec: no findings. Standards: P2 non-object contract handling, P2 unapproved `noqa` suppressions, P3 runner-output wording. Verification: full command passed CTest 5/5 and pytest 10/10; data run treated as implementer-reported. |
| 2026-10-17 | Red | `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | The new identity-indexing regression could not import `source_contract_index`, proving the missing behavior before correction. |
| 2026-10-17 | Green | `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` | `/home/zhangy/git/ql-essvi` | working tree | passed | Added the shared source-contract index helper; malformed non-dictionary entries are skipped without shifting original list indexes. Focused test passed 1/1. |
| 2026-10-17 | Corrected real-data diagnosis | `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit` | `/home/zhangy/git/ql-essvi` | `cdbdfc3` | passed | 212/212 cold fits, 113,965 valid quotes, 259,978 source contracts reconciled; original metrics and candidate counts unchanged. JSON output and checked-in Markdown relationship clarified. |
| 2026-10-17 | Final verification after round-1 fixes | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `cdbdfc3` | passed | Release build succeeded; CTest 5/5 passed; pytest 11/11 passed. |
| 2026-10-17T12:37:59Z | Review round 2 start | Base-to-HEAD review `ef6c788b8e2479b9aa74a1b595fc9844d9094082..cdbdfc3868f2dd2620e20e852ee4e47a7ab44407`; three isolated axes | `/home/zhangy/git/ql-essvi` | `cdbdfc3868f2dd2620e20e852ee4e47` | reviewing | Frozen diff `/tmp/cal-c1-round2.diff`, SHA-256 `52a68031d3aac9cfa547a4d61d82a7b61edb0aa7676a0e32e454b0136d596fb0`; three isolated reviewers. All round-1 findings resolved and reverified. |
| 2026-10-17T12:43:30Z | Review round 2 | Base-to-HEAD snapshot `ef6c788b8e2479b9aa74a1b595fc9844d9094082..cdbdfc3868f2dd2620e20e852ee4e47a7ab44407`; three isolated axes | `/home/zhangy/git/ql-essvi` | `cdbdfc3868f2dd2620e20e852ee4e47` | changes required | Spec: P2 missing a breakdown by relative bid/ask total-variance band width (Addendum C §5, WP-C1); current table is price spread `(ask-bid)/mid`. Standards: approved, no findings. Verification: reviewer did not run the configured command; the prior full verification and external-data results were implementer-reported at `cdbdfc3`. Frozen diff SHA-256 `52a68031d3aac9cfa547a4d61d82a7b61edb0aa7676a0e32e454b0136d596fb0`. |
| 2026-10-17 | Red | `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | The new assertions failed at collection because `grouped_band_width_quality` was not yet implemented. |
| 2026-10-17 | Green | `.venv/bin/python -m pytest -q tests/test_analyze_fit_quality.py` | `/home/zhangy/git/ql-essvi` | working tree | passed | Added midpoint-normalized total-variance band width and bucket aggregation; tests verify the formula, bucket assignment, quote counts, cap share, and RMS. Focused suite passed 3/3. |
| 2026-10-17 | Corrected real-data diagnosis | `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c1-round3.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | 212/212 cold fits converged; 113,965 quotes reconciled. New band-width groups account for 110,355 / 3,597 / 13 quotes (all included quotes); the JSON records the definition and per-group quality metrics. Holdout dates were not read. |
| 2026-10-17 | Final verification after round-2 correction | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release build succeeded; CTest 5/5 passed; pytest 13/13 passed. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |
| 1 — Standards P2 (judgement) | Identity indexing calls `.get()` on every raw contract although replay preparation skips non-dictionary entries. | Resolved | Added `source_contract_index`, which skips non-dictionary entries while preserving original list indexes. `tests/test_analyze_fit_quality.py` failed before and passed after; the real-data run still converges on 212/212 snapshots. |
| 1 — Standards P2 (mechanical) | Added `# noqa: E402` suppressions have no ticket waiver or configured linter. | Resolved | Removed both new `noqa` comments; no linter or suppression check was added. |
| 1 — Standards P3 (judgement) | Ticket says the runner emits the Markdown report, but it prints JSON to stdout. | Resolved | Ticket and report now state that the runner prints JSON and the checked-in Markdown report is transcribed from it. |
| 2 — Spec P2 (mechanical) | Addendum C §5 WP-C1 requires a breakdown by relative band width; the report only groups by relative option-price spread `(ask-bid)/mid`. | Resolved | Added a separate relative total-variance width `(ask variance - bid variance) / midpoint variance` breakdown with quote counts, above-cap share, RMS, maxima, and summed-excess contribution. The analyzer and report both distinguish this from option-price spread. The test passed, and the real-data runner produced 3,597, 13, and 110,355 quotes in the observed width buckets, totaling 113,965. |

## Gate candidates

- Proposed: deterministic scan of added Python lines for suppression pragmas (`# noqa`, `# type: ignore`, and equivalents), requiring an explicit ticket waiver. No such check is configured today.

## Delivery record

- Base commit: `ef6c788b8e2479b9aa74a1b595fc9844d9094082`
- Baseline status: passed
- Baseline verification: configured Release build, CTest 5/5, pytest 10/10 at base.
- Branch: `delivery/calibrator-speedup-addendum-c/CAL-C1`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi`
- Commits: `f15bc16` (C1 diagnosis), `ad3b842` (implementation verification record), `cdbdfc3` (round-1 corrections); round-2 correction awaiting commit and final review.
- GitHub Issue: none (local tracker)
- Tests: C1 analyzer-focused tests passed 3/3; real-data analysis passed on 212/212 training snapshots; configured CTest 5/5 and pytest 13/13 passed after the round-2 correction.
- Diff review: round 1 findings corrected; round 2 spec finding corrected; third and final review pending.
