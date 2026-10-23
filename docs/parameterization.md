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
adjacent Mingone interval. **D20** (2026-09-17; [T11](../.wayfinder/tickets/T11-mingone-global.md)):
the joint product-of-intervals solver is retired; sequential never calendar-locked on this
snapshot. D17's bid/ask band objective in total variance is unchanged.

Surface validity and independent post-fit checks enforce the strict conditions below. Calibration may prefer the CM1 margin on top of the strict adjacent-slice condition.

- `theta > 0`, `psi > 0`, `abs(rho) < 1`
- butterfly (Martini--Mingone N&S, **D21**; [T10](../.wayfinder/tickets/T10-mm-butterfly.md)):
  `psi*(1+abs(rho)) < 4` (Roger-Lee) and `psi^2 <= f_MM(theta, |rho|)`, which replaces
  Gatheral--Jacquier `psi^2*(1+abs(rho)) < 4*theta`
- adjacent calendar slices, ordered by expiry:
  `theta_2 > theta_1`, `psi_2 > psi_1`,
  `psi_2 > psi_1*max((1+rho_1)/(1+rho_2), (1-rho_1)/(1-rho_2))`,
  and `psi_2 < psi_1/theta_1 * theta_2` (Mingone sufficient upper bound; without it this
  snapshot produced dense-grid calendar violations even when the T01 lower bounds held).
  Calibration fits this strict floor first. If the winner already clears a 0.5% proportional
  margin on the psi-ratio lower bound, it is kept unchanged; otherwise calibration retries with
  that margin and retains the strict winner if no headroom-safe fit converges. `validate()` and
  dense-grid scans enforce the original strict conditions above.

The scorer independently checks the generated surface on a dense grid from `k=-0.5` to `k=0.5`
at 0.01 increments per expiry. Points outside the observed included quote range are marked
`is_tail=true`; all points are included in arbitrage checks, while quote containment uses only
observed quotes. **D19:** exact in-band fraction uses a `1e-12` tolerance; models are ranked by
mean/median IV distance to the nearer bid/ask edge (zero if inside). Butterfly constraints use
the Martini--Mingone infimum (D21), independently checked by the dense-grid scan.

## Enforcement and independent checks

| Property | Calibration enforcement (D18) | Post-fit check |
|---|---|---|
| `theta > 0`, `psi > 0`, `abs(rho) < 1` | per-slice domain | parameter CSV and diagnostics |
| MM butterfly N&S infimum (D21) | hard Roger-Lee plus `psi^2 <= f_MM`, per slice | dense-grid butterfly scan |
| HM/Corbetta calendar sufficient conditions | strict-epsilon floor first; keep a strict-fit winner that already clears CM1's 0.5% proportional psi-ratio margin, otherwise retry with that margin and retain the strict winner if no headroom-safe fit converges | `validate()` and dense-grid scan check the original strict conditions |
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
