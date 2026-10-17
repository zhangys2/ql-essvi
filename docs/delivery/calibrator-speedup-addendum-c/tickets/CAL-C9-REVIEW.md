# CAL-C9-REVIEW — Re-accept the complete vega-weighted C9 deliverable

## State

- Current: `implemented`
- Last transition: `implementing` → `implemented`
- At: `2026-10-17T18:09:01Z`
- Reason/evidence: Review-only re-acceptance passed the configured Release build, CTest 5/5, pytest 33/33, focused expiry-parity coverage, and training-only C5/A-B reruns. The fixed production implementation remains unchanged.
- Blocker: None.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17T18:03:07Z` | User approved C9; the complete implementation and review scope are recorded here. |
| `ready` | `baseline-checking` | `2026-10-17T18:03:38Z` | Claimed by `omp coding assistant`; review branch created from fixed C9 commit `1dac3aa1ca3239397f4b5eeee8ceab6cb1ab0cd5`. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T18:05:45Z` | New branch configured Release build passed; CTest 5/5 and pytest 33/33. |
| `baseline-passed` | `implementing` | `2026-10-17T18:05:52Z` | Review-only scope confirmed; code is unchanged from the fixed C9 deliverable. Focused expiry-parity test passed 1/1. |
| `implementing` | `implemented` | `2026-10-17T18:09:01Z` | Review-only scope complete; corrected C5/A-B results match the checked-in report and configured verification passed. No source behavior was changed. |

## Claim

`omp coding assistant`

## Parent and predecessor records

- Re-accepts the complete C9 deliverable from [CAL-C9](CAL-C9.md); the original ticket remains blocked as an append-only historical record.
- [Addendum C implementation spec](../implementation-spec.md); source handoff: [Addendum C](../../../calibrator-speedup-addendum-c.md).
- Follows the review-only fresh-ticket pattern in [CAL-C1-REVIEW](CAL-C1-REVIEW.md).

## Review base and fixed deliverable

- Review base: `fdf70b8b80ce6c9b617141293bda99a560be3e07` (the exact C9 implementation base after WP-C5).
- Fixed C9 deliverable: `1dac3aa1ca3239397f4b5eeee8ceab6cb1ab0cd5`, including the expiry-parity correction `43b3fca89339d2385415bb0845fe1716a6833a4e` and the blocked-state record for the original ticket.
- Review scope: full base-to-HEAD diff from the review base through this ticket's frozen HEAD; no new production behavior is planned.

## What to accept

Re-accept the full C9 deliverable after the original CAL-C9 review cap was reached: mandatory median-normalized Black–Scholes vega weighting, cold and warm objective/gate use, normalized-CSV compatibility, invalid-weight rejection, unchanged six-column `diagnostics.csv`, report-only IBIT A/B, and converter/replay expiry parity. The corrected implementation is already committed; this ticket performs fresh configured verification and an independent full-diff review without changing production behavior.

## User decisions and evidence

- **DEC-002 — mandatory vega-weighted band loss:** preserve the bid/ask band objective and use median-normalized Black–Scholes price sensitivity; no mid-price target.
- **DEC-003 — report-only A/B:** compare unit and vega-weighted cold fits on cleaned IBIT; the comparison does not choose the production objective.
- IBIT Oct 12–14 is the only evaluation scope for this ticket. Do not inspect Oct 15–16 or SPY.
- The user's “c9 approved” approves this re-acceptance scope; it is not represented as an independent review result.

## Acceptance criteria

- The fixed C9 diff satisfies all acceptance criteria in [CAL-C9](CAL-C9.md), including finite-positive normalized weights for included quotes, blank weights for excluded rows, legacy unit-weight defaults, weighted cold/warm calculations, unchanged diagnostics schema, and price-/vega-space reporting.
- Converter and replay agree on New York session date, raw calendar-day year fraction, same-day/past-expiry exclusion, included rows, and normalized weights before snapshot-median normalization.
- The training-only C9 A/B is reproducible on Oct 12–14 IBIT with the approved C5 combined profile; both arms report per-snapshot counts and quality metrics. The result is report-only and does not alter mandatory vega weighting.
- Full configured verification passes at this ticket boundary; no Oct 15–16 or SPY source data is read.
- A fresh independent three-axis review approves the frozen base-to-HEAD snapshot. If review findings require corrections, they are dispositioned and verified within this ticket's review cap.

## Out of scope

- New production-code behavior, changing the C5 filters, changing `diagnostics.csv`, mid-centering, SPY, Oct 15–16, live collector changes, or advancing to WP-C2.
- Reopening the original CAL-C9 history or rewriting its prior review findings.

## Test plan

This ticket re-accepts a completed implementation; production-code TDD is not applicable. Red/Green and prior correction evidence remain in CAL-C9.

- Run the configured full verification command from `docs/delivery/DELIVERY.md` on the fixed branch.
- Run the corrected C5 refit and report-only vega A/B on IBIT Oct 12–14 only; compare generated coverage and metrics with `docs/fit-quality.md`.
- Run focused converter/replay expiry-parity coverage.
- Freeze the complete diff from `fdf70b8b80ce6c9b617141293bda99a560be3e07` through this ticket's fixed HEAD, including ticket metadata, then run the three independent spec, standards, and verification axes.

## Expected seam

The user-facing result is `docs/fit-quality.md`. Review the shared expiry and weight-preparation seam in `python/convert_ibit_snapshots.py`, `python/replay_snapshots.py`, and `python/vega_weights.py`, the weight validation/objective path in `cpp/calibrator.cpp` and `cpp/calibrator.hpp`, the normalized CSV contract, and the C9 behavioral tests and evaluator.

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|
| 2026-10-17T18:05:45Z | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `9866e5502c2e49649e5dd29d0e0ae7893ff1469d` | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |
| 2026-10-17T18:05:52Z | Focused expiry-parity check | `.venv/bin/python -m pytest -q tests/test_vega_weighting.py::test_converter_and_replay_emit_identical_vega_weights` | `/home/zhangy/git/ql-essvi` | working tree | passed | Cross-UTC-midnight fixture verifies the shared New York date, same-day/past exclusion, next-session inclusion, and matching converter/replay weights; 1 passed. |
| 2026-10-17T18:08:22Z | Corrected C5 training rerun | `.venv/bin/python scripts/evaluate_quote_cleaning.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c9-review-c5.json` | `/home/zhangy/git/ql-essvi` | `af5d518fc9e09d7e196198b45840bf3e56051ce4` | passed | All five profiles completed 212/212 fits on Oct 12–14 only. Combined: 90,700 quotes; 22,548 unique cleaning exclusions (22,450 premium, 3,253 spread, 11,212 delta); share above cap 69.10%, RMS ratio 2.1742, max 64.1403. Metrics match `docs/fit-quality.md`. |
| 2026-10-17T18:08:22Z | Corrected report-only A/B | `.venv/bin/python scripts/evaluate_vega_weighting.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c9-review-ab.json` | `/home/zhangy/git/ql-essvi` | `af5d518fc9e09d7e196198b45840bf3e56051ce4` | passed | Both arms completed 212/212 fits on 90,700 quotes. Unit objective `0.3268406267822395`; weighted objective `0.1334024602632656`. Output confirms only Oct 12–14 inputs; no holdout or SPY was read. Metrics match `docs/fit-quality.md`. |
| 2026-10-17T18:09:01Z | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `af5d518fc9e09d7e196198b45840bf3e56051ce4` | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|

## Review record

No review has been run for this fresh ticket.
