# IBIT cold-fit quality diagnosis (WP-C1)

## Scope and result

This is the unfiltered, unweighted current-pipeline baseline for IBIT training snapshots from 2026-10-12 through 2026-10-14. The analysis uses 212 independent cold fits and 113,965 included quotes. It does not read 2026-10-15 or 2026-10-16; those dates remain reserved for WP-C7. SPY is deferred.

The overall cold fit puts 71.71% of quotes above the 0.1 excess-ratio cap; RMS excess ratio is 2.410 band widths, and the maximum is 64.134. The current baseline has no cleaning filters and no vega weights. Each snapshot was prepared by `replay_snapshots.prepare_snapshot` and fitted with the existing `build/essvi_fit` CLI.

## Reproduction and definitions

From the repository root, with the Release binary built:

```sh
python3 scripts/analyze_fit_quality.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit
```

The command prints JSON to stdout. This checked-in Markdown report is transcribed from that output and is not overwritten by the runner.

The runner reads only `ibit/chain_20261012.jsonl.gz`, `chain_20261013.jsonl.gz`, and `chain_20261014.jsonl.gz`. For each usable snapshot it writes a temporary normalized CSV using `convert_ibit_snapshots.HEADER`, then invokes:

```text
build/essvi_fit normalized_quotes.csv parameters.csv fitted_quotes.csv surface_grid.csv diagnostics.csv
```

Temporary fit inputs and outputs are removed after each cold fit. The runner checks that every fit converges and that valid plus excluded quote counts reconcile to the raw contract count.

For each quote, bid and ask total variance are `T * IV^2`; the fitted variance is read from `fitted_quotes.csv`. Excess is the distance from fitted variance to the closed bid/ask total-variance interval, or zero in-band. Excess ratio is `excess / max(ask_variance - bid_variance, 1e-6)`. The cap is strictly `ratio > 0.1`. RMS is the square root of mean squared excess ratio. The contribution column below is each bucket's share of the sum of squared total-variance excesses, matching the current unweighted objective. No quotes were refitted after applying candidate filters; candidate retained-quality figures are post-hoc masks of this full-data fit, not filter-fit results.

The source contains 259,978 contracts across the 212 snapshots. Replay preparation retained 259,973 rows after excluding 5 locked quotes; it excluded 146,008 other rows and 0 crossed quotes. Of the prepared rows, 113,965 were valid OTM IV quotes and were scored. All 212 cold fits converged.

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

| Relative-spread bucket | Quotes | Share above cap | RMS ratio | Maximum ratio | Sum-excess share |
|---|---:|---:|---:|---:|---:|
| `< 0.25` | 87,996 | 80.49% | 2.706 | 64.134 | 97.98% |
| `0.25–0.5` | 12,012 | 54.46% | 0.882 | 11.491 | 1.16% |
| `0.5–1` | 13,844 | 31.39% | 0.755 | 28.536 | 0.85% |
| `1–2` | 113 | 7.08% | 0.037 | 0.128 | 0.003% |

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

## Cleaning candidates for approval (WP-C5 gate)

The candidate population is the 113,965 currently included quotes. Counts below are per-rule effects on the training split. Snapshot exclusion quantiles are p10 / median / p90, followed by the maximum snapshot count. `Retained share / RMS` are the post-hoc full-data-fit metrics described above; they do not predict the result of refitting after cleaning.

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
3. **Wide relative spreads are not the main source of the objective.** Quotes with relative spread below 0.25 account for 97.98% of summed excess². The p95 spread filter removes 2.85% of quotes, and the masked share-above-cap/RMS metrics worsen slightly.
4. **Near-forward and put buckets have high miss rates.** `0 < k <= 0.05` has 92.44% above cap and 28.69% of summed excess². Puts have a higher above-cap share than calls (76.08% vs 66.46%), while calls contribute more total squared excess because of their counts and premium mix.
5. **Fetch-order gradients are weak in this sample.** Within-snapshot fetch-position R² is 0.00418; the snapshot-move regression R² is 0.0465. Collection takes about 100 seconds, but the recorded linear relationships do not explain most of the signed residual variation.

## Decision required before WP-C5

No cleaning thresholds are selected by this analysis. The candidate data support two combined policies for explicit approval:

- **Less restrictive:** `bid > 0`, mid premium `>= $0.05`, relative spread `<= 0.6666666667`, and `0.02 <= |BS delta| <= 0.95`; excludes 20.19% of training quotes.
- **More restrictive delta window:** same rules, with `0.05 <= |BS delta| <= 0.95`; excludes 27.04%.

Either policy's masked cap/RMS figures are worse than the unfiltered fit. Cleaning may still be justified for input quality, but the actual fit effect must be measured after refitting in WP-C5. Await explicit threshold approval before implementing that ticket.
