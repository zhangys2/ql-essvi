# eSSVI parameterization and constraints

Let `k = log(K/F(T))`, `theta(T)` be forward ATM total variance, `psi(T)=theta(T)phi(T)`, and `rho(T) in (-1,1)`:

```text
w(k,T) = 1/2 * (theta + rho*psi*k
                + sqrt((psi*k + theta*rho)^2 + theta^2*(1-rho^2)))
```

The implementation uses `phi(theta)=eta*theta^(-lambda)` with `eta>0` and `0<=lambda<=1/2`. It uses the exact bounded smooth correlation family
`rho(theta) = -0.98 + 1.96*sigmoid(a + b*log(theta))`, with `a in [-5,5]` and `b in [-2,2]`; this guarantees `-0.98 < rho < 0.98`. Constant rho reduces eSSVI to SSVI; the current calibration family is the eSSVI extension.

For numerical safety, calibration enforces strict versions of the following sufficient conditions:

- `theta > 0`, `psi > 0`, `abs(rho) < 1`
- butterfly (Gatheral--Jacquier sufficient bound):
  `psi*(1+abs(rho)) < 4` and `psi^2*(1+abs(rho)) < 4*theta`
- adjacent calendar slices, ordered by expiry:
  `theta_2 > theta_1`, `psi_2 > psi_1`, and
  `psi_2 > psi_1*max((1+rho_1)/(1+rho_2), (1-rho_1)/(1-rho_2))`

The scorer independently checks the generated surface on a dense grid from `k=-0.5` to `k=0.5` at 0.01 increments per expiry. Points outside the observed included quote range are marked `is_tail=true`; all points are included in arbitrage checks, while quote containment uses only observed quotes. The MM necessary-and-sufficient butterfly infimum is not used; this is explicitly a conservative sufficient-condition benchmark.

## Enforcement and independent checks

| Property | Calibration enforcement | Post-fit check |
|---|---|---|
| `theta > 0`, `0 <= lambda <= 1/2`, `eta > 0`, `-5 <= a <= 5`, `-2 <= b <= 2` | NLopt bounds | parameter CSV and diagnostics |
| bounded smooth `rho(theta)` family | derived from bounded `a,b` in every objective/constraint evaluation | `abs(rho) < 1` |
| GJ butterfly sufficient bounds | hard SLSQP inequalities with strict epsilon margin | dense-grid butterfly scan |
| HM/Corbetta calendar sufficient conditions | hard adjacent-slice SLSQP inequalities with strict epsilon margin | dense-grid calendar scan |
| solver failure | no unconstrained fallback; `converged=false` and error are emitted | diagnostics CSV |
| tail safety | dense grid extends to `k in [-0.5,0.5]`; tail points are retained | all grid points included in arbitrage checks |

Missing or non-finite quote inputs are excluded by the shared Python filter and retain an
empty numeric field plus `included=false` and `filter_reason`. Excluded quotes are never
silently scored as fitted values.

References:

- Gatheral & Jacquier, *Arbitrage-free SVI volatility surfaces*, https://arxiv.org/abs/1204.0646.
- Corbetta, Cohort, Laachir & Martini, *Robust calibration and arbitrage-free interpolation of SSVI slices*, https://arxiv.org/abs/1804.04924.
- Hendriks & Martini, *The Extended SSVI Volatility Surface*, https://papers.ssrn.com/sol3/papers.cfm?abstract_id=3351795.
- Mingone et al., *No arbitrage global parametrization for the eSSVI volatility surface*, https://arxiv.org/abs/2204.00312.
