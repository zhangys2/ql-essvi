# ql-essvi fit quality on real data — Addendum C implementation

- Intake: decision-only
- Base commit: `ef6c788b8e2479b9aa74a1b595fc9844d9094082`
- Source handoff: `docs/calibrator-speedup-addendum-c.md`
- Approval: the user instructed “copy the file to this repo and implement” after confirming mandatory vega weighting, IBIT-first scope, vega error aggregation, WP-C7 approval, and report-only A/B. Treat that instruction as approval of this implementation scope. User approval remains required for the WP-C5 cleaning thresholds after WP-C1.

## Source handoff

Read first: `docs/delivery/DELIVERY.md`, `README.md`, `docs/parameterization.md`, `docs/csv-contract.md`, `docs/calibrator-speedup-addendum-b.md`, this addendum (`docs/calibrator-speedup-addendum-c.md`), `docs/perf-results.md`, `python/convert_ibit_snapshots.py`, `python/replay_snapshots.py`, `python/run_snapshot_replay.py`, `cpp/calibrator.cpp`, `cpp/calibrator.hpp`. Build, in this order and on IBIT first: (1) WP-C1 read-only diagnosis of the cold-fit misses on the IBIT replay by expiry, moneyness, premium, relative band width and fetch-order position, proposing cleaning thresholds from the data; (2) WP-C5 counted cleaning filters (minimum bid, minimum premium, maximum relative spread, strike window) applied identically everywhere, with each rule's exclusion counts and effect reported; (3) WP-C9 the vega-weighted band loss (weight = Black-Scholes price sensitivity to total variance, normalized per snapshot, passed as an optional `weight` CSV column) with side-by-side unweighted and price-space metrics and an A/B on cleaned data; (4) WP-C2 forward from put-call parity; (5) WP-C3 synchronous quote sets (analysis first; any change to the live collector needs separate approval); (6) WP-C4 American inversion; (7) WP-C6 model-capacity check by removing each constraint in turn; (8) WP-C7 re-base the quality criteria on the cleaned IBIT result and re-report once under the split-day protocol (tune on 2026-10-12 to 14, evaluate once on 2026-10-15 and 16); (9) WP-C8 make the warm path opt-in and resolve the cold-path A/B by building `49fed2b` from `git archive` and timing interleaved pairs. Decisions already made by Yuan: cold refit is the production path for 5-minute updates and warm-path latency and fallback criteria are no longer blocking; data cleaning was missed and is the main fix; IBIT first, SPY deferred; vega-weighted loss with the bid/ask band kept (any in-band surface is acceptable); cleaning rules come from the data; thresholds are never tuned on the evaluation days; the D17-D22 semantics and the existing CSV headers stay except for the optional `weight` column. Assumptions to confirm during spec review: the placeholder quality targets in WP-C7, the 250 ms cold-refit p99 limit for 40 x 100, and that the Black-Scholes European IV remains the calibration stand-in until WP-C4. Validate with the `DELIVERY.md` commands plus the replay on the real IBIT snapshots; do not report a command as passing unless it ran. Rules: do not edit the addenda, do not create tickets, branches, commits or code until the implementation spec is explicitly approved, and report files changed, validation evidence, unresolved risks and blockers.

## Problem

The C++ calibrator currently minimizes unweighted squared total-variance excess outside the bid/ask band. The real IBIT replay has large band misses, and the existing quote pipeline has no explicit premium, spread, or strike-window cleaning. The delivery must first quantify where cold-fit misses originate, then apply data-derived cleaning and mandatory vega weighting, followed by the ordered forward, synchrony, American-pricing, capacity, criteria, and close-out work packages.

## Non-goals

- Do not make SPY a gate or start SPY work before IBIT is settled.
- Do not make mid-centering part of the objective; any in-band surface remains acceptable.
- Do not change the live IB collector without separate user approval.
- Do not tune or select thresholds on 2026-10-15 or 16.
- Do not change `diagnostics.csv` headers or remove support for older quote CSVs without a `weight` column.
- Do not weaken eSSVI arbitrage constraints in production; WP-C6 is a diagnostic comparison only.

## Verified repository and data facts

- `python/convert_ibit_snapshots.py` passes spot as the forward to European Black-Scholes IV inversion, selects OTM quotes against that spot, and currently emits the established 13-column normalized quote header.
- `cpp/calibrator.hpp` defines `Quote` without a weight field. `cpp/calibrator.cpp::bandLoss` sums squared total-variance excess without weights; `readQuotes` maps fields by header name.
- `python/replay_snapshots.py` already discovers IBIT and SPY files separately and records source order. The sampled IBIT snapshot contains per-contract `tick_time`, snapshot `fetch_start_ms`/`fetch_end_ms`, and `underlying_start`/`underlying_end`, so WP-C3 can analyze recorded timing without changing the live collector.
- The configured data root has IBIT snapshots for 2026-10-10 and 12–16, plus SPY snapshots for 12–16. Current IBIT counts are 60 snapshots on Oct 12 and 76 on each of Oct 13–16; Oct 10 has one unusable/forced snapshot. Thus the threshold-development days have 212 IBIT snapshots and the Oct 15–16 holdout has 152. These counts must be rechecked when the replay is run.
- `docs/delivery/DELIVERY.md` configures local tickets, one ticket at a time in the primary checkout, and the full verification command. Its recorded master SHA is stale: the clean checkout at intake was `ef6c788b8e2479b9aa74a1b595fc9844d9094082`, which is the base used here.
- Baseline at that base passed: Release CMake build, CTest 5/5, Python pytest 10/10 (2026-10-17).

## User decisions

- **DEC-001 — IBIT-first scope.** Run gating work on IBIT; defer SPY until IBIT is settled.
- **DEC-002 — mandatory vega-weighted band loss.** Keep the bid/ask band objective and weight each out-of-band total-variance excess by its price sensitivity, as defined in WP-C9. Do not add a mid target.
- **DEC-003 — A/B is report-only.** Compare unweighted and vega-weighted cold fits on cleaned IBIT data, report both sets of metrics, and keep vega weighting as the default regardless of the result.
- **DEC-004 — WP-C7 approved.** Re-base quality criteria on cleaned IBIT data; retain the specified cold-latency limits of 250 ms p99 for 40×100 and 100 ms p99 for the real replay. The placeholder quality values are to be replaced by measured criteria in WP-C7.
- **DEC-005 — holdout protocol.** Derive thresholds using Oct 12–14; evaluate once on Oct 15–16. Do not use holdout results to choose filters or tune weights.
- **DEC-006 — filter selection approval.** WP-C1 proposes minimum bid, minimum premium, maximum relative spread, and strike-window rules from the IBIT training-day distributions. The user approves the final thresholds before WP-C5 implements them.

## Implementation decisions and inferences

- Run the tickets in this dependency order: WP-C1 → WP-C5 → WP-C9 → WP-C2 → WP-C3 → WP-C4 → WP-C6 → WP-C7 → WP-C8. The later tickets use the preceding ticket's committed output as their base; do not branch every ticket from the original base.
- WP-C1 is IBIT-only for threshold development. Report the prescribed expiry, moneyness, premium, relative-band-width, option-type, and fetch-position breakdowns on Oct 12–14; reserve Oct 15–16 for WP-C7's one-time holdout evaluation.
- For WP-C9, interpret `weight` as the per-quote scale `u_i` in the specified squared loss, `sum((u_i * dw_i)^2)`. Use median-one normalization per snapshot (the spec's concrete example) and retain raw sensitivity separately for price-space reporting. Missing `weight` means 1.0. Unweighted cap metrics remain visible as required; the A/B does not choose the production objective.
- Keep the existing European inversion until WP-C4, as the specified work-package order requires an apples-to-apples baseline. At WP-C4, derive/document pricing inputs from the cleaned parity/forward results where possible; if the data cannot determine a defensible rate/carry input for the American model, stop that ticket and request the missing decision rather than inventing it.
- WP-C3 may implement offline timestamp filtering/adjustment in the replay path. Any live collector change remains a separate approval gate.
- WP-C6 reports diagnostic unconstrained/constraint-removed fits only; production calibration continues enforcing all current safety constraints.

## Proposed modules and data/API changes

- Reuse the converter and replay preparation path for quote filters, per-rule exclusion counts, parity-derived forwards, synchrony experiments, and implied-vol inversion.
- Append an optional `weight` field to normalized quote CSVs; older files without it parse with weight 1.0. Add `Quote.weight` with a source-compatible default and propagate the weight through preparation, calibration objective/gates, and quality metrics. Preserve `diagnostics.csv` headers.
- Keep per-quote excess-ratio metrics unweighted for comparability with Addendum B; report separate price-space excess in ticks and dollars per contract, plus vega-weighted RMS and maximum.
- Record the WP-C1 and WP-C7 analysis/replay tables in `docs/fit-quality.md`; update `docs/csv-contract.md` for the optional column and filter/report semantics.

## Acceptance criteria

1. WP-C1 produces a reproducible IBIT cold-fit decomposition on training days and data-derived candidate cleaning thresholds without using holdout days for selection.
2. WP-C5 applies the user-approved filters consistently in conversion and replay, with per-rule and combined exclusion counts per snapshot.
3. WP-C9 makes vega-weighted band excess the mandatory/default cold objective; A/B output is diagnostic only, and old CSVs retain unit weights.
4. WP-C2 through WP-C6 report incremental effects against the cleaned baseline, one change at a time, preserving required arbitrage safety in production.
5. WP-C7 replaces placeholder quality criteria from measured cleaned-data results, publishes the one-time Oct 15–16 evaluation, and enforces the approved cold p99 limits.
6. WP-C8 documents warm calibration as opt-in/experimental, keeps its tests green, and reports a controlled interleaved `49fed2b` versus current cold-path A/B.
7. All configured repository verification passes; each work package has focused behavioral coverage where it changes code and a fresh independent diff review before handoff.

## Validation and rollout

- Baseline/full suite: `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q`.
- Focused C++ checks use the configured CTest pattern after building; Python behavior tests use `.venv/bin/python -m pytest -q <test-file>`.
- Exercise changed paths with the real IBIT replay at `~/vol-rv/data`, using Oct 12–14 for development and Oct 15–16 for the one-time evaluation. Record exact commands and outcomes; do not call target misses execution failures.
- Do not change the live IB collector, push, merge, or create PRs. One local commit and an independent review are required per ticket.

## Risks and blockers

- **Next approval gate:** WP-C5 cannot start until the user approves WP-C1's proposed cleaning thresholds (DEC-006).
- **American-model inputs:** WP-C4 is blocked if cleaned parity data and available repository inputs cannot provide defensible rates/carry for the specified American inversion; do not substitute guessed assumptions.
- **Holdout leakage:** report only the planned one-time Oct 15–16 evaluation; do not use those days to select cleaning or loss parameters.
- **Performance portability:** report the host/compiler/load for the 250 ms and 100 ms cold-p99 criteria; no cross-host comparison is implied.
