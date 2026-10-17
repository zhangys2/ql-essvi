# CAL-C9-REVIEW — Re-accept the complete vega-weighted C9 deliverable

## State

- Current: `reviewing`
- Last transition: `implemented` → `reviewing`
- At: `2026-10-17T20:20:12Z`
- Reason/evidence: The accepted round-two P3 coverage gap is closed in commit `e7c71220a9a6c10fb36343d1f8d71a3fc485dd7e`. The focused C++ regression and configured full suite pass; the corrected full base-to-HEAD range is entering round-three review.
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
| `implementing` | `implemented` | `2026-10-17T18:32:30Z` | Corrections committed as `8c5a6983048dde8bf9d917e0bbb54a08b13208cf`; the training-only C5/A-B reruns, focused vega suite, and configured full verification passed. Round 2 review is next. |
| `implemented` | `reviewing` | `2026-10-17T18:35:19Z` | Final evidence is recorded and the corrected base-to-HEAD range is being frozen for independent round-two review. |
| `reviewing` | `changes-requested` | `2026-10-17T18:50:05Z` | Round 2 spec and standards approved; verification passed with one accepted P3 C++ partial-number parsing coverage gap. Owner will close the gap before handoff. |
| `changes-requested` | `implementing` | `2026-10-17T20:18:50Z` | Added a `2junk` included-weight CSV fixture and rejection assertion; the focused C++ calibrator regression passed 1/1. Full verification remained pending. |
| `implementing` | `implemented` | `2026-10-17T20:19:49Z` | Regression committed as `e7c71220a9a6c10fb36343d1f8d71a3fc485dd7e`; configured Release build, CTest 5/5, and pytest 33/33 passed. |
| `implemented` | `reviewing` | `2026-10-17T20:20:12Z` | Final round-two correction evidence is recorded; the corrected base-to-HEAD range is entering the third and final review round. |

## Claim

`omp coding assistant`

## Parent and predecessor records

- Re-accepts the complete C9 deliverable from [CAL-C9](CAL-C9.md); the original ticket remains blocked as an append-only historical record.
- [Addendum C implementation spec](../implementation-spec.md); source handoff: [Addendum C](../../../calibrator-speedup-addendum-c.md).
- Follows the review-only fresh-ticket pattern in [CAL-C1-REVIEW](CAL-C1-REVIEW.md).

## Review base and fixed deliverable

- Review base: `fdf70b8b80ce6c9b617141293bda99a560be3e07` (the exact C9 implementation base after WP-C5).
- Original fixed C9 deliverable: `1dac3aa1ca3239397f4b5eeee8ceab6cb1ab0cd5`, including the expiry-parity correction `43b3fca89339d2385415bb0845fe1716a6833a4e` and the blocked-state record for the original ticket.
- Round 1 reviewed the full base-to-HEAD diff ending at `f9b777c02e91bc43cd213f06c3c24b19f26373b9`. Round 2 reviewed the corrected range ending at `ade0d6e43f029879524243b53ea0dd84751e03ce`. The next review keeps the same base and adds only the accepted parser-boundary test and ticket evidence.

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

- Production behavior beyond corrections required to satisfy accepted review findings; changing C5 thresholds, changing `diagnostics.csv`, mid-centering, SPY, Oct 15–16, live collector changes, or advancing to WP-C2.
- Reopening the original CAL-C9 history or rewriting its prior review findings.

## Test plan

The original C9 implementation's red/green evidence remains in CAL-C9. Round-one corrections add focused locked-quote parity, invalid schema-weight boundaries, and real-fitter A/B report coverage. Round-two adds a malformed numeric-prefix CSV regression.

- Run the configured full verification command from `docs/delivery/DELIVERY.md` on the corrected branch.
- Run the corrected C5 refit and report-only vega A/B on IBIT Oct 12–14 only; compare generated coverage and metrics with `docs/fit-quality.md`.
- Run focused converter/replay expiry- and locked-quote parity coverage and the C++ calibrator regression test.
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
| 2026-10-17T18:26:54Z | Corrected report-only A/B | `.venv/bin/python scripts/evaluate_vega_weighting.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c9-review-ab-round1-fix.json` | `/home/zhangy/git/ql-essvi` | `8c5a6983048dde8bf9d917e0bbb54a08b13208cf` | passed | Both arms completed 212/212 fits on 90,700 combined-profile quotes. Unit objective `0.3268406267822395`; vega objective `0.1334024602632656`. The report lists only IBIT Oct 12–14 files. |
| 2026-10-17T18:28:34Z | Corrected C5 training rerun | `.venv/bin/python scripts/evaluate_quote_cleaning.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c9-review-c5-round1-fix.json` | `/home/zhangy/git/ql-essvi` | `8c5a6983048dde8bf9d917e0bbb54a08b13208cf` | passed | All five profiles completed 212/212 on Oct 12–14. Combined: 90,700 included; 22,548 cleaning exclusions; above-cap share 69.10%, RMS ratio 2.1742, max 64.1403. Metrics match the checked-in report. |
| 2026-10-17T18:31:34Z | Final round-one correction verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `8c5a6983048dde8bf9d917e0bbb54a08b13208cf` | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |
| 2026-10-17T18:31:59Z | Focused round-one regression suite | `.venv/bin/python -m pytest -q tests/test_vega_weighting.py` | `/home/zhangy/git/ql-essvi` | `8c5a6983048dde8bf9d917e0bbb54a08b13208cf` | passed | 5 passed, including locked quote converter/replay parity, invalid included schema weights, and real-binary report-only A/B with per-snapshot coverage and objective/quality consistency. |
| 2026-10-17T18:50:05Z | Review round 2 | Frozen base-to-HEAD snapshot `fdf70b8b80ce6c9b617141293bda99a560be3e07..ade0d6e43f029879524243b53ea0dd84751e03ce`; SHA-256 `36dc45bbf4e9b8a87e9c7933a8bbbca26b41599786fc66d40e5caf659c64a631` | `/home/zhangy/git/ql-essvi` | `ade0d6e43f029879524243b53ea0dd84751e03ce` | changes requested | Spec and standards axes approved. Configured verification passed (CTest 5/5, pytest 33/33) and identified one P3 untested partial-numeric-weight rejection branch; accepted for correction. |
| 2026-10-17T20:18:50Z | Focused C++ CSV regression | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_calibrator_regression_tests -j2 && ctest --test-dir build -R essvi_calibrator_regression_tests --output-on-failure` | `/home/zhangy/git/ql-essvi` | working tree | passed | `essvi_calibrator_regression_tests` rebuilt; CTest 1/1 passed, including rejection of an included weight `2junk`. |
| 2026-10-17T20:19:49Z | Final round-two correction verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `e7c71220a9a6c10fb36343d1f8d71a3fc485dd7e` | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |

## Review dispositions

| Round | Finding | Disposition | Evidence |
|---|---|---|---|
| 1 | P2: converter includes locked quotes that replay excludes, shifting median-normalized weights. | accepted for correction | Corrected by `8c5a6983048dde8bf9d917e0bbb54a08b13208cf`; converter predicate now excludes equality before weighting, with a locked-quote parity fixture. |
| 1 | P2: included zero/negative/non-finite weights are not covered through the Python schema validator. | accepted for correction | Added included blank, zero, negative, NaN, and ±infinity cases in `tests/test_vega_weighting.py`; focused suite passed. |
| 1 | P2: A/B per-snapshot report entries and assembled report metrics lack behavioral assertions. | accepted for correction | Real-binary integration test now asserts all three per-snapshot entries and fit-objective/quality consistency; focused suite passed. |
| 1 | P2: executable fit stub emits fixed diagnostics/fits; assertions verify weights forwarded to the stub, not real weighted A/B outcomes. | accepted for correction | Replaced the stub with `build/essvi_fit`; test verifies distinct real fit objectives and matching objective/quality aggregates. |
| 2 | P3: C++ CSV parser numeric-prefix rejection (`2junk`) lacks direct regression coverage. | accepted for correction | Added `partial-numeric-weight.csv` with `2junk`; calibration rejects it with the included-weight error. Focused CTest and final configured suite pass. |

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
- **UNTESTED — included-weight schema boundaries.** `python/validate_schema.py:23–36` adds finite-positive validation, but the round-one snapshot's tests did not exercise included zero, negative, NaN, or infinity values.
- **UNTESTED — per-snapshot A/B report shape.** The round-one snapshot's A/B test asserted only dates, fit counts, aggregate quote counts, and forwarded weights, not the per-snapshot entries or objective/quality aggregates.
- Load-bearing facts were otherwise rung 4 (Ran): expiry/weight parity cases, C++ invalid-weight ingress and legacy defaults, weighted cold/warm calculations and cap gate, and the executable-stub A/B routing.

#### Gate candidates

- Extend the converter/replay parity test with a positive locked quote and assert it is excluded from both included sets and does not affect normalized weights.
- No deterministic gate is proposed for the judgement finding about the mocked A/B test; replace it with real-fitter behavioral coverage.

**Outcome: changes required.**

### Round 2 — frozen commit `ade0d6e43f029879524243b53ea0dd84751e03ce`

- **Reviewed range:** `fdf70b8b80ce6c9b617141293bda99a560be3e07..ade0d6e43f029879524243b53ea0dd84751e03ce`.
- **Frozen snapshot:** `/tmp/cal-c9-review-round2.patch`, SHA-256 `36dc45bbf4e9b8a87e9c7933a8bbbca26b41599786fc66d40e5caf659c64a631`.

#### Spec review — isolated

No spec findings. The round-one locked-quote eligibility finding is corrected; the converter rejects `ask <= bid` before weight normalization and the parity test checks inclusion/weight agreement. No Oct 15–16 or SPY source data was inspected.

#### Standards review — isolated

No standards findings. No weakened checks found. Gate candidates: none.

#### Verification review — isolated

- Configured full-suite command run exactly from `docs/delivery/DELIVERY.md:14–15`; exit 0, Release configure/build succeeded, CTest 5/5 and pytest 33/33 passed (29.13 seconds).
- Prior round-one schema/A-B coverage gaps are corrected and pass. No real holdout or SPY source data was accessed. Real-data C5/A-B results are recorded but were not independently rerun in this review.
- Load-bearing facts were rung 4 (Ran): legacy/default and invalid-weight handling, weighted cold/warm behavior and gate, converter/replay expiry and locked-quote parity, and synthetic real-fitter A/B report routing.
- **UNTESTED — P3 parser boundary.** `cpp/calibrator.cpp:942–945` rejects numeric prefixes with trailing junk after `std::stod`; `tests/test_calibrator_regression.cpp:107–140` covered a nonnumeric token but not a numeric prefix such as `2junk`. Owner accepted this suggestion for correction because permanent test policy requires error-boundary coverage.

**Independent round-two verdict:** spec approved; standards approved; verification passed with one non-blocking P3 suggestion. **Ticket outcome: changes requested** to close the accepted P3 gap before handoff.

### Round-one correction evidence

- Correction commit: `8c5a6983048dde8bf9d917e0bbb54a08b13208cf`.
- Converter now rejects `ask <= bid` before computing included-row weights; `docs/csv-contract.md` records locked/crossed quote exclusion. The parity test verifies locked quotes are excluded and do not change normalized weights.
- Python schema tests reject included blank, zero, negative, NaN, and positive/negative infinity weights.
- The A/B evaluator test now invokes the real `build/essvi_fit` binary on three deterministic synthetic training snapshots; it checks per-snapshot counts, both report objectives against their corresponding quality aggregates, and distinct unit/weighted fit objectives.
- Focused C9 suite: 5 passed. Configured Release verification: CTest 5/5, pytest 33/33. Corrected C5 and report-only A/B training reruns match the checked-in metrics; only Oct 12–14 IBIT files were read.

### Round-two correction evidence

- Test commit: `e7c71220a9a6c10fb36343d1f8d71a3fc485dd7e`.
- `tests/test_calibrator_regression.cpp` writes an included weight `2junk`, reads the CSV, and asserts calibration rejects it with `included quote weight must be finite and positive`; the existing nonnumeric `invalid` conversion-exception case remains covered. Focused CTest passed 1/1. The final configured Release build passed; CTest 5/5 and pytest 33/33 passed.
