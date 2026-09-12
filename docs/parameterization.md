# eSSVI parameterization and constraints

Let `k = log(K/F(T))`, `theta(T)` be forward ATM total variance, `psi(T)=theta(T)phi(T)`, and `rho(T) in (-1,1)`:

```text
w(k,T) = 1/2 * (theta + rho*psi*k
                + sqrt((psi*k + theta*rho)^2 + theta^2*(1-rho^2)))
```

The implementation uses `phi(theta)=eta*theta^(-lambda)` with `eta>0` and `0<=lambda<=1/2`, and a bounded smooth `rho(theta)` parameterization. Constant rho reduces eSSVI to SSVI.

For numerical safety, calibration enforces strict versions of the following sufficient conditions:

- `theta > 0`, `psi > 0`, `abs(rho) < 1`
- butterfly (Gatheral--Jacquier sufficient bound):
  `psi*(1+abs(rho)) < 4` and `psi^2*(1+abs(rho)) < 4*theta`
- adjacent calendar slices, ordered by expiry:
  `theta_2 > theta_1`, `psi_2 > psi_1`, and
  `psi_2 > psi_1*max((1+rho_1)/(1+rho_2), (1-rho_1)/(1-rho_2))`

The scorer independently checks the generated surface on a dense grid. The MM necessary-and-sufficient butterfly infimum is not used in the first implementation; this is explicitly a conservative sufficient-condition benchmark, not a claim of exact MM enforcement.

References:

- Gatheral & Jacquier, *Arbitrage-free SVI volatility surfaces*, arXiv:1204.0646.
- Corbetta, Cohort, Laachir & Martini, *Robust calibration and arbitrage-free interpolation of SSVI slices*, arXiv:1804.04924.
- Hendriks & Martini, *The Extended SSVI Volatility Surface*.
- Mingone et al., *No arbitrage global parametrization for the eSSVI volatility surface*, arXiv:2204.00312.
