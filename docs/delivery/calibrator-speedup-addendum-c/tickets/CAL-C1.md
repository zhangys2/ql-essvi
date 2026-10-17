# CAL-C1 — Decompose IBIT cold-fit misses and propose cleaning rules

## State

- Current: `implementing`
- Last transition: `baseline-passed` → `implementing`
- At: `2026-10-17T12:25:43Z`
- Reason/evidence: the baseline passed; the report and reproducible read-only analyzer are implemented. This transition was recorded after the analysis run; no behavior-changing production code is in scope.
- Blocker: None for CAL-C1. WP-C5 remains gated on the user's threshold approval.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17` | Approved implementation scope; acceptance criteria and test plan recorded here. |
| `ready` | `baseline-checking` | `2026-10-17` | Claimed by `omp coding assistant`; primary-checkout branch created at the declared base. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T11:55:05Z` | Configured full verification passed at the declared base: CTest 5/5 and pytest 10/10. |
| `baseline-passed` | `implementing` | `2026-10-17T12:25:43Z` | Administrative transition recorded after the analysis run; the ticket remains analysis-only and changes no production behavior. |

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

- No permanent unit test is appropriate for this read-only analysis: the historical raw snapshot files are external to the repository, and C1 changes no calibrator or collector behavior. The reproducible analysis runner is exercised end-to-end on the real IBIT training data.
- Run from the repository root: `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit`. It reads only Oct 12–14, runs one cold CLI fit per usable snapshot, verifies convergence and quote-count reconciliation, and emits the metrics in `docs/fit-quality.md`.
- Run the configured full verification at the ticket boundary.

## Blocked by

None for CAL-C1.

## Expected seam

The user-visible result is `docs/fit-quality.md`, reproduced by `scripts/analyze_fit_quality.py` using `python/convert_ibit_snapshots.py`, `python/replay_snapshots.py`, and the existing cold calibration CLI. This ticket adds no second production filtering convention.

## Evidence and decision references

- Claims: none (decision-only intake)
- Decisions: DEC-001, DEC-005, DEC-006

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-10-17 | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `ef6c788b8e2479b9aa74a1b595fc9844d9094082` | passed | Release build succeeded; CTest 5/5 passed; pytest 10/10 passed. |
| 2026-10-17 | Real-data diagnosis | `python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit` | `/home/zhangy/git/ql-essvi` | working tree | passed | 212/212 independent cold fits converged on 2026-10-12–14; 113,965 valid quotes; 259,978 source contracts reconciled; holdout dates not read. Results and methods are in `docs/fit-quality.md`. |
| 2026-10-17 | Final verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release build succeeded; CTest 5/5 passed; pytest 10/10 passed. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |

## Delivery record

- Base commit: `ef6c788b8e2479b9aa74a1b595fc9844d9094082`
- Baseline status: passed
- Baseline verification: configured Release build, CTest 5/5, pytest 10/10 at base.
- Branch: `delivery/calibrator-speedup-addendum-c/CAL-C1`
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi`
- Commit: pending
- GitHub Issue: none (local tracker)
- Tests: real IBIT C1 analysis passed; final configured CTest 5/5 and pytest 10/10 passed. No permanent test added (analysis-only ticket; external raw-data dependency).
- Diff review: pending.
