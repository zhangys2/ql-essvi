---
id: T08
title: Decide whether the eSSVI family has enough capacity for this chain
type: Research (AFK)
status: open
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

Why does the calibrated eSSVI surface keep only about 1% of included quotes inside their bid/ask
band, and which eSSVI form should the benchmark calibrate so that D6's primary metric measures the
model rather than an over-restricted parameterization?

## Why it matters

After D17, when eSSVI is calibrated directly against the bid/ask bands, it contains 88 of 7,740
included quotes (1.1370%), with zero arbitrage violations. On the 7,594 quotes both models fit, the
figures are 1.1193% for eSSVI and 0.2370% for CVI. At these levels neither fitter is close to the
market, so "eSSVI wins on containment" says little. The calibrated family (D13) has 4 global
parameters (`eta`, `lambda`, `a`, `b`) shared across 51 expiries, plus one `theta_t` per expiry.
Every smile shape therefore has to come from those 4 global parameters.

## Scope

- **Diagnose where the misfit sits:**
  - by expiry (the 7-day expiry has about 116% near-ATM IV), moneyness and band width
  - whether it is structural (the model cannot bend enough) or an optimisation failure (SLSQP local
    minimum or starting point)
- **Candidate forms, all still eSSVI or SSVI per D5 (raw per-slice SVI stays out of scope):**
  - free `rho_t` and `psi_t` per slice under the T01 butterfly and calendar conditions
    (Corbetta et al. slice-by-slice)
  - richer global `phi` or `rho` families
  - the arbitrage-free global parameterization of Mingone et al.
- **Consequences:**
  - which decisions each candidate reopens (D13, D14, D17)
  - parameter count against 7,740 quotes
  - calibration robustness and runtime
- **Honest ceiling:** what containment a well-fitted arbitrage-free surface can reach on this
  snapshot, given that containment is scored with a `1e-12` tolerance on tight listed-index bands.

## Done when

The main cause of the low containment is identified with evidence, and one parameterization is
recommended with the decisions it changes and the rejected alternatives recorded. Implementation is
authorised separately.
