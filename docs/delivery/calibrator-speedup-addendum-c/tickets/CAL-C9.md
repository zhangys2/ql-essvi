# CAL-C9 — Make vega-weighted band loss mandatory

## State

- Current: `implementing`
- Last transition: `baseline-passed` → `implementing`
- At: `2026-10-17T15:55:47Z`
- Reason/evidence: Exact-base Release build, CTest 5/5, and pytest 25/25 passed; implementation may proceed.
- Blocker: None.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17T15:54:19Z` | WP-C5 finished; user-approved mandatory vega weighting, IBIT-first scope, and report-only A/B are recorded in the parent implementation spec. |
| `ready` | `baseline-checking` | `2026-10-17T15:54:38Z` | Created the C9 ticket branch at the declared exact base commit. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T15:55:47Z` | Configured full baseline passed: Release configure/build, CTest 5/5, pytest 25/25. |
| `baseline-passed` | `implementing` | `2026-10-17T15:55:47Z` | Started the approved WP-C9 implementation after the exact-base baseline passed. |

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

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|
| 2026-10-17T15:55:47Z | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `fdf70b8b80ce6c9b617141293bda99a560be3e07` | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 25/25 passed. |
| 2026-10-17T16:18:45Z | Red | `.venv/bin/python -m pytest -q tests/test_pipeline.py::test_prepare_inputs_emits_median_normalized_weights` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | Converter output lacked `weight`; assertion raised `KeyError: 'weight'`. |
| 2026-10-17T16:19:33+00:00 | Green | `.venv/bin/python -m pytest -q tests/test_pipeline.py::test_prepare_inputs_emits_median_normalized_weights` | `/home/zhangy/git/ql-essvi` | working tree | passed | The generic converter now emits finite positive weights for included quotes, median 1, and blank weights for excluded quotes; 1 passed. |
| 2026-10-17T16:28:37+00:00 | Red | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_calibrator_regression_tests -j2 && ctest --test-dir build --output-on-failure -R essvi_calibrator_regression_tests` | `/home/zhangy/git/ql-essvi` | working tree | expected failure | CRLF weighted CSV left `weight` unread; regression asserted `weighted_quotes[i].weight == 2.0` and failed. |
| 2026-10-17T16:30:58+00:00 | Green | `export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_calibrator_regression_tests -j2 && ctest --test-dir build --output-on-failure -R essvi_calibrator_regression_tests` | `/home/zhangy/git/ql-essvi` | working tree | passed | After stripping final-field CR, full 13-column legacy and 14-column weighted CRLF fixtures pass; CTest 1/1. |
| 2026-10-17T16:30:58+00:00 | A/B | `.venv/bin/python scripts/evaluate_vega_weighting.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c9-evaluation.json` | `/home/zhangy/git/ql-essvi` | working tree | passed | Training-only IBIT: both arms fit 212/212 snapshots and 90,954 quotes; summed objective 0.3268896906 unit vs 0.1346668819 vega-weighted. Holdout and SPY were not read. |
| 2026-10-17T16:32:33+00:00 | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | working tree | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 33/33 passed. |

## Evidence and decision references

- Source: Addendum C §5 WP-C9 and §7 DEC-002/DEC-003.
- Approved integration decisions: `docs/delivery/calibrator-speedup-addendum-c/implementation-spec.md` DEC-002, DEC-003, and WP-C9 interpretation.
- Standard equity-option unit: OCC Equity Options Product Specifications, https://www.theocc.com/clearance-and-settlement/clearing/equity-options-product-specifications.

## Review depth

`standard`
