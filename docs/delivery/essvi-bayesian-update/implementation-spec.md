# Constrained Bayesian eSSVI warm update — implementation spec

**Status:** Approved by the user on 2026-10-31; ticket publication and implementation are authorized under the delivery workflow.

**Intake:** Decision-only, from the user request and the selected approach in this conversation.

**Base commit:** `497f0c37d27056ec2e6df631c0f570d80fe332f4`.

## Source handoff

The user requested: “wayfinder implement a constrained Bayesian state update.” When asked which observation model to use, the user chose: “Preserve band loss.”

## Problem and outcome

`calibrateWarm` uses the previous surface as a search start or reuse candidate, but `CalibrationResult` carries no posterior uncertainty. The requested outcome is a sequential state update in which the previous fitted surface and its uncertainty form a prior, the current quote snapshot supplies the new evidence, and the reported eSSVI surface is the constrained posterior mode.

## Scope and non-goals

### In scope

- Carry per-expiry posterior covariance in transformed eSSVI coordinates with `CalibrationResult`, aligned to `surface.slices`.
- Predict the previous covariance with per-update process noise; the existing API has no snapshot timestamp.
- Use the complete current quote snapshot once per update. Preserve the current weighted total-variance bid/ask band loss as a generalized-Bayes likelihood, normalized by its existing `bandLossScale`.
- Select the posterior mode subject to the existing hard butterfly and adjacent-expiry calendar conditions. Keep the existing quote-quality gates and cold/restricted fallback behavior.
- Initialize covariance for cold fits, propagate it through warm/fallback results, expose it read-only in the Python result, and document its meaning and limits.
- Record the separately authorized implementation work in the existing Wayfinder map after spec approval.

### Non-goals

- Replacing the D17 band-loss target with mid- or band-center fitting, or claiming a generative quote-noise model.
- Changing the eSSVI parameterization, no-arbitrage conditions, quality gates, fallback policy, CSV schemas, quote preprocessing, CVI, or QuantLib.
- Matching expiries by a new calendar-date field. Preserve the existing exact `expiry` key; an unmatched expiry starts cold.
- Adding snapshot timestamps or treating `previous_quotes` as a second likelihood. They remain for identity/reuse detection only.

## Evidence and constraints

### Repository facts

- The parameterization documentation fixes D17’s bid/ask band objective in total variance and the D21 butterfly plus calendar conditions (`docs/parameterization.md:15-35`; `.wayfinder/tickets/T05-calibration-approach.md:43-58`).
- `calibrateWarm` already distinguishes prior-snapshot warm starts, unchanged-quote reuse, local fits, and cold fallback; all accepted surfaces pass existing feasibility validation (`cpp/calibrator.cpp:1076-1079, 1212-1244, 1315-1327, 1400-1603`).
- `CalibrationResult` stores the surface and reuse/degradation state but no covariance (`cpp/calibrator.hpp:43-79`). Python exposes the native result read-only (`cpp/python_bindings.cpp:98-132`).
- Quote expiry is represented as a year fraction, and existing warm-state lookup uses exact equality (`docs/csv-contract.md:3,13`; `cpp/calibrator.cpp:221-224`).
- The delivery tracker is local, uses one primary-checkout branch per ticket, and requires baseline, TDD, full verification, review, and a local commit (`docs/delivery/DELIVERY.md:3-29`).

### User decisions

- **DEC-001 — Preserve the existing band loss as the generalized-Bayes quote likelihood.** Do not silently replace D17 with a new interval-noise model.
- **DEC-002 — Add a constrained Bayesian state update to the warm calibrator.** The returned surface is the posterior mode and must satisfy existing hard no-arbitrage constraints.

### Approved implementation inferences

- Use `z=(log(theta), atanh(rho), log(psi))` as the state coordinates. The prior is Gaussian in `z`; the feasible support remains defined by the existing constraints in surface coordinates.
- For each expiry, update with the complete current snapshot once and do not append the previous snapshot’s quote likelihood. Treat each `current_quotes` call as a fresh observation; repeated snapshots may therefore sharpen uncertainty under this conditional-independence assumption.
- Use the existing normalized band energy `bandLoss / bandLossScale` in the generalized likelihood. Select the mode with the sum of that data energy and the Gaussian prior quadratic. Keep `CalibrationResult::objective` as the existing raw band-loss diagnostic for compatibility.
- Carry a symmetric positive-definite covariance approximation in transformed coordinates. Estimate data information from active out-of-band quote residuals using a positive-semidefinite Gauss-Newton approximation; use a documented diagonal variance floor for cold-start or weakly identified slices. Add per-update process noise before each warm update. Set defaults from the available snapshot replay, without tuning on the held-out portion.
- Do not use the posterior mean as the surface: it can violate the constrained feasible set. Apply existing validation and quality gates to the posterior mode; preserve cold fallback when the warm candidate fails.
- If no usable prior result or no exact-expiry match exists, cold-start that slice. If the prior covariance is absent, malformed, or not positive definite, do not use it as a Bayesian prior; use the existing cold-start path.

## Proposed seam and API

Keep the state update in `cpp/calibrator.cpp` behind the existing `calibrateWarm` API. Add compact per-slice covariance state to `CalibrationResult` in `cpp/calibrator.hpp`; expose read-only covariance snapshots through `cpp/python_bindings.cpp`. Extend `WarmOptions` only if replay calibration requires a public process-noise or initialization control. Keep candidate feasibility checks in the existing `fitSlice`/`calibrateWarm` path; no soft no-arbitrage penalties.

The seam catches state propagation, prior influence, feasibility, and fallback regressions through the public warm API. It does not establish that the generalized likelihood is a calibrated market-noise model; that is explicitly not claimed.

## Acceptance criteria

- A warm update uses the previous result’s mean and covariance as the prior and the current quote snapshot as the sole new quote evidence.
- The MAP changes when the prior changes under the same informative current quotes; it is not merely a warm start for the raw band-loss minimum.
- Cold initialization and every successful warm/fallback result have finite symmetric positive-definite covariance aligned one-to-one with surface slices.
- Every returned successful surface passes `validate()` and the existing dense-grid butterfly/calendar checks. Existing quote-quality gates and fallback contracts remain active.
- Missing/unusable priors and unmatched expiries take the documented cold path without returning an invalid posterior state.
- Passing a Python `CalibrationResult` to the next `calibrate_warm` call preserves its native covariance state; Python inspection returns read-only snapshots.
- Existing raw objective, diagnostics, and CSV contracts remain unchanged. README and parameterization documentation state the generalized-Bayes interpretation, covariance approximation, process-noise cadence, and limits.
- A held-out snapshot replay reports one-step-ahead band loss, surface movement, feasibility, fallback rate, and latency against the existing warm and cold paths. Do not claim predictive improvement unless the measured holdout supports it.

## Verification strategy

1. Record and pass the configured baseline at this ticket’s declared base commit before editing.
2. Add public-seam C++ regressions for prior influence, covariance initialization/propagation, full-snapshot updates, exact-expiry cold starts, hard feasibility, and unchanged fallback behavior; demonstrate the focused test fails before production changes.
3. Add Python binding coverage for read-only covariance state and consecutive-result reuse.
4. Run focused warm CTest and Python binding tests, then the configured full command from `docs/delivery/DELIVERY.md`.
5. Run a bounded warm-scenario or real snapshot replay with a held-out next-snapshot score; report actual results without weakening existing thresholds.

## Risks and limits

- A flat band likelihood is weakly identifying inside quoted bands; posterior uncertainty there is prior- and floor-dominated.
- A local Gauss-Newton covariance ignores higher-order curvature and truncation by active arbitrage constraints. Hard constraints still govern every reported mode; covariance is a local approximation, not a guarantee that Gaussian draws are feasible.
- The generalized-Bayes temperature and per-update process noise affect the prior/data balance. Calibrate on training snapshots and report held-out performance.
- Repeated unchanged quote values are treated as fresh observations under the stated model; if snapshots are cached rather than re-observed, this can overstate confidence.
- Exact expiry matching means a changed time-to-expiry does not inherit the old posterior; it cold-starts rather than guessing a cross-expiry mapping.

## Open decisions

None. The user approved the implementation inferences above as part of this spec on 2026-10-31.
