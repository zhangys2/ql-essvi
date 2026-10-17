# CAL-C9-REVIEW — Re-accept the complete vega-weighted C9 deliverable

## State

- Current: `implementing`
- Last transition: `changes-requested` → `implementing`
- At: `2026-10-17T18:26:56Z`
- Reason/evidence: Round 1 corrections are implemented: converter locked-quote eligibility now matches replay; parity coverage includes a locked quote; schema tests cover invalid included weights; and the synthetic A/B evaluator test uses the real fitter and asserts report outcomes. Focused vega tests passed 5/5 and configured verification passed; training-only reruns and fresh review remain.
- Blocker: None.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17T18:03:07Z` | User approved C9; the complete implementation and review scope are recorded here. |
| `ready` | `baseline-checking` | `2026-10-17T18:03:38Z` | Claimed by `omp coding assistant`; review branch created from fixed C9 commit `1dac3aa1ca3239397f4b5eeee8ceab6cb1ab0cd5`. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T18:05:45Z` | New branch configured Release build passed; CTest 5/5 and pytest 33/33. |
| `baseline-passed` | `implementing` | `2026-10-17T18:05:52Z` | Review-only scope confirmed; code is unchanged from the fixed C9 deliverable. Focused expiry-parity test passed 1/1. |
| `implementing` | `implemented` | `2026-10-17T18:09:01Z` | Review-only scope complete; corrected C5/A-B results match the checked-in report and configured verification passed. No source behavior was changed. |
| `implemented` | `reviewing` | `2026-10-17T18:09:43Z` | Fixed C9 deliverable and final verification are ready; a frozen full-diff snapshot is being sent to independent spec, standards, and verification reviewers. |
| `reviewing` | `changes-requested` | `2026-10-17T18:16:47Z` | Round 1 found P2 spec/standards and verification gaps on the frozen C9 deliverable; findings are recorded below for correction. |
| `changes-requested` | `implementing` | `2026-10-17T18:26:56Z` | Accepted round-one corrections are implemented; focused vega tests and configured verification passed. Training-only reruns and fresh review remain. |

## Claim

`omp coding assistant`

## Parent and predecessor records

- Re-accepts the complete C9 deliverable from [CAL-C9](CAL-C9.md); the original ticket remains blocked as an append-only historical record.
- [Addendum C implementation spec](../implementation-spec.md); source handoff: [Addendum C](../../../calibrator-speedup-addendum-c.md).
- Follows the review-only fresh-ticket pattern in [CAL-C1-REVIEW](CAL-C1-REVIEW.md).

## Review base and fixed deliverable

- Review base: `fdf70b8b80ce6c9b617141293bda99a560be3e07` (the exact C9 implementation base after WP-C5).
- Original fixed C9 deliverable: `1dac3aa1ca3239397f4b5eeee8ceab6cb1ab0cd5`, including the expiry-parity correction `43b3fca89339d2385415bb0845fe1716a6833a4e` and the blocked-state record for the original ticket.
- Round 1 reviewed the full base-to-HEAD diff ending at `f9b777c02e91bc43cd213f06c3c24b19f26373b9`. The next review range keeps the same base and adds only accepted review corrections, focused coverage, and this ticket's evidence.

## What to accept

Re-accept the full C9 deliverable after the original CAL-C9 review cap was reached: mandatory median-normalized Black–Scholes vega weighting, cold and warm objective/gate use, normalized-CSV compatibility, invalid-weight rejection, unchanged six-column `diagnostics.csv`, report-only IBIT A/B, and converter/replay expiry parity. Round-one corrections align converter locked-quote eligibility with replay and strengthen behavioral coverage; they do not change the objective or A/B decision.

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

- Production behavior beyond corrections required to satisfy the accepted review findings; changing C5 thresholds, changing `diagnostics.csv`, mid-centering, SPY, Oct 15–16, live collector changes, or advancing to WP-C2.
- Reopening the original CAL-C9 history or rewriting its prior review findings.

## Test plan

The original C9 implementation's red/green evidence remains in CAL-C9. Round-one corrections add focused locked-quote parity, invalid schema-weight boundaries, and real-fitter A/B report coverage.

- Run the configured full verification command from `docs/delivery/DELIVERY.md` on the corrected branch.
- Run the corrected C5 refit and report-only vega A/B on IBIT Oct 12–14 only; compare generated coverage and metrics with `docs/fit-quality.md`.
- Run focused converter/replay expiry- and locked-quote parity coverage.
- Freeze the complete diff from `fdf70b8b80ce6c9b617141293bda99a560be3e07` through this ticket's corrected HEAD, including ticket metadata, then run the three independent spec, standards, and verification axes.

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
| 2026-10-17T18:16:47Z | Review round 1 | Frozen base-to-HEAD snapshot `fdf70b8b80ce6c9b617141293bda99a560be3e07..f9b777c02e91bc43cd213f06c3c24b19f26373b9`; SHA-256 `19304d5ebd63b1a9c345998bb8dd4e2fd418506d5a5be3a78d2af82927658943` | `/home/zhangy/git/ql-essvi` | `f9b777c02e91bc43cd213f06c3c24b19f26373b9` | changes required | Three isolated axes completed. Configured Release configure/build, CTest 5/5, and pytest 33/33 passed. Spec/standards findings and two untested verification gaps are recorded below. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|
| 1 | P2: converter includes locked quotes that replay excludes, shifting median-normalized weights. | accepted for correction | `python/convert_ibit_snapshots.py:191–205, 275–295`; `python/replay_snapshots.py:162–169`; ticket acceptance criterion at line 52. |
| 1 | P2: included zero/negative/non-finite weights are not covered through the Python schema validator. | accepted for correction | `python/validate_schema.py:23–36`; `tests/test_vega_weighting.py:122–171`. |
| 1 | P2: A/B per-snapshot report entries and assembled report metrics lack behavioral assertions. | accepted for correction | `scripts/evaluate_vega_weighting.py:95–131`; ticket acceptance criterion at line 53; `tests/test_vega_weighting.py:237–250`. |
| 1 | P2: executable fit stub emits fixed diagnostics/fits; assertions verify weights forwarded to the stub, not real weighted A/B outcomes. | accepted for correction | `tests/test_vega_weighting.py:191–250`; repository engineering rule forbids tests of wiring/forwarding/mock echoes. |

## Review record

### Round 1 — frozen commit `f9b777c02e91bc43cd213f06c3c24b19f26373b9`

- **Reviewed range:** `fdf70b8b80ce6c9b617141293bda99a560be3e07..f9b777c02e91bc43cd213f06c3c24b19f26373b9`.
- **Frozen snapshot:** `/tmp/cal-c9-review-round1.patch`, SHA-256 `19304d5ebd63b1a9c345998bb8dd4e2fd418506d5a5be3a78d2af82927658943`.

#### Spec review — isolated

The independent spec reviewer found no issues. Owner inspection found one blocking `MISSING` requirement:

### P2 — Converter and replay disagree on locked-quote eligibility

- Evidence: `CAL-C9-REVIEW.md:52` requires converter/replay agreement on included rows and normalized weights. `python/convert_ibit_snapshots.py:191–205` accepts `bid == ask` and `convert_snapshot` includes those rows in weight normalization at lines 275–295. `python/replay_snapshots.py:162–166` drops locked quotes.
- Impact: a locked quote can enter the converter's median and alter weights for other included quotes, while replay omits it.
- Recommendation: exclude locked quotes before converter weight normalization and add a converter/replay regression with a positive locked quote; compare included IDs and weights.

#### Standards review — isolated

### P2 — Locked-quote mismatch changes normalized weights

- Kind: mechanical.
- Evidence: `python/convert_ibit_snapshots.py:191–205, 275–295`; `python/replay_snapshots.py:162–166`.
- Impact: the two preparation paths can produce different included sets and snapshot medians.
- Recommendation: align converter eligibility with replay before normalization; test an otherwise valid out-of-the-money quote with `bid == ask`.
- Gate: extend `tests/test_vega_weighting.py::test_converter_and_replay_emit_identical_vega_weights` to verify both paths exclude the locked quote and agree on included normalized weights.

### P2 — A/B test asserts mocked weight forwarding

- Kind: judgement.
- Evidence: `tests/test_vega_weighting.py:191–250`; the temporary executable logs incoming weights but emits a constant objective and fitted variance derived from ask values, independent of the weighting behavior.
- Impact: the permanent test verifies evaluator-to-process wiring, not that real weighted fitting produces the reported A/B behavior.
- Recommendation: exercise the evaluator with the real `essvi_fit` binary on deterministic synthetic quotes and assert consumer-visible fit/report outcomes, or remove the forwarding-only assertions and retain real-fitter behavioral coverage.

#### Verification review — isolated

- Configured command: `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q`.
- Result: exit 0; Release configure/build passed, CTest 5/5 passed, pytest 33/33 passed.
- False claims: none contradicted. The review did not independently rerun the real-data C5/A-B evaluations.
- **UNTESTED — included-weight schema boundaries.** `python/validate_schema.py:23–36` adds finite-positive validation, but `tests/test_vega_weighting.py:122–171` does not exercise included zero, negative, NaN, or infinity values. Add those validator cases.
- **UNTESTED — per-snapshot A/B report shape.** `scripts/evaluate_vega_weighting.py:95–131` assembles per-snapshot exclusions and full report metrics, while `tests/test_vega_weighting.py:237–250` asserts only dates, fit counts, aggregate quote counts, and forwarded weights. Assert the three per-snapshot entries and key report fields.
- Load-bearing facts were otherwise rung 4 (Ran): expiry/weight parity cases, C++ invalid-weight ingress and legacy defaults, weighted cold/warm calculations and cap gate, and the executable-stub A/B routing.

#### Gate candidates

- Extend the converter/replay parity test with a positive locked quote and assert it is excluded from both included sets and does not affect normalized weights.
- No deterministic gate is proposed for the judgement finding about the mocked A/B test; replace it with real-fitter behavioral coverage.

**Outcome: changes required.**
