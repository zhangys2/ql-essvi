# Shared CSV contract

All files are UTF-8 CSV, comma-delimited, with an exact header and decimal floating-point values. Numeric output uses at least 12 significant digits where generated from a fitted surface. Dates use `YYYY-MM-DD`; times are year fractions; variance and `psi` are dimensionless. Empty numeric fields are allowed only where the row status/flag explains why the value is unavailable; no `NaN` or `inf` tokens are emitted.

## normalized_quotes.csv

One row per raw quote:

```text
quote_id,effective_date,expiry_date,time_to_expiry,forward,discount_factor,strike,option_type,bid_iv,ask_iv,mid_iv,included,filter_reason,weight
```

`option_type` is `C` or `P`; IVs are decimal volatilities (not percentage points). Excluded values remain present with empty numeric fields and `included=false`.

Current converters emit the trailing `weight` column; readers accept both this 14-column schema and the legacy 13-column schema, which means unit weights. For included quotes, `weight` is the finite positive Black–Scholes price sensitivity per unit total variance, `D*F*phi(d1)/(2*sigma*sqrt(T))`, divided by the median sensitivity among included quotes in that snapshot. Excluded rows have an empty weight. Missing or blank weights mean 1.0 for C++ compatibility; schema validation requires each included weight to be finite and positive.

`filter_reason` is empty for included rows. Existing input-quality failures use one stable reason code. Cleaning exclusions contain every failed selected rule in this stable order: `minimum_bid`, `minimum_premium`, `relative_spread`, `delta_window`, separated by semicolons. Cleaning applies only after the existing contract, market, OTM, and IV-inversion checks pass. The approved thresholds use raw option prices: bid `> $0`; mid premium `(bid+ask)/2 >= $0.05` per option share; relative spread `(ask-bid)/mid <= 0.6666666667`; and absolute European Black–Scholes delta from the existing mid-IV, forward, strike, and expiry in the inclusive range `[0.02, 0.95]`.

Per-rule cleaning counts are independent and may overlap. The combined cleaning count counts each quote once. In replay, cleaning exclusions remain part of `excluded_other` for raw-source reconciliation; the additional cleaning counters are diagnostic subsets, not extra categories to add to that reconciliation. The replay TSV manifest records `excluded_cleaning_minimum_bid`, `excluded_cleaning_minimum_premium`, `excluded_cleaning_relative_spread`, `excluded_cleaning_delta_window`, and `excluded_cleaning_combined` per snapshot. Its JSON report includes aggregate counts and the same per-snapshot values under `cleaning_exclusions_by_snapshot`.

## fitted_quotes.csv

```text
quote_id,time_to_expiry,strike,log_moneyness,fitted_total_variance,fitted_iv,status
```

## surface_grid.csv

```text
time_to_expiry,forward,discount_factor,log_moneyness,strike,fitted_total_variance,fitted_iv,is_tail
```

## parameters.csv

```text
time_to_expiry,theta,psi,rho,phi,butterfly_safe,calendar_safe
```

## diagnostics.csv

```text
model,converged,objective,iterations,runtime_seconds,error
```

Exactly one row is emitted per fit. `converged` is `true` only when the requested constrained solver completed and all safety checks passed. A failed fit emits `converged=false` and a non-empty `error`; it never falls back to an unconstrained result. `runtime_seconds` is wall clock time and is indicative only across languages.

The legacy CVI notebook additionally emits `benchmark_output/parameters.csv` with this notebook-specific schema (its CVI fitting behavior is unchanged):

```text
time_to_expiry,atm_total_variance,observed_min_log_moneyness,observed_max_log_moneyness,grid_points,fit_rows
```

This is a parameter summary, not an eSSVI `(theta, psi, rho, phi)` file. The shared adapter retains the eSSVI parameter schema separately and joins both models only through quote IDs and the dense-grid contract.
