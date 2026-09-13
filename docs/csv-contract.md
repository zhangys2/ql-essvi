# Shared CSV contract

All files are UTF-8 CSV, comma-delimited, with an exact header and decimal floating-point values. Numeric output uses at least 12 significant digits where generated from a fitted surface. Dates use `YYYY-MM-DD`; times are year fractions; variance and `psi` are dimensionless. Empty numeric fields are allowed only where the row status/flag explains why the value is unavailable; no `NaN` or `inf` tokens are emitted.

## normalized_quotes.csv

One row per raw quote:

```text
quote_id,effective_date,expiry_date,time_to_expiry,forward,discount_factor,strike,option_type,bid_iv,ask_iv,mid_iv,included,filter_reason
```

`option_type` is `C` or `P`; IVs are decimal volatilities (not percentage points). Excluded values remain present with empty numeric fields and `included=false`.

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

Exactly one row is emitted per fit. `converged` is `true` only when the requested constrained
solver completed and all safety checks passed. A failed fit emits `converged=false` and a
non-empty `error`; it never falls back to an unconstrained result. `runtime_seconds` is wall
clock time and is indicative only across languages.

The legacy CVI notebook additionally emits `benchmark_output/parameters.csv` with this
notebook-specific schema (its CVI fitting behavior is unchanged):

```text
time_to_expiry,atm_total_variance,observed_min_log_moneyness,observed_max_log_moneyness,grid_points,fit_rows
```

This is a parameter summary, not an eSSVI `(theta, psi, rho, phi)` file. The shared adapter
retains the eSSVI parameter schema separately and joins both models only through quote IDs and
the dense-grid contract.
