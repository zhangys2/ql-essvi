# IBIT cold-fit quality diagnosis (WP-C1)

## Scope and result

This section preserves the historical, unfiltered, unweighted C1 baseline for IBIT training snapshots from 2026-10-12 through 2026-10-14. The original analysis used 212 independent cold fits and 113,965 included quotes before the corrected expiry convention. Current HEAD does not reproduce that count: its corrected-expiry no-cleaning comparator has 113,248 included quotes (WP-C5, minimum-bid-only). The analysis does not read 2026-10-15 or 2026-10-16; those dates remain reserved for WP-C7. SPY is deferred.

The historical C1 cold fits put 71.71% of quotes above the 0.1 excess-ratio cap; RMS excess ratio was 2.410 band widths, and the maximum was 64.134. The original baseline used no cleaning filters and no vega weights. Each snapshot was prepared by `replay_snapshots.prepare_snapshot` and fitted with the existing `build/essvi_fit` CLI.

## Reproduction and definitions

From the repository root, with the Release binary built:

```sh
python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit
```

The command prints JSON to stdout. On current HEAD, the corrected-expiry analyzer reproduces 113,248 included quotes, matching the minimum-bid-only comparator in WP-C5. The 113,965 C1 count and metrics above are retained from the historical pre-expiry-fix output; this Markdown report is not overwritten by the runner.

The runner reads only `ibit/chain_20261012.jsonl.gz`, `chain_20261013.jsonl.gz`, and `chain_20261014.jsonl.gz`. For each usable snapshot it writes a temporary normalized CSV using `convert_ibit_snapshots.HEADER`, then invokes:

```text
build/essvi_fit normalized_quotes.csv parameters.csv fitted_quotes.csv surface_grid.csv diagnostics.csv
```

Temporary fit inputs and outputs are removed after each cold fit. The runner checks that every fit converges and that valid plus excluded quote counts reconcile to the raw contract count.

For each quote, bid and ask total variance are `T * IV^2`; the fitted variance is read from `fitted_quotes.csv`. Excess is the distance from fitted variance to the closed bid/ask total-variance interval, or zero in-band. Excess ratio is `excess / max(ask_variance - bid_variance, 1e-6)`. The cap is strictly `ratio > 0.1`. RMS is the square root of mean squared excess ratio. Relative total-variance band width is `(ask_variance - bid_variance) / midpoint_variance`, where midpoint variance is `(ask_variance + bid_variance) / 2`; this groups the same total-variance band used by the fit. The relative option-price spread `(ask-bid)/mid` is shown separately for comparison and cleaning-candidate analysis. The contribution column below is each bucket's share of the sum of squared total-variance excesses, matching the current unweighted objective. No quotes were refitted after applying candidate filters; candidate retained-quality figures are post-hoc masks of this full-data fit, not filter-fit results.

In the historical C1 run, the source contained 259,978 contracts across the 212 snapshots. Replay preparation retained 259,973 rows after excluding 5 locked quotes; it excluded 146,008 other rows and 0 crossed quotes. Of the prepared rows, 113,965 were valid OTM IV quotes and were scored. All 212 cold fits converged.

## Overall and daily baseline

| Session | Included quotes | Above-cap quotes | Share above cap | RMS ratio | Maximum ratio |
|---|---:|---:|---:|---:|---:|
| 2026-10-12 | 30,025 | 21,250 | 70.77% | 2.555 | 28.536 |
| 2026-10-13 | 41,365 | 29,783 | 72.00% | 2.457 | 64.134 |
| 2026-10-14 | 42,575 | 30,688 | 72.08% | 2.253 | 19.308 |
| **Pooled** | **113,965** | **81,721** | **71.71%** | **2.410** | **64.134** |

## Fit misses by quote characteristics

`Sum-excess share` is the fraction of all squared total-variance excess contributed by that bucket; it is not a refitted objective with that bucket removed.

### Expiry

| Days to expiry | Quotes | Share above cap | RMS ratio | Maximum ratio | Sum-excess share |
|---|---:|---:|---:|---:|---:|
| 0–7 | 26,200 | 66.90% | 2.539 | 28.536 | 1.00% |
| 8–30 | 50,603 | 69.81% | 2.061 | 16.273 | 4.89% |
| 31–90 | 24,235 | 84.03% | 3.227 | 64.134 | 15.20% |
| 91–180 | 12,927 | 65.76% | 1.406 | 23.553 | 78.92% |

### Log-moneyness `k = ln(K/F)`

| Bucket | Quotes | Share above cap | RMS ratio | Maximum ratio | Sum-excess share |
|---|---:|---:|---:|---:|---:|
| `k <= -0.20` | 11,476 | 75.07% | 1.589 | 46.640 | 15.67% |
| `(-0.20,-0.10]` | 20,517 | 64.82% | 1.688 | 58.640 | 3.64% |
| `(-0.10,-0.05]` | 13,937 | 91.12% | 3.066 | 64.134 | 4.98% |
| `(-0.05,0)` | 16,235 | 78.10% | 3.063 | 28.536 | 10.17% |
| `(0,0.05]` | 16,072 | 92.44% | 3.675 | 23.188 | 28.69% |
| `(0.05,0.10]` | 15,406 | 62.25% | 1.503 | 13.400 | 10.49% |
| `(0.10,0.20]` | 18,547 | 46.96% | 1.349 | 15.447 | 14.33% |
| `k > 0.20` | 1,775 | 71.61% | 1.749 | 9.574 | 12.02% |

### Mid premium (USD per option share)

| Mid-premium bucket | Quotes | Share above cap | RMS ratio | Maximum ratio | Sum-excess share |
|---|---:|---:|---:|---:|---:|
| `< $0.05` | 22,913 | 44.22% | 0.939 | 28.536 | 1.78% |
| `$0.05–$0.25` | 31,105 | 74.68% | 2.208 | 46.640 | 5.50% |
| `$0.25–$1` | 34,461 | 83.27% | 2.909 | 64.134 | 12.86% |
| `$1–$5` | 25,385 | 77.08% | 2.793 | 23.553 | 76.68% |
| `>= $5` | 101 | 99.01% | 1.801 | 7.208 | 3.18% |

### Relative price spread `(ask-bid)/mid`

This is the option-price spread used in the cleaning candidates below; it is distinct from the total-variance width used by the fitting objective.

| Relative price-spread bucket | Quotes | Share above cap | RMS ratio | Maximum ratio | Sum-excess share |
|---|---:|---:|---:|---:|---:|
| `< 0.25` | 87,996 | 80.49% | 2.706 | 64.134 | 97.98% |
| `0.25–0.5` | 12,012 | 54.46% | 0.882 | 11.491 | 1.16% |
| `0.5–1` | 13,844 | 31.39% | 0.755 | 28.536 | 0.85% |
| `1–2` | 113 | 7.08% | 0.037 | 0.128 | 0.003% |

### Relative total-variance band width `(ask variance-bid variance)/midpoint variance`

The midpoint variance is `(ask variance + bid variance) / 2`; each side's total variance is `T * IV^2`. Buckets use the same relative-width ranges as the option-price spread table.

| Relative total-variance band-width bucket | Quotes | Share above cap | RMS ratio | Maximum ratio | Sum-excess share |
|---|---:|---:|---:|---:|---:|
| `< 0.25` | 110,355 | 73.21% | 2.434 | 64.134 | 99.82% |
| `0.25–0.5` | 3,597 | 25.74% | 1.491 | 28.536 | 0.18% |
| `0.5–1` | 13 | 7.69% | 0.747 | 2.689 | 0.002% |

### Option type

| Type | Quotes | Share above cap | RMS ratio | Maximum ratio | Sum-excess share |
|---|---:|---:|---:|---:|---:|
| Calls | 51,800 | 66.46% | 2.370 | 23.188 | 65.53% |
| Puts | 62,165 | 76.08% | 2.442 | 64.134 | 34.47% |

## Fetch-order and underlying-move check (H2)

The signed residual is fitted total variance minus the midpoint of the bid/ask total-variance bounds. Source-order position is the raw contract array index normalized to `[0,1]`; this array is the recorded fetch order. Regressions are within-snapshot: each snapshot is centered independently before pooling, preventing different snapshot-level residual means from driving the position slope. The 212 individual slopes are also summarized below.

| Predictor | Observations | Within-snapshot pooled slope | R² | Per-snapshot slope p10 / median / p90 |
|---|---:|---:|---:|---:|
| Normalized source fetch position | 113,965 quotes / 212 snapshots | 0.000564 | 0.00418 | -0.000064 / 0.000620 / 0.000966 |
| Normalized contract `tick_time` | 113,965 quotes / 212 snapshots | 0.000577 | 0.00434 | -0.000104 / 0.000619 / 0.000978 |

The underlying move is the log return from the start to end underlying midpoint of each collection; the response is that snapshot's median signed residual. Across 212 snapshots, the regression slope is 0.01442 and R² is 0.0465. Collection duration was 98.155 s p10, 102.164 s median, 106.175 s p90, and 106.429 s maximum. The signed log-move p10/median/p90 was -0.000651 / 0 / 0.000649, with maximum absolute move 0.002368. The low explanatory power is descriptive evidence, not proof that asynchrony has no effect; these regressions do not establish causality.

## C1 post-hoc cleaning candidates (not refits)

The candidate population is the 113,965 historically included C1 quotes. Counts below are per-rule effects on the training split. Snapshot exclusion quantiles are p10 / median / p90, followed by the maximum snapshot count. `Retained share / RMS` are the post-hoc full-data-fit metrics described above; they do not predict the result of refitting after cleaning.

The minimum observed positive bid is `$0.01`. The existing European IV inversion already rejects zero bids for these OTM quotes, so the explicit `bid > 0` rule excludes no additional rows. Mid-premium quantiles are `$0.02` p05, `$0.025` p10, `$0.065` p25, `$0.285` median, `$0.89` p75, `$1.825` p90, and `$2.45` p95. Relative-spread p95 is `0.6666666667`; absolute Black–Scholes delta p05/p10/p25/median/p75/p90/p95 is `0.01525 / 0.02014 / 0.04736 / 0.14306 / 0.29789 / 0.41621 / 0.45807`. The delta uses the current European mid-IV, forward, strike, and expiry; the upper bound `0.95` does not bind (maximum observed absolute delta is `0.55764`).

| Candidate rule | Retained / excluded | Excluded per snapshot: p10 / median / p90; max | Retained share above cap | Retained RMS ratio |
|---|---:|---:|---:|---:|
| `bid > $0` | 113,965 / 0 | 0 / 0 / 0; 0 | 71.71% | 2.410 |
| Mid premium `>= $0.05` | 91,052 / 22,913 | 91 / 111 / 120; 123 | 78.62% | 2.654 |
| Mid premium `>= $0.10` | 77,547 / 36,418 | 150 / 177 / 186; 197 | 80.20% | 2.795 |
| Relative spread `<= 0.6666666667` (training p95) | 110,712 / 3,253 | 12 / 15 / 19; 30 | 73.29% | 2.445 |
| `0.02 <= |BS delta| <= 0.95` | 102,753 / 11,212 | 42 / 55 / 59; 63 | 74.46% | 2.533 |
| `0.05 <= |BS delta| <= 0.95` | 84,398 / 29,567 | 118 / 144 / 152; 159 | 80.37% | 2.763 |
| Combined: `bid > 0`, mid `>= $0.05`, spread `<= 0.6666666667`, delta `[0.02,0.95]` | 90,954 / 23,011 | 91 / 112 / 120; 123 | 78.61% | 2.656 |
| Combined: same, delta `[0.05,0.95]` | 83,149 / 30,816 | 123 / 150.5 / 157.9; 166 | 80.33% | 2.756 |

### Ranked diagnosis

1. **Long-dated bands dominate the squared total-variance excess.** The 91–180d bucket has 11.34% of quotes but 78.92% of summed excess². The 31–90d bucket adds 15.20%; together, expiries over 30d account for 94.12%.
2. **The $1–$5 premium bucket dominates by price scale.** It has 22.28% of quotes and 76.68% of summed excess². Quotes below `$0.05` have lower band-miss ratios and contribute only 1.78% of summed excess²; minimum-premium filtering removes many quotes but does not improve the masked share-above-cap or RMS metrics.
3. **Wide relative price spreads are not the main source of the objective.** Quotes with relative spread below 0.25 account for 97.98% of summed excess². The p95 spread filter removes 2.85% of quotes, and the masked share-above-cap/RMS metrics worsen slightly.
4. **Near-forward and put buckets have high miss rates.** `0 < k <= 0.05` has 92.44% above cap and 28.69% of summed excess². Puts have a higher above-cap share than calls (76.08% vs 66.46%), while calls contribute more total squared excess because of their counts and premium mix.
5. **Fetch-order gradients are weak in this sample.** Within-snapshot fetch-position R² is 0.00418; the snapshot-move regression R² is 0.0465. Collection takes about 100 seconds, but the recorded linear relationships do not explain most of the signed residual variation.

## DEC-006 — approved policy for WP-C5

The user approved the less-restrictive policy on 2026-10-17: bid `> $0`, raw mid premium `(bid+ask)/2 >= $0.05` per option share, raw relative spread `(ask-bid)/mid <= 0.6666666667`, and absolute European Black–Scholes delta in `[0.02, 0.95]`. The delta uses the existing mid-IV, forward, strike, and expiry. The more-restrictive `0.05` delta-floor candidate above was not selected.

All cap/RMS values in the preceding table are post-hoc evaluations against the full-data unfiltered fit, not refit results. They are preserved as C1 historical evidence; WP-C5 measures actual cold-fit effects under the approved profiles.

## WP-C5 refitted cleaning profiles

Reproduce from the repository root after building the Release fitter:

```sh
python3 scripts/evaluate_quote_cleaning.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit
```

The runner refits all 212 IBIT snapshots from Oct 12–14 independently under each of the four single-rule profiles and the combined profile. It reads only `chain_20261012.jsonl.gz`, `chain_20261013.jsonl.gz`, and `chain_20261014.jsonl.gz`; Oct 15–16 and SPY are not read. JSON output includes per-snapshot counts for all rules and unique combined exclusions, plus refitted unweighted cold-fit metrics. The C1 diagnosis and its original baseline are preserved as historical results. These C5 refits use the corrected shared expiry convention: New York session date, calendar-day difference divided by 365, and same-day/past expiries excluded in both converter and replay. The minimum-bid-only profile is the current no-cleaning comparator under that convention.

| Refit profile | Excluded by rule: bid / premium / spread / delta | Unique cleaning exclusions | Included quotes | Above-cap share | RMS excess ratio | Maximum ratio |
|---|---:|---:|---:|---:|---:|---:|
| Unfiltered C1 baseline (historical) | — | 0 | 113,965 | 71.71% | 2.410 | 64.134 |
| Minimum bid only | 0 / 0 / 0 / 0 | 0 | 113,248 | 71.22% | 2.248 | 64.133 |
| Mid premium >= $0.05 | 0 / 22,450 / 0 / 0 | 22,450 | 90,798 | 69.18% | 2.180 | 64.133 |
| Relative spread <= 0.6666666667 | 0 / 0 / 3,253 / 0 | 3,253 | 109,995 | 72.76% | 2.272 | 64.133 |
| Absolute delta [0.02, 0.95] | 0 / 0 / 0 / 11,212 | 11,212 | 102,036 | 68.50% | 2.103 | 64.140 |
| Combined approved policy | 0 / 22,450 / 3,253 / 11,212 | 22,548 | 90,700 | 69.10% | 2.174 | 64.140 |

Per-rule counts are independent and overlap; the combined count is unique. The minimum-bid rule adds no exclusions because the existing European IV inversion already rejects zero bid prices in this OTM sample. All five current profiles completed 212/212 cold fits. Raw source size was 259,978 contracts per profile; the replay's existing locked/crossed/other categories still reconcile, with cleaning exclusions reported as a diagnostic subset of `excluded_other`.

Against the corrected minimum-bid-only comparator, the premium, delta, and combined profiles lower share-above-cap and RMS; the spread-only profile worsens both. The combined maximum ratio is slightly higher than comparator. These are measured effects of the explicitly approved policy, not a claim that every metric improves. Vega-weighted fitting and its report-only A/B belong to WP-C9, not these unweighted C5 measurements.

## WP-C9 mandatory vega-weighted fit and report-only A/B

Reproduce the report from the repository root with the Release fitter:

```sh
.venv/bin/python scripts/evaluate_vega_weighting.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit
```

The evaluator reads only IBIT `chain_20261012.jsonl.gz`, `chain_20261013.jsonl.gz`, and `chain_20261014.jsonl.gz`. It runs independent unit-weight and per-snapshot median-normalized raw \(dC/dw\)-weighted cold fits under the approved C5 combined cleaning profile. Both arms completed 212/212 snapshots with 90,700 included quotes from 259,978 input contracts. Combined cleaning excluded 22,548 quotes (22,450 minimum-premium, 3,253 relative-spread, 11,212 delta-window, 0 minimum-bid; rule counts overlap). Each arm also had 5 locked contracts, 0 crossed contracts, and 169,273 `excluded_other` contracts; combined cleaning exclusions are included in that count. Oct 15–16 and SPY were not read.

The converter and replay use the same New York session date and raw calendar-day year fraction; same-day and past-expiry contracts are excluded before weight normalization. The report below reflects this shared eligibility rule.

| Fit arm | Fits / snapshots | Included quotes | Combined cleaning exclusions | Share above unweighted cap | RMS excess ratio | Maximum ratio | Summed fit objective |
|---|---:|---:|---:|---:|---:|---:|---:|
| Unit weights | 212 / 212 | 90,700 | 22,548 | 69.10% | 2.1742 | 64.1403 | 0.326841 |
| Vega weights | 212 / 212 | 90,700 | 22,548 | 68.82% | 2.0913 | 43.9205 | 0.133402 |

The cap share, RMS ratio, and maximum ratio remain the original unweighted per-quote metrics. The summed objective is \(\sum e_i^2\) for the unit arm and \(\sum (u_i e_i)^2\) for the vega arm, where \(e_i\) is total-variance band excess and \(u_i\) is normalized raw sensitivity.

| Fit arm | Sum of squared vega-weighted excess | Vega-weighted RMS | Vega-weighted maximum |
|---|---:|---:|---:|
| Unit weights | 0.143387 | 0.00125734 | 0.0124365 |
| Vega weights | 0.133402 | 0.00121277 | 0.0126194 |

Price-space excess is the first-order equivalent \(dC/dw\) times absolute total-variance band excess; it is not a direct repricing error. One tick is `$0.01` per option share. Contract-dollar values multiply by 100 shares, the standard OCC equity-option unit; the source snapshots contain no multiplier field.

| Fit arm | Price excess ticks RMS | Maximum ticks | Dollars/share RMS | Maximum dollars/share | Dollars/100-share contract RMS | Maximum dollars/contract |
|---|---:|---:|---:|---:|---:|---:|
| Unit weights | 7.0010 | 71.3250 | $0.070010 | $0.713250 | $7.0010 | $71.3250 |
| Vega weights | 6.7489 | 72.3740 | $0.067489 | $0.723740 | $6.7489 | $72.3740 |

The A/B is report-only: mandatory vega weighting remains the production objective regardless of the comparison. In this sample, vega weighting lowers the summed objective, unweighted share above cap, unweighted RMS excess ratio, and price-space RMS; it raises the maximum price-space excess slightly. These competing outcomes are not a gate or a claim that every quality metric improves. The 100-share assumption follows the [OCC Equity Options Product Specifications](https://www.theocc.com/clearance-and-settlement/clearing/equity-options-product-specifications).
