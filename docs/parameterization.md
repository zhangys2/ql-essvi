# eSSVI parameterization and constraints

Let `k = log(K/F(T))`, `theta(T)` be forward ATM total variance, `psi(T)=theta(T)phi(T)`, and `rho(T) in (-1,1)`:

```text
w(k,T) = 1/2 * (theta + rho*psi*k
                + sqrt((psi*k + theta*rho)^2 + theta^2*(1-rho^2)))
```

**D18** (2026-09-16; [T08](../.wayfinder/tickets/T08-essvi-model-capacity.md)): each listed expiry has its own
`(theta_t, rho_t, psi_t)`, subject only to the sufficient conditions below. Constant `rho` reduces
eSSVI to SSVI (D5). There is no shared `phi` or `rho` curve across expiries. This supersedes D13's
power-law `phi(theta)=eta*theta^(-lambda)` and `rho(T)=-0.98+1.96*sigmoid(a+b log T)`.

The checked-in calibrator implements D18: slices are fitted sequentially going forward
(Corbetta et al.), anchoring `theta` near ATM, sampling `rho`, and searching `psi` inside the
T01 bounds. Mingone's product-of-intervals reparameterization is the same 3N family and remains
the D14 fallback if sequential calendar locking appears. D17's bid/ask band objective in total
variance is unchanged.

For numerical safety, calibration enforces strict versions of the following sufficient conditions:

- `theta > 0`, `psi > 0`, `abs(rho) < 1`
- butterfly (Gatheral--Jacquier sufficient bound):
  `psi*(1+abs(rho)) < 4` and `psi^2*(1+abs(rho)) < 4*theta`
- adjacent calendar slices, ordered by expiry:
  `theta_2 > theta_1`, `psi_2 > psi_1`,
  `psi_2 > psi_1*max((1+rho_1)/(1+rho_2), (1-rho_1)/(1-rho_2))`,
  and `psi_2 < psi_1/theta_1 * theta_2` (Mingone sufficient upper bound; without it this
  snapshot produced dense-grid calendar violations even when the T01 lower bounds held)

The scorer independently checks the generated surface on a dense grid from `k=-0.5` to `k=0.5`
at 0.01 increments per expiry. Points outside the observed included quote range are marked
`is_tail=true`; all points are included in arbitrage checks, while quote containment uses only
observed quotes. The MM necessary-and-sufficient butterfly infimum is not used; this is
explicitly a conservative sufficient-condition benchmark.

## Enforcement and independent checks

| Property | Calibration enforcement (D18) | Post-fit check |
|---|---|---|
| `theta > 0`, `psi > 0`, `abs(rho) < 1` | per-slice domain | parameter CSV and diagnostics |
| GJ butterfly sufficient bounds | hard inequalities with strict epsilon margin, per slice | dense-grid butterfly scan |
| HM/Corbetta calendar sufficient conditions | hard adjacent-slice inequalities with strict epsilon margin (sequential lower bounds, or Mingone interval encoding) | dense-grid calendar scan |
| solver failure | no unconstrained fallback; `converged=false` and error are emitted | diagnostics CSV |
| tail safety | dense grid extends to `k in [-0.5,0.5]`; tail points are retained | all grid points included in arbitrage checks |

The D13 NLopt box bounds on `(eta, lambda, a, b)` do not apply under D18.

Missing or non-finite quote inputs are excluded by the shared Python filter and retain an
empty numeric field plus `included=false` and `filter_reason`. Excluded quotes are never
silently scored as fitted values.

References:

- Gatheral & Jacquier, *Arbitrage-free SVI volatility surfaces*, https://arxiv.org/abs/1204.0646.
- Corbetta, Cohort, Laachir & Martini, *Robust calibration and arbitrage-free interpolation of SSVI slices*, https://arxiv.org/abs/1804.04924.
- Hendriks & Martini, *The Extended SSVI Volatility Surface*, https://papers.ssrn.com/sol3/papers.cfm?abstract_id=3351795.
- Mingone, *No arbitrage global parametrization for the eSSVI volatility surface*, https://arxiv.org/abs/2204.00312.
