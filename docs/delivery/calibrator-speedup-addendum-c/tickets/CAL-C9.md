# CAL-C9 — Make vega-weighted band loss mandatory

## State

- Current: `changes-requested`
- Last transition: `reviewing` → `changes-requested`
- At: `2026-10-17T17:13:17Z`
- Reason/evidence: Round-three spec review found a P2 converter/replay mismatch for same-day or past expiries: their time-to-expiry conventions can change the included quote set and snapshot median before weight normalization. Standards and verification axes approved. The mismatch is reproduced by the parity fixture and accepted for correction.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17T15:54:19Z` | WP-C5 finished; user-approved mandatory vega weighting, IBIT-first scope, and report-only A/B are recorded in the parent implementation spec. |
| `ready` | `baseline-checking` | `2026-10-17T15:54:38Z` | Created the C9 ticket branch at the declared exact base commit. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T15:55:47Z` | Configured full baseline passed: Release configure/build, CTest 5/5, pytest 25/25. |
| `baseline-passed` | `implementing` | `2026-10-17T15:55:47Z` | Started the approved WP-C9 implementation after the exact-base baseline passed. |
| `implementing` | `implemented` | `2026-10-17T16:33:30+00:00` | C9 implementation commit `a2621b4c6fb7c71a040827b810e2af34705a61f2` recorded; acceptance behavior, configured verification, and training-only A/B are complete. |
| `implemented` | `reviewing` | `2026-10-17T16:34:31+00:00` | Final verification passed and the implementation commit is fixed; independent review is starting. |
| `reviewing` | `changes-requested` | `2026-10-17T16:45:59+00:00` | Round 1 review accepted five findings for correction: validate included weights at C++ fit ingress and reject invalid weighted rows; reject excluded-row weights; cover blank current-schema defaults; cover IBIT excluded-row parity; exercise warm local weighted equations. |
| `changes-requested` | `reviewing` | `2026-10-17T16:52:32Z` | Corrections committed as `438d073`; focused regression, configured full verification, and training-only IBIT A/B smoke passed. The updated fixed snapshot is entering round 2 review. |
| `reviewing` | `changes-requested` | `2026-10-17T17:03:22Z` | Round 2 spec and standards axes approved; verification approved with one accepted non-blocking suggestion to exercise the synthetic A/B through a real executable stub rather than monkeypatching `subprocess.run`. |
| `changes-requested` | `reviewing` | `2026-10-17T17:05:49Z` | Test-plan suggestion corrected in `28d7b85`; focused executable-stub test and configured full verification passed. The updated fixed snapshot is entering round 3 review. |
| `reviewing` | `changes-requested` | `2026-10-17T17:13:17Z` | Round-three spec review found a P2 same-day/past-expiry converter/replay eligibility mismatch that changes median-one weights; accepted for correction. Standards and verification axes approved. |

## Claim

`omp coding assistant`

## Parent

[Addendum C implementation spec](../implementation-spec.md); source handoff: [Addendum C](../../../calibrator-speedup-addendum-c.md)

## Base commit

`fdf70b8b80ce6c9b617141293bda99a560be3e07` — finished WP-C5 implementation, review, and final ticket metadata.

## User decisions

- **DEC-002 — mandatory vega-weighted band loss:** weight total-variance band excess by the Black–Scholes price sensitivity, normalized to median 1 per snapshot; no mid-price target.
- **DEC-003 — report-only A/B:** compare unweighted and vega-weighted cold fits on cleaned IBIT data; weighting remains the default regardless of the result.
- IBIT is the only gate. Use the approved combined WP-C5 filter policy and Oct 12–14 training snapshots; do not inspect Oct 15–16 or SPY.

## What to build

Compute normalized per-quote vega sensitivity in the IBIT quote preparation and generic normalized-input paths, pass it through the optional normalized-CSV `weight` column, and use it in the C++ loss and warm-candidate gate. Preserve legacy CSV compatibility (missing weight means 1.0) and the `diagnostics.csv` header. Keep existing unweighted per-quote cap quality metrics visible, and report price-space and normalized vega-weighted quality beside them. Add a deterministic cold-fit A/B evaluator for the combined cleaned IBIT training set.

## Implementation decisions

- For mid-IV `sigma`, time `T`, forward `F`, strike `K`, discount factor `D`, and `d1=(ln(F/K)+sigma^2*T/2)/(sigma*sqrt(T))`, raw price sensitivity per option share is `D*F*phi(d1)/(2*sigma*sqrt(T))`, equivalent to vega divided by `2*sigma*T`. Normalize included-quote sensitivities by their snapshot median to get `weight`; excluded rows carry an empty weight. C5's combined profile defines which quotes participate in the median.
- The objective is `sum((weight * total_variance_excess)^2)`. Apply the same squared weight to cold candidate scores, warm local residual/Jacobian normal equations, and loss scale. The warm per-quote cap gate compares `weight * excess_ratio` with the existing cap; retain the original unweighted excess-ratio fields as separate reported quality metrics.
- Missing or blank weight defaults to 1.0 in C++; the converter emits a finite positive value for every included quote. The normalized CSV validator accepts either the existing 13-column header or that header plus `weight`.
- C++ CSV splitting strips a terminal carriage return from the final field so the appended weight is parsed from standard CRLF output; compatibility tests use the canonical 13- and 14-column headers.
- Price-space error is the absolute total-variance band excess times the raw sensitivity. Report RMS and maximum in cents/ticks (`$0.01` per option share), dollars per share, and dollars per standard 100-share IBIT contract; report RMS and maximum of `weight * total_variance_excess` separately. The source snapshots use regular `IBIT` trading-class symbols and do not carry a multiplier field; cite the standard 100-share option unit in the report and label this assumption.
- Preserve the C1 diagnosis and C5 refit numbers as unweighted historical results. The new A/B runs each usable snapshot twice, using the same C5 combined cleaning policy and inputs; it does not gate adoption.

## Acceptance criteria

- The IBIT converter and replay preparation emit identical, snapshot-median-normalized weights; included weights are finite and positive, excluded rows are blank, and normalized sensitivity has median 1.
- C++ `Quote.weight` has a trailing source-compatible default of 1.0. Both old 13-column normalized CSVs and new weighted CSVs load; explicit weights change the cold objective, and warm gates/objective calculations use the weight.
- `diagnostics.csv` remains exactly six columns. Existing unweighted excess-ratio share-over-cap, RMS, and maximum remain reported unchanged. New analysis reports price-space excess RMS/max in cents, dollars/share, dollars/contract, and normalized vega-weighted excess RMS/max.
- A reproducible A/B refits cleaned IBIT Oct 12–14 under unit weights and normalized vega weights, reports both arms with the above metrics and per-snapshot fit counts, and does not inspect holdout dates or SPY.
- Permanent behavioral tests cover the sensitivity formula/median normalization, converter/replay parity, CSV legacy/new schemas, C++ weight parsing/default/objective behavior, warm weighted gating, and unchanged diagnostics header.
- Configured full verification passes; a fresh independent three-axis review approves the frozen base-to-HEAD snapshot.

## Out of scope

- Mid-centering; SPY; Oct 15–16 holdout; changing the approved C5 cleaning policy; changing `diagnostics.csv`; American pricing before WP-C4; any changes to the live collector.

## Test plan

- Start from exact base `fdf70b8b80ce6c9b617141293bda99a560be3e07` and run the configured full baseline.
- Add behavioral tests first and record Red before implementation.
- Run focused C++ and Python tests; exercise the A/B evaluator on deterministic synthetic snapshots with an executable stub.
- Run the actual evaluator on IBIT Oct 12–14 only, with the C5 combined filter. Record exact inputs, arm counts, and metrics.
- Run the configured full verification, capture the complete base-to-HEAD diff including untracked files in a frozen external review archive, obtain independent spec/standards/verification axes, and finish the ticket only after approval.

## Expected seam

`python/vega_weights.py::raw_price_sensitivity` and the snapshot normalizers are shared by `convert_ibit_snapshots.convert_snapshot`, `replay_snapshots.prepare_snapshot`, and `prepare_inputs.main`. C++ `Quote.weight`, `readQuotes`, `bandLoss`, and the warm local objective/gate consume the column; `scripts/evaluate_vega_weighting.py` exercises the cold-fit A/B.

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|
| 2026-10-17T15:55:47Z | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `fdf70b8b80ce6c9b617141293bda99a560be3e07` | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 25/25 passed. |
| 2026-10-17T16:18:45Z | Red | `.venv/bin/python -m pytest -q tests/test_pipeline.py::test_prepare_inputs_emits_median_normalized_weights` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | Converter output lacked `weight`; assertion raised `KeyError: 'weight'`. |
| 2026-10-17T16:19:33+00:00 | Green | `.venv/bin/python -m pytest -q tests/test_pipeline.py::test_prepare_inputs_emits_median_normalized_weights` | `/home/zhangy/git/ql-essvi` | working tree | passed | The generic converter now emits finite positive weights for included quotes, median 1, and blank weights for excluded quotes; 1 passed. |
| 2026-10-17T16:28:37Z | Red | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_calibrator_regression_tests -j2 && ctest --test-dir build --output-on-failure -R essvi_calibrator_regression_tests` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | CRLF weighted CSV left `weight` unread; regression asserted `weighted_quotes[i].weight == 2.0` and failed. |
| 2026-10-17T16:30:58Z | Green | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_calibrator_regression_tests -j2 && ctest --test-dir build --output-on-failure -R essvi_calibrator_regression_tests` | `/home/zhangy/git/ql-essvi` | working tree | passed | After stripping final-field CR, full 13-column legacy and 14-column weighted CRLF fixtures pass; CTest 1/1. |
| 2026-10-17T16:30:58Z | A/B | `.venv/bin/python scripts/evaluate_vega_weighting.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c9-evaluation.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | Training-only IBIT: both arms fit 212/212 snapshots and 90,954 quotes; summed objective 0.3268896906 unit vs 0.1346668819 vega-weighted. Holdout and SPY were not read. |
| 2026-10-17T16:32:33Z | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |
| 2026-10-17T16:45:59Z | Review round 1 | Frozen base-to-HEAD `fdf70b8b80ce6c9b617141293bda99a560be3e07..a2621b4c6fb7c71a040827b810e2af34705a61f2`; archive `/tmp/cal-c9-a262-review-snapshot.tar.gz` (SHA-256 `56e6d17b54bd350aad81c5d780f9e3949da91f2b5c96cfffdcaa372d5f6f8546`), patch SHA-256 `6a70e7c6156916cea510ea245036cb50472d880699d7301f9ed94861b793d8e9` | `/home/zhangy/git/ql-essvi` | `a2621b4c6fb7c71a040827b810e2af34705a61f2` | changes requested | Spec axis approved; standards axis required changes; verification axis approved with suggestions. The reviewer ran configured Release build, CTest 5/5, and pytest 33/33 successfully on the frozen snapshot; behavior gaps are listed in the review record. |
| 2026-10-17T16:48:39Z | Review-gap Red | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_calibrator_regression_tests -j2 && ctest --test-dir build --output-on-failure -R essvi_calibrator_regression_tests` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | New invalid-weight CSV regression failed because `readQuotes` silently omitted zero-weight rows; parser gap reproduced. |
| 2026-10-17T16:48:39Z | Review-gap Red | `.venv/bin/python -m pytest -q tests/test_vega_weighting.py::test_converter_and_replay_emit_identical_vega_weights tests/test_vega_weighting.py::test_normalized_schema_accepts_legacy_and_weighted_files` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | IBIT converter/replay excluded-row parity passed; schema regression failed because validator accepted a nonblank excluded-row weight. |
| 2026-10-17T16:49:16Z | Review-gap Green | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_calibrator_regression_tests -j2 && ctest --test-dir build --output-on-failure -R essvi_calibrator_regression_tests` | `/home/zhangy/git/ql-essvi` | working tree | passed | C++ regression passed direct cold/warm rejection for zero, negative, and NaN included weights; explicit zero CSV weight retained and rejected; blank 14-column weight defaults to 1; weighted local warm fit converges and changes the fitted slice under nonuniform weight. CTest 1/1. |
| 2026-10-17T16:49:16Z | Review-gap Green | `.venv/bin/python -m pytest -q tests/test_vega_weighting.py::test_converter_and_replay_emit_identical_vega_weights tests/test_vega_weighting.py::test_normalized_schema_accepts_legacy_and_weighted_files` | `/home/zhangy/git/ql-essvi` | working tree | passed | IBIT converter/replay agree for included and excluded quote rows; excluded weight is empty. Normalized schema rejects nonblank weight on excluded rows; focused tests 2/2. |
| 2026-10-17T16:52:08Z | Review-gap Green extension | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_calibrator_regression_tests -j2 && ctest --test-dir build --output-on-failure -R essvi_calibrator_regression_tests` | `/home/zhangy/git/ql-essvi` | working tree | passed | Added malformed textual weight CSV coverage; the row is retained and fit ingress rejects it with the finite-positive error. CTest 1/1. |
| 2026-10-17T16:52:08Z | Final correction verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |
| 2026-10-17T16:52:08Z | A/B correction smoke | `.venv/bin/python scripts/evaluate_vega_weighting.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c9-review-fix-evaluation.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | Training-only IBIT Oct 12–14: 212/212 fits per arm, 90,954 quotes. Summed objective 0.3268896906491981 unit versus 0.13466688189465315 vega-weighted; no holdout dates or SPY used. Existing report-only comparison remains unchanged. |
| 2026-10-17T16:52:32Z | Round 1 correction commit | `git commit -m "Validate C9 vega weights at fit ingress"` | `/home/zhangy/git/ql-essvi` | `438d073` | committed | Corrected included-weight ingress/parser handling, normalized-schema exclusion semantics, and all five review coverage gaps. |
| 2026-10-17T17:03:22Z | Review round 2 | Frozen base-to-HEAD `fdf70b8b80ce6c9b617141293bda99a560be3e07..8f69b30862cc2ca4ede3e444a25fdf05127ad7d4`; archive `/tmp/cal-c9-8f69b30-review-snapshot.tar.gz` (SHA-256 `231b118e0de03091b68a1321f23bc33ea2a4895c1367d04808ca77f6d737448b`), patch SHA-256 `bb22d5bdabf9ac9b0d6ce987f55999ed0ecce5cd80e8b40d93f3909e4054478a` | `/home/zhangy/git/ql-essvi` | `8f69b30862cc2ca4ede3e444a25fdf05127ad7d4` | approved with suggestion | Spec axis approved; standards axis approved with no findings; verification axis approved with one non-blocking suggestion. Independent verification ran configured Release configure/build, CTest 5/5, pytest 33/33. No real holdout or SPY source files were opened. |
| 2026-10-17T17:05:24Z | Executable-stub Green | `.venv/bin/python -m pytest -q tests/test_vega_weighting.py::test_vega_ab_runs_unit_and_weighted_cold_arms_on_training_only` | `/home/zhangy/git/ql-essvi` | working tree | passed | The deterministic three-date synthetic A/B now invokes a temporary executable fit stub for all six cold fits; the stub emits diagnostics and fitted quote rows and records unit versus normalized-vega inputs. 1 passed. |
| 2026-10-17T17:05:24Z | Final round-two correction verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |
| 2026-10-17T17:05:49Z | Round 2 correction commit | `git commit -m "Exercise vega A/B with executable fit stub"` | `/home/zhangy/git/ql-essvi` | `28d7b85` | committed | Replaced the monkeypatched subprocess with an executable temporary fit stub matching the accepted test-plan suggestion. |
| 2026-10-17T17:13:17Z | Review round 3 | Frozen base-to-HEAD `fdf70b8b80ce6c9b617141293bda99a560be3e07..c32f76248a5d9ebebbb376dde6921c74f8685e1e`; archive `/tmp/cal-c9-c32f762-review-snapshot.tar.gz` (SHA-256 `bab83000fa3420d70081b2975ae0074bed44b67f84d4178288f7e853a2eaf48d`), patch SHA-256 `5bd79a71aafaed0b4beed886836de63d5afa6316fe6e3b7b48d8b5e639e4baee` | `/home/zhangy/git/ql-essvi` | `c32f76248a5d9ebebbb376dde6921c74f8685e1e` | changes requested | Spec axis found P2 same-day/past-expiry eligibility mismatch; standards axis approved; verification axis approved after Release configure/build, CTest 5/5, pytest 33/33. No holdout or SPY source data was read. |
| 2026-10-17T17:13:54Z | Expiry-parity Red | `.venv/bin/python -m pytest -q tests/test_vega_weighting.py::test_converter_and_replay_emit_identical_vega_weights` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | Same-day and past-expiry fixtures exposed different included sets: converter retained 3 quotes while replay retained 5, before comparing the resulting normalized weights. |
| 2026-10-17T17:15:53Z | Expiry/timezone-parity Red | `.venv/bin/python -m pytest -q tests/test_vega_weighting.py::test_converter_and_replay_emit_identical_vega_weights` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | A snapshot at 2026-10-13 00:15 UTC (2026-10-12 in New York) with same-day, past, and next-session expiries produced 3 converter-included versus 6 replay-included quotes. |
| 2026-10-17T17:16:52Z | Expiry/timezone-parity Green | `.venv/bin/python -m pytest -q tests/test_vega_weighting.py::test_converter_and_replay_emit_identical_vega_weights tests/test_quote_cleaning.py::test_converter_and_replay_share_cleaning_reasons_and_counts tests/test_snapshot_replay.py::test_strict_preparation_excludes_locked_crossed_and_other_quotes tests/test_snapshot_replay.py::test_iv_changes_match_contract_identity_not_source_row_order` | `/home/zhangy/git/ql-essvi` | working tree | passed | Converter and replay now share the New York session date and raw calendar-day year fraction. Cross-UTC-midnight fixture verifies same-day/past exclusions, next-session inclusion, equal date/time/inclusion/weight fields, and median weights; 4 focused tests passed. |
| 2026-10-17T17:32:42Z | Final correction verification | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |
| 2026-10-17T17:32:42Z | Corrected C5/A/B evidence recorded | `.venv/bin/python scripts/evaluate_quote_cleaning.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit` and `.venv/bin/python scripts/evaluate_vega_weighting.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit` | `/home/zhangy/git/ql-essvi` | working tree | passed | Updated training-only outputs: combined C5 90,700 included / 22,548 cleaning exclusions; A/B 212/212 fits per arm, same 90,700 quotes, summed objective 0.3268406267822395 unit versus 0.1334024602632656 weighted. Oct 15–16 and SPY were not read. |

## Evidence and decision references

- Source: Addendum C §5 WP-C9 and §7 DEC-002/DEC-003.
- Approved integration decisions: `docs/delivery/calibrator-speedup-addendum-c/implementation-spec.md` DEC-002, DEC-003, and WP-C9 interpretation.
- Standard equity-option unit: OCC Equity Options Product Specifications, https://www.theocc.com/clearance-and-settlement/clearing/equity-options-product-specifications.

## Review depth

`standard`

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |
| 1 | P2: validate finite-positive weights for included `Quote`s at both C++ fit ingress paths; malformed explicit weighted CSV values must not be silently omitted. | accepted | Finding verified against `cpp/calibrator.cpp` parser and fit ingress; correction evidence will be appended before round 2. |
| 1 | P3: reject nonblank weight values on `included=false` rows in the normalized CSV validator. | accepted | Finding verified against `python/validate_schema.py`; correction evidence will be appended before round 2. |
| 1 | P3: add coverage that blank weight in a current 14-column CSV defaults to 1.0. | accepted | Missing coverage verified in the C++ CRLF fixtures; correction evidence will be appended before round 2. |
| 1 | P3: cover excluded-row blank weight parity in the IBIT converter and replay paths. | accepted | Existing converter/replay parity fixture contains included rows only; correction evidence will be appended before round 2. |
| 1 | P3: test the weighted warm local objective/Jacobian path when the cap gate does not force fallback. | accepted | Existing warm test covers cap fallback only; correction evidence will be appended before round 2. |
| 2 | Suggestion: run the deterministic synthetic A/B evaluator test through an executable fit stub rather than monkeypatching `subprocess.run`, matching the ticket test plan. | accepted | Verified in `tests/test_vega_weighting.py::test_vega_ab_runs_unit_and_weighted_cold_arms_on_training_only` at the frozen round-two snapshot; correction evidence will be appended before round 3. |
| 3 | P2: align converter and replay expiry eligibility before snapshot weight normalization so same-day and expired contracts cannot produce different included sets or medians. | accepted | Correction is implemented and verified as recorded under `Correction evidence — round 3`; no fresh independent review was run after the round-three snapshot. |

## Correction evidence — round 1

Correction commit: `438d073` (`Validate C9 vega weights at fit ingress`).

| Finding | Correction evidence |
| --- | --- |
| P2 included weight validation and invalid CSV retention | `cpp/calibrator.cpp::hasInvalidIncludedWeight` rejects nonfinite/nonpositive included weights from both `calibrate` and `calibrateWarm`. `readQuotes` retains zero and malformed textual weights as invalid values instead of omitting rows. `essvi_calibrator_regression_tests` passed on 2026-10-17T16:52:08Z; covers direct zero/negative/NaN cold and warm inputs, zero and malformed CSV weights, and fit-ingress error. |
| P3 excluded-row schema weight | `python/validate_schema.py::validate` now rejects nonempty weight on `included=false`. `test_normalized_schema_accepts_legacy_and_weighted_files` passed with the excluded-row rejection regression on 2026-10-17T16:49:16Z. |
| P3 blank 14-column default | `legacyAndWeightedCsvControlColdObjective` verifies blank `weight` on the current 14-column header loads as 1.0; `essvi_calibrator_regression_tests` passed on 2026-10-17T16:52:08Z. |
| P3 IBIT excluded-row parity | `test_converter_and_replay_emit_identical_vega_weights` now includes an excluded ATM call and proves both paths emit matching rows with blank excluded weight; passed on 2026-10-17T16:49:16Z. |
| P3 weighted warm local equations | `weightedWarmLocalFitUsesWeightedEquations` forces the warm local solver with cap gate passing, checks objective against the weighted band-loss formula, and observes a parameter change under nonuniform weight; `essvi_calibrator_regression_tests` passed on 2026-10-17T16:52:08Z. |

Configured correction verification passed on 2026-10-17T16:52:08Z: Release configure/build, CTest 5/5, pytest 33/33. The training-only IBIT A/B reran on the same scope; 212/212 snapshots per arm and 90,954 quotes; unit summed objective `0.3268896906491981`, weighted `0.13466688189465315`.

## Correction evidence — round 2

Correction commit: `28d7b85` (`Exercise vega A/B with executable fit stub`).

- `tests/test_vega_weighting.py::test_vega_ab_runs_unit_and_weighted_cold_arms_on_training_only` writes an executable temporary fit stub, invokes the actual evaluator subprocess boundary six times on three deterministic synthetic IBIT dates, emits the diagnostics/fitted CSV artifacts the analyzer consumes, and checks unit versus normalized-vega weights. Focused test passed 1/1 at 2026-10-17T17:05:24Z.
- Configured verification passed at 2026-10-17T17:05:24Z: Release configure/build, CTest 5/5, pytest 33/33.

## Review record

### Round 3 — frozen commit `c32f76248a5d9ebebbb376dde6921c74f8685e1e`

- **Spec axis — changes required.** P2: replay floors same-day/past expiry to one day while converter uses the raw calendar-day difference; this can include a quote only in replay and shift its snapshot median weight.
- **Standards axis — approved, no findings.** Reviewed the fixed round-three patch including the executable stub; no patch-introduced engineering defects or test lifecycle issues found.
- **Verification axis — approved.** Configured Release configure/build, CTest 5/5, and pytest 33/33 passed on the fixed snapshot. No additional untested C9 acceptance behavior identified; holdout and SPY source files were not read.

### Round 2 — frozen commit `8f69b30862cc2ca4ede3e444a25fdf05127ad7d4`

- **Spec axis — approved, no findings.** Confirmed the loss, compatibility, metrics, and report-only evaluator remain within the approved C9 scope; no holdout or SPY source data was used.
- **Standards axis — approved, no findings.** Round-one ingress and schema corrections are correct; no additional bug or maintainability issue found.
- **Verification axis — approved with one non-blocking suggestion.** Configured Release configure/build, CTest 5/5, and pytest 33/33 passed on the frozen snapshot. The prior round-one behavior gaps are covered. Accepted suggestion: replace the synthetic A/B test's monkeypatched `subprocess.run` with an executable fit stub, as specified in the ticket test plan.

### Round 1 — frozen commit `a2621b4c6fb7c71a040827b810e2af34705a61f2`

- **Spec axis — approved, no findings.** Confirmed mandatory normalized vega weighting, cold/warm objective and gate scope, legacy compatibility, metrics, and IBIT Oct 12–14 training-only A/B under C5 cleaning; no holdout dates or SPY.
- **Standards axis — changes required.** P2: direct `Quote` API accepts zero, negative, or nonfinite included weights; warm cap computation can then be invalid or bypassed. Weighted CSV parse silently skips invalid weight rows. P3: schema validator accepts a nonblank weight on excluded rows.
- **Verification axis — approved with suggestions.** Frozen-snapshot configured verification passed (Release configure/build, CTest 5/5, pytest 33/33). Missing behavioral coverage: current 14-column blank-weight default, warm local weighted equations, IBIT converter/replay excluded-row parity.

### Gate candidates

- C++ CTest: invalid direct included weights rejected in cold and warm APIs; invalid explicit weighted CSV rows are retained as invalid and rejected at fit ingress; blank 14-column weights default to 1.0; warm local weighted equations are observable with the cap gate passing.
- Python pytest: excluded rows with nonblank weights fail schema validation; IBIT converter and replay agree on excluded-row blank weights and included quote weights.
- Accepted round-two correction: execute the synthetic A/B evaluator against a temporary executable fit stub; verify it receives unit and normalized-vega inputs and returns valid fit artifacts.
- Accepted round-three correction: align expired/same-day quote eligibility in converter and replay, then prove identical included/excluded rows and median-one weights with same-day and past-expiry fixtures.

## Correction evidence — round 3

- `python/convert_ibit_snapshots.py` defines shared `trading_session_date` and `date_based_year_fraction` helpers. Converter and replay both use the New York session date and raw calendar-day difference divided by 365; same-day and past-expiry contracts are excluded before normalization, matching Addendum C §3's stated same-day exclusion. `docs/csv-contract.md` records the shared contract.
- `tests/test_vega_weighting.py::test_converter_and_replay_emit_identical_vega_weights` uses a timestamp at 2026-10-13 00:15 UTC (2026-10-12 in New York), verifies converter/replay date, time, inclusion and weights row-by-row, excludes same-day/past expiries, and includes the next-session expiry. The parity test plus three related converter/replay behavior tests passed 4/4 at 2026-10-17T17:16:52Z.
- Configured Release verification passed at 2026-10-17T17:32:42Z: CTest 5/5 and pytest 33/33.
- Corrected training-only IBIT C5 refits completed all 212 snapshots per profile. The combined profile has 90,700 included quotes, 22,548 unique cleaning exclusions, 22,450 minimum-premium exclusions, 3,253 relative-spread exclusions, and 11,212 delta-window exclusions. Updated C5 results are in `docs/fit-quality.md`.
- Corrected report-only A/B completed 212/212 fits per arm on 90,700 included quotes. The unit arm's summed objective is `0.3268406267822395`; the vega-weighted arm's is `0.1334024602632656`. Updated cap, vega-error, price-space, and coverage metrics are in `docs/fit-quality.md`. Only IBIT Oct 12–14 was read; Oct 15–16 and SPY were not read.
- The correction is not independently reviewed: the third review round already ended with a blocking spec finding. No fourth review is recorded.
