# Shared CSV contract

All files are UTF-8 CSV, comma-delimited, with a header and decimal floating-point values. Dates use `YYYY-MM-DD`; times are year fractions; variance and `psi` are dimensionless.

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
time_to_expiry,log_moneyness,strike,fitted_total_variance,fitted_iv,is_tail
```

## parameters.csv

```text
time_to_expiry,theta,psi,rho,phi,butterfly_safe,calendar_safe
```

## diagnostics.csv

```text
model,converged,objective,iterations,runtime_seconds,error
```
