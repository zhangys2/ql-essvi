# ql-essvi Calibrator — Addendum C (fit quality on real data; retire the speed chase)

Extends [[2026-10-16-ql-essvi-calibrator-speedup-addendum-b]]. Written 2026-10-17 after re-checking the implemented Addendum B (commit `8fca20e`, branch `delivery/calibrator-speedup-addendum-b/CAL-B6`). Facts marked **(verified)** come from code or measurements I ran or read; **(hypothesis)** items must be tested by the work packages below.

## 1. Why this addendum

1. **The speed targets are not met and cannot matter at the chosen cadence.** Warm p99 and fallback targets are missed in every scenario (the agent's own tables and my independent run agree). Yuan chose to tune for 5-minute updates. A full cold refit of a 40 x 100 chain takes about 55 to 110 ms and the real IBIT replay's cold p50 is 24 to 30 ms **(verified)**, which is under 0.04% of a 5-minute interval. Further warm-path work buys nothing at that cadence.
2. **On real data even the cold fit misses the bands by a lot (verified from `docs/perf-results.md`, 739-snapshot replay):** 63 to 78% of quotes sit beyond the 0.1 cap, the RMS excess ratio is 1.7 to 3.1 band widths, and the maximum ratio reaches 64. Warm fallback is 94 to 100%, so warm equals cold there. Quality, not speed, is the open problem.
3. **My synthetic check shows the opposite:** on a chain generated from a valid eSSVI surface, cold fits reach near-zero loss. So the model and optimizer can fit data that is consistent with them; the real-data misfit comes from the data pipeline, the model's constraints, or both.

## 2. Decisions proposed (Yuan to confirm)

- **D-C1.** For the 5-minute cadence the production path is the **cold refit**. The warm API stays in the code, off by default and marked experimental; its p99, fallback and latency criteria are no longer blocking. No further warm-path tuning unless a sub-second cadence is required.
- **D-C2.** Quality criteria are re-based on what a cold fit can achieve on **cleaned** real data (WP-C7). The numbers in Addendum B section 3 (cap 0.1, RMS 0.02/0.05) were set before any real-data evidence and are not reachable today.
- **D-C3.** No tuning of thresholds on the data used to report results: tune on 2026-10-12 to 14, evaluate once on 2026-10-15 and 16, then on later days (the agent tuned a shortcut threshold of 0.1034 on the same 739-snapshot replay it reports; treat that threshold as unvalidated).

## 3. What the data pipeline does today (verified in `python/replay_snapshots.py` and `python/convert_ibit_snapshots.py`)

- The forward passed to IV inversion is the **spot reference** (`quote_row(..., spot)`), so there is no carry, dividend or borrow term, and no put-call-parity forward. OTM selection (calls above, puts below) is also relative to spot.
- Implied vols are **European Black-Scholes** inversions of bid and ask, a stand-in for American options (documented by the agent).
- One `spot_ref` is used for the whole snapshot, while a snapshot takes about 100 to 130 seconds to collect (collector logs show 106 to 110 s for IBIT) and the contract order within a snapshot is the fetch order.
- Filters: locked and crossed quotes excluded, same-day expiries excluded, non-OTM relative to spot excluded, failed inversions excluded. There is no minimum bid or premium filter, no maximum relative spread filter, and no strike-range or delta window.
- Objective: squared total-variance excess beyond the bid/ask band, unweighted (decision D17).

## 4. Hypotheses for the misfit (to be tested, not assumed)

| # | Hypothesis | Rough size (my estimate) |
|---|---|---|
| H1 | Forward = spot ignores carry. Over a year at about 4% rates this moves log-moneyness by about 4%, mislocates the ATM point and the OTM split, and corrupts ITM-near-ATM inversions. | Larger for long expiries |
| H2 | Snapshot asynchrony. A 2-minute spread of quote times against one spot: with BTC volatility near 50%, the 2-minute standard deviation of IBIT is about 0.1%, which for a one-week ATM option is about 0.9 vol point of apparent IV noise, comparable to the quoted half-band (about 0.8 vol point). | Large for IBIT short expiries |
| H3 | European stand-in for American options (early-exercise value in puts, dividends for SPY). | Small OTM, larger for puts and long expiries |
| H4 | Uncleaned quotes: zero bids, one-tick quotes at very low premium, extremely wide bands, deep wings where bands are huge in total variance and dominate the unweighted loss. | Unknown |
| H5 | Model capacity: the Martini-Mingone butterfly bound and the adjacent-slice calendar conditions bind on real smiles, so a constrained eSSVI slice cannot reach the bands even for clean data. | Unknown |
| H6 | Unweighted squared total-variance loss lets a few high-variance quotes (long expiries, wings) set the fit. | Unknown |

## 5. Work packages (tickets, in this order)

Order after Yuan's confirmation on 2026-10-17 that data cleaning was missed in the original pipeline: **WP-C1 (read-only diagnosis, to choose the filters) -> WP-C5 (cleaning, now the main fix) -> WP-C9 (vega-weighted loss) -> WP-C2 (forward) -> WP-C3 (synchrony) -> WP-C4 (American) -> WP-C6 (model capacity) -> WP-C7 (re-base and re-report) -> WP-C8 (close-out)**. All of it on IBIT first (Yuan, 2026-10-17); the cleaning thresholds come from the IBIT data. WP-C2 to WP-C4 are evaluated on top of the cleaned data, one at a time, so each effect is measured against the cleaned baseline and not against the uncleaned one. H4 (uncleaned quotes) is treated as confirmed in kind; its size is still to be measured.

### WP-C1 — Decompose the miss on real data (first; read-only analysis)
- On the 739-snapshot capture, **cold fits only**, report for the current pipeline: share over cap and RMS ratio broken down by asset, expiry bucket (days to expiry), moneyness bucket (log-moneyness or delta), premium bucket, relative band width, and option type; and the position of the contract within its snapshot's fetch order (H2 test: regress the signed residual on fetch position and on the spot move during the fetch).
- Deliverable: a table and a short ranked list of where the excess comes from. Tests H2, H4 and H6 directly.

### WP-C2 — Forward and carry from put-call parity
- For each expiry, estimate the forward from near-ATM call and put mids, `C - P = DF (F - K)` (American adjustments or restriction to the strikes where early exercise is negligible), with a documented fallback to rate and dividend or borrow assumptions. Use it for log-moneyness, OTM selection and IV inversion.
- Report cold-fit quality with F = spot versus F from parity (H1).

### WP-C3 — Synchronous quote sets
- Quantify asynchrony (WP-C1) and test mitigations: use only quotes within a short window of the reference spot; interpolate or adjust each quote to a common time using the recorded underlying where available; or restructure collection (ATM region first, shorter fetch). Report effect on quality (H2).
- If a collector change is needed, propose it separately; do not touch the live IB collector without Yuan's approval.

### WP-C4 — American pricing in the converter
- Replace the European inversion with a Bjerksund-Stensland (2002) style American inversion in the converter (with dividends for SPY), keeping the European result for comparison. Report effect on quality (H3).

### WP-C5 — Quote cleaning policy
- Add explicit, counted filters: minimum bid greater than zero, minimum premium (a few ticks), maximum relative spread, a strike window (for example |delta| from 0.05 to 0.95, or within a set number of standard deviations), and optional vega or relative-spread weights. Record exclusion counts per rule per snapshot. Evaluate each rule alone and combined (H4, H6).

### WP-C6 — Model capacity check
- For clean subsets, compare the constrained per-slice eSSVI fit with (a) an unconstrained slice fit and (b) the fit with each constraint removed in turn (butterfly bound, Roger-Lee, calendar). Report band loss for each (H5). If the constraints are binding on real smiles, say so; the quality targets must then be defined relative to a constrained optimum.

### WP-C7 — Re-base the criteria and re-report once
- Using the best cleaned configuration, define achievable cold quality and set the quality criteria relative to it (propose numbers from the data; placeholders until then: share over cap at most 10% target and 25% hard, RMS ratio at most 0.5, to be replaced). Re-run the replay under the D-C3 split protocol and publish one clean table (not a series of superseded runs).
- Replace the speed criteria with: cold refit p99 at most 250 ms for 40 x 100 on vps1 (about 0.1% of the cadence), and cold p99 of the real replay at most 100 ms.

### WP-C8 — Close out the warm path and the cold-path A/B
- Make the warm path opt-in and document it as experimental; keep its tests green. Resolve the "inconclusive" cold-path A/B by building `49fed2b` (the base commit) from `git archive` into a separate directory with the same compiler and flags, and timing both binaries interleaved over at least 10 repeated pairs with the same input. Report medians and spread.

### WP-C9 - Vega-weighted band loss (decided 2026-10-17; runs right after WP-C5)
- **Weight.** For quote i with total-variance excess `dw_i` outside its band, the price error is `(dC/dw)_i * dw_i`, where `dC/dw = vega / (2 sigma T)` from the Black-Scholes sensitivity of the option price to total variance `w = sigma^2 T` (for the European stand-in, or the American equivalent once WP-C4 lands). The new loss is the sum over quotes of `(u_i * dw_i)^2`, with `u_i = (dC/dw)_i` normalized per snapshot (for example to a median of 1) so its scale does not drift. In words: the loss measures price error, so high-vega near-the-money quotes count and low-vega wings count little.
- **Plumbing.** Compute `u_i` in the Python converter from the quote's mid IV, forward, strike and expiry, and pass it as an optional extra column (`weight`, default 1.0 when absent) so the existing CSV contract and older files stay valid; add `weight` to `Quote` in C++ and use it in the band loss, the gate and the metrics. Do not change `diagnostics.csv` headers.
- **Metrics (report both, side by side).** The unweighted per-quote cap metrics from Addendum B (share over cap, maximum and RMS excess ratio), and new price-space metrics: excess price in ticks (IBIT options mostly trade in one-cent ticks) and in dollars per contract, vega-weighted RMS, and maximum.
- **A/B (report only).** Cold fits on cleaned IBIT data compare the unweighted and vega-weighted losses. Report both outcomes using the metrics above; this comparison does not gate adoption. The vega-weighted band loss is mandatory and remains the default regardless of the A/B result; do not revert to unweighted loss based on that result.
- **Optional comparison arm** (only if Yuan asks): a vega-weighted fit to the mid inside the band.

## 6. Process

- One ticket at a time in the primary checkout, as in `docs/delivery/DELIVERY.md`; decision-only intake with this addendum as the handoff; spec approval before tickets.
- Analysis tickets (C1) produce tables in `docs/perf-results.md` or a new `docs/fit-quality.md`; code changes only where a work package asks for them.
- Do not relax a criterion silently. Any change to a threshold is recorded with the data that justified it.

## 7. Decisions confirmed and open questions

Confirmed by Yuan on 2026-10-17 (reply "yes, data cleaning was missed before"):
- D-C1 to D-C3 stand: cold path in production for 5-minute updates; quality criteria re-based on cleaned data; split-day protocol (tune on 12 to 14 October, evaluate once on 15 and 16 October).
- Data cleaning was missed in the original pipeline; WP-C5 is the main fix and runs right after WP-C1.
- Changing the IB snapshot collection later is acceptable in principle if WP-C3 shows asynchrony matters; any change to the live collector still needs a separate explicit approval (recorded here as understood from the "yes", to be reconfirmed when the change is proposed).

Answered by Yuan on 2026-10-17:
1. **Asset: IBIT now.** All work packages run on IBIT first. SPY is deferred (it can be reported as a secondary check once IBIT is settled, but it is not a gate).
2. **Objective: use a vega weight.** The loss becomes vega-weighted (WP-C9 below defines it exactly). The bid/ask band structure of D17 stays (any in-band surface is acceptable, no mid-centering); what changes is that each quote's band excess is weighted by its price sensitivity.
3. **Cleaning rules: from the data.** WP-C1 proposes the thresholds (minimum bid, minimum premium, maximum relative spread, strike window) from the IBIT distributions; the agent reports each rule's effect and exclusion counts, and Yuan approves the final list.

Still open: none blocking. Interpretation note for item 2: "use a vega weight" is read as weighting the existing band-excess loss, not as switching the fit target to the mid. If a mid-based fit is wanted as well, say so and WP-C9 gains a comparison arm.

## 8. Decision-only handoff (for the next delivery cycle)

Read first: `docs/delivery/DELIVERY.md`, `README.md`, `docs/parameterization.md`, `docs/csv-contract.md`, `docs/calibrator-speedup-addendum-b.md`, this addendum (`docs/calibrator-speedup-addendum-c.md`), `docs/perf-results.md`, `python/convert_ibit_snapshots.py`, `python/replay_snapshots.py`, `python/run_snapshot_replay.py`, `cpp/calibrator.cpp`, `cpp/calibrator.hpp`. Build, in this order and on IBIT first: (1) WP-C1 read-only diagnosis of the cold-fit misses on the IBIT replay by expiry, moneyness, premium, relative band width and fetch-order position, proposing cleaning thresholds from the data; (2) WP-C5 counted cleaning filters (minimum bid, minimum premium, maximum relative spread, strike window) applied identically everywhere, with each rule's exclusion counts and effect reported; (3) WP-C9 the vega-weighted band loss (weight = Black-Scholes price sensitivity to total variance, normalized per snapshot, passed as an optional `weight` CSV column) with side-by-side unweighted and price-space metrics and an A/B on cleaned data; (4) WP-C2 forward from put-call parity; (5) WP-C3 synchronous quote sets (analysis first; any change to the live collector needs separate approval); (6) WP-C4 American inversion; (7) WP-C6 model-capacity check by removing each constraint in turn; (8) WP-C7 re-base the quality criteria on the cleaned IBIT result and re-report once under the split-day protocol (tune on 2026-10-12 to 14, evaluate once on 2026-10-15 and 16); (9) WP-C8 make the warm path opt-in and resolve the cold-path A/B by building `49fed2b` from `git archive` and timing interleaved pairs. Decisions already made by Yuan: cold refit is the production path for 5-minute updates and warm-path latency and fallback criteria are no longer blocking; data cleaning was missed and is the main fix; IBIT first, SPY deferred; vega-weighted loss with the bid/ask band kept (any in-band surface is acceptable); cleaning rules come from the data; thresholds are never tuned on the evaluation days; the D17-D22 semantics and the existing CSV headers stay except for the optional `weight` column. Assumptions to confirm during spec review: the placeholder quality targets in WP-C7, the 250 ms cold-refit p99 limit for 40 x 100, and that the Black-Scholes European IV remains the calibration stand-in until WP-C4. Validate with the `DELIVERY.md` commands plus the replay on the real IBIT snapshots; do not report a command as passing unless it ran. Rules: do not edit the addenda, do not create tickets, branches, commits or code until the implementation spec is explicitly approved, and report files changed, validation evidence, unresolved risks and blockers.
