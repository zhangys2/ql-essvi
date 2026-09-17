# eSSVI vs CVI benchmark

## Destination

Establish, through resolved decisions and a concrete implementation plan, how to build an
**eSSVI** implied-volatility surface fitter (SSVI as the constant-rho special case) as a
standalone C++ project linking QuantLib, so it can be benchmarked against the existing Python
**CVI** (Convex Volatility Interpolation) fitter on `option_chain.csv`, judged by bid-ask
containment and arbitrage-violation counts.

Complete when every decision ticket is closed and the implementation plan is written.
**No production code** — building is authorised separately.

## Notes

Charter decisions, agreed by grilling on 2026-09-12. These are not tickets; do not re-litigate
without saying so explicitly.

- **D1** Deliverable is a standalone C++ project linking QuantLib — not an upstream
  contribution, not a patch on the `lballabio/QuantLib` clone.
- **D2** Purpose is arbitrage-free fitting of listed index option surfaces.
- **D3** Role is to benchmark against the existing CVI fitter, not to replace it.
- **D4** Comparison happens through a shared CSV contract, quality-first. Cross-language
  calibration timing is reported as indicative only, never decisive.
- **D5** Target eSSVI, implemented so constant rho recovers plain SSVI.
- **D6** Primary metric: fraction of quotes whose fitted IV lies inside the bid/ask IV band.
  Secondary: butterfly and calendar violations on a dense grid, including the tails.
  **D19** (2026-09-17): exact `1e-12` containment stays the in-band count; models are ranked by
  mean/median IV distance to the band. See [T09](../tickets/T09-d6-metric.md).
- **D7** Code and this map live together in `~/source/repos/essvi-bench`.
- **D8** Gate is decisions plus plan; no code.

Environment facts verified 2026-09-12:

- QuantLib is consumable from WSL without installing: `~/QuantLib/build/release/cmake/QuantLibConfig.cmake`,
  plus `libQuantLib.so.1.44.0` and `quantlib-config`. Windows has **no** built QuantLib library.
- QuantLib already ships raw per-slice SVI in `ql/experimental/volatility/`
  (`SviSmileSection`, `sviTotalVariance`, `SviInterpolation` on the XABR machinery). It has
  **no** SSVI or eSSVI.
- CVI source of truth is `~/source/repos/CVI-Vol-Surface-Fitting` (public GitHub repo,
  `zhangys2/CVI-Vol-Surface-Fitting`). The fitter lives in the **notebook**, not in a module;
  `vol_surface_cuosqp.py` is only a cvxpy/cuOSQP solver harness.
- The notebook has **no export calls**, but does contain reusable scaffolding: `EstimateForwards`,
  `filterData`, `black_model`, `CalculatePDF`, `NoButterflyArbitrageCDF`/`PDF`,
  `NoCalendarSpreadArbitrage`, `PositiveVariance`, `UpwardSlopingWings`, `RogerLeeMoment`,
  `LinearWingExtrapolation`.
- Data: `option_chain.csv` is a **single snapshot** — 1 effective date (2024-11-11), 51 expiries,
  14,364 rows, columns `Effective Date, Expiry Date, Last Trade Date, Option Type, Strike, Bid, Ask`.
  No implied vols, no forward, no discount factors. `sofrDFs.csv` is `T,DF` with 16 rows.

Skills: use `grilling` for HITL tickets; use `domain-modeling` only if terminology becomes
canonical or a hard-to-reverse architectural choice needs an ADR.

Execution exception: **none**. This map plans decisions only.

## Decisions so far

- **D9** Build and run the C++ project on WSL Ubuntu/GCC, using the existing QuantLib build tree.
- **D10** Python owns the canonical preprocessing and scoring pipeline; C++ consumes the normalized CSV contract.
- **D11** Add a minimal, additive CVI export adapter without changing CVI fitting behavior.
- **D12** Implement a plain eSSVI evaluator first; add a QuantLib surface adapter after calibration and scoring are validated. Adapter implemented 2026-09-17: `essvi::EssviBlackVarianceSurface` and `essvi::EssviSmileSection` (see [T02](../tickets/T02-quantlib-consumption.md)).

These decisions were confirmed by the user on 2026-09-12. Additional confirmed implementation decisions:

- **D13** Use power-law `phi`, smooth low-dimensional maturity-dependent `rho`, hard constraints during calibration, and jointly fitted `theta_t` initialized from ATM data.
- **D14** Use NLopt SLSQP, fail explicitly rather than falling back to unconstrained fitting, and use total variance canonically with IV as a reporting field.
- **D15** Use deterministic shared filtering, long-form CSV, a common dense grid with tail-only arbitrage checks, and empty fields plus status/reason columns for missing values.
- **D16** Add the CVI export cell directly to the existing notebook; the user authorized modifying, committing, and pushing that repository.
- **D17** Calibrate eSSVI against the bid/ask band in total variance rather than mid IV, starting each `theta_t` from its nearest-ATM quote (confirmed 2026-09-16; see [Choose the eSSVI calibration approach](../tickets/T05-calibration-approach.md)).
- **D18** Calibrate eSSVI as free `(θ_t, ρ_t, ψ_t)` per expiry under the T01 constraints plus Mingone's sufficient calendar upper bound on `ψ`, sequentially (Corbetta). This supersedes D13's 4-global `φ`/`ρ` family (see [Decide whether the eSSVI family has enough capacity for this chain](../tickets/T08-essvi-model-capacity.md)).
- **D19** Keep D6's exact `1e-12` in-band fraction; rank models by mean and median IV distance to the bid/ask band (see [Revisit D6 so the primary metric measures the model on this chain](../tickets/T09-d6-metric.md)).

The exact sufficient conditions and schemas are documented in `docs/parameterization.md` (D18 form; T01 inequalities plus Mingone's calendar `ψ` upper bound) and `docs/csv-contract.md`. The CVI/eSSVI included-quote count difference remains a residual fairness gap.

## Not yet specified

- None. Tail policy, filtering, common-quote intersection, single-snapshot interpretation, and
  plain-grid-first exposure are documented in `docs/parameterization.md`, `docs/csv-contract.md`,
  and `benchmark_report.md`.

## Out of scope

- Replacing CVI in production — D3 fixed the role as benchmark only.
- Integration with the `localvol` Dupire engine — a separate effort.
- GPU acceleration of eSSVI calibration.
- Upstreaming anything to `lballabio/QuantLib`.
- Raw per-slice SVI as a third baseline — excluded when D5 chose eSSVI with SSVI as its special case.
- Calibration speed as a decisive metric — D4 makes cross-language timing indicative only.
