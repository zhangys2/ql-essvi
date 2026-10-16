# ql-essvi Calibrator Speedup — Addendum A (warm update under market-like moves)

Extends [[2026-10-15-ql-essvi-calibrator-speedup-design]] (the "parent spec", `docs/calibrator-speedup-spec.md` in the repo). Written after an independent check of the implementation on vps1, 2026-10-15.

## 1. Why this addendum exists

The implementation passes its own tests and meets the cold-path targets, but the warm-update result in `docs/perf-results.md` (p99 2.9 ms) comes from a test where every quote moves by 1e-7 against a reuse tolerance of 1e-5, so all 40 slices are simply reused. Under market-like moves the warm path is far from the 3 ms target.

## 2. What was verified independently

Fresh out-of-tree build of branch `perf/essvi-calibrator` (working tree, uncommitted) in `/tmp/essvi_verify/build`, Release `-O2`, GCC 15.2, core library without QuantLib.

| Check | Result |
|---|---|
| ctest (4 tests: core, MM bound 1e6 samples, cold regression, warm calibrator) | 4/4 passed |
| `essvi_bench` 40 x 100, cold | first call 56.2 ms, repeat median 49.1 ms, p99 57.5 ms |
| `essvi_bench` warm (1e-7 moves) | median 0.40 ms, p99 1.78 ms, 40/40 slices reused |
| Cold full chain vs historical baseline (1.259 s) | about 22x on this run (repeat median) |

The cold-path claims hold. Load average was 2.3 to 5.3 on 4 vCPUs during these runs (collectors active), so absolute timings carry that noise.

## 3. Market-like warm test (reference implementation)

Source: `/tmp/essvi_verify/realistic_warm.cpp` on vps1 and `scratch/realistic_warm.cpp` in the vault. It builds a 40 expiry x 100 quote chain with a valid term structure (rho varying from -0.5 to -0.3 across expiries), band half-spread `0.003 (1 + 2|k|)` in IV, then evolves it for 200 updates with a common level factor, a common tilt factor, a small idiosyncratic part per expiry, and per-quote noise. For each update it times `calibrateWarm()` and, for comparison, `calibrate()` on the same quotes. Default `WarmOptions`.

An earlier version moved each expiry independently; that made the term structure arbitrage-prone and was discarded. Results below are from the corrected generator.

| Scenario | Warm median | Warm p99 | Cold median | Slices refit (mean) | Fallbacks | Warm failures | Invalid surfaces |
|---|---:|---:|---:|---:|---:|---:|---:|
| quiet: 10% of expiries move 0.05 vol pt | 3.6 ms | 17.6 ms | 65.5 ms | 4.7 of 40 | 4 / 200 | 0 | 0 |
| normal: all expiries, 0.2 vol pt + noise | 21.3 ms | 52.8 ms | 71.9 ms | 40 of 40 | 0 / 200 | 0 | 0 |
| fast: all expiries, 1 vol pt + tilt | 25.4 ms | 168.7 ms | 68.2 ms | 40 of 40 | 79 / 200 | 0 | 0 |
| front: front quarter 0.4 vol pt, back 0.05 | 11.7 ms | 29.3 ms | 66.9 ms | 40 of 40 | 0 / 200 | 0 | 0 |

Quality: summed band loss warm versus cold was within about 5% in every scenario (for example normal: 2.53e-2 vs 2.39e-2 over 200 updates), worst per-update gap 8.5e-5 (normal) and 3.7e-3 (fast). No unconverged or invalid surfaces.

Reading: warm is correct and 3 to 6x faster than cold, but a normal 5-minute-style update that changes every slice costs about 0.5 ms per refit slice (21 ms / 40), against a target of about 75 microseconds per slice for 3 ms in total. The `fast` scenario falls back to the cold slice search for 40% of updates, which also produces the 169 ms tail.

## 4. Revised acceptance criteria (replace parent spec section 1 warm row)

| Scenario (see section 3) | Target p99 | Hard limit p99 | Other |
|---|---:|---:|---|
| quiet | <= 3 ms | <= 10 ms | |
| normal | <= 3 ms | <= 10 ms | fallback rate <= 1% |
| front | <= 3 ms | <= 10 ms | |
| fast | <= 10 ms | <= 20 ms | fallback rate <= 10% |

For every scenario: zero invalid surfaces, zero warm failures, summed band loss within 5% of cold, and the parent spec's dense-grid arbitrage scan clean. A result that misses a target is reported as missed.

## 5. Work packages

### WP6 - Make the scenarios part of the repo bench
- Port the generator and the four scenarios from `realistic_warm.cpp` into `bench/` (flag such as `--scenario quiet|normal|fast|front|all`), with seeded randomness and the same output metrics (median, p90, p99, max for warm and cold; refit/reuse means; path counts; failures; band-loss sums).
- Add a replay mode that feeds consecutive IBIT snapshots from `~/vol-rv/data/ibit/` (converted to `normalized_quotes.csv`) through `calibrateWarm()`; label Black-Scholes-inverted IVs as a stand-in for American IVs.
- Report the load average before and after each run. The collectors run every 5 minutes (Deribit all day, IB during US hours), so for clean numbers use a quiet minute or state the load.

### WP7 - Cheap "still fits" reuse test
- Before any search on a changed slice, evaluate the band loss of the previous slice parameters on the new quotes (one pass, a few microseconds). If the loss is at most `loss_tol` and `pairSafe` holds against the (possibly updated) predecessor, keep the previous parameters.
- Open design question (section 6, question 1): staying inside the band is what the objective asks for, but a surface that drifts toward a band edge is a worse quote surface for market making than one centered on the mid.

### WP8 - Local solver for changed slices
- Replace the local grid-plus-golden-section refit with a few iterations of a Gauss-Newton or Levenberg-Marquardt step on `(theta, rho, psi)`. The residuals are the squared-hinge band excesses, which are continuously differentiable; the partial derivatives of `w` with respect to the three parameters are closed-form (see `docs/parameterization.md` and the eSSVI derivative formulas).
- Constraints: clip steps into the feasible box from the current predecessor (`psiLower`, `psiUpper`, `|rho| < 0.98`, calendar conditions) and backtrack to feasibility; never accept an infeasible point.
- Budget: at most about 15 objective evaluations per slice, and `mmPsi2Bound` called once per accepted iterate, not per trial. Report objective evaluations and `mmPsi2Bound` calls per refit slice for the warm path (today only cold totals are reported).
- Keep the periodic cold check, and keep the cold slice search as the fallback.

### WP9 - Fallback gate
- The `fast` scenario falls back 40% of the time. Find out why (log the gate inputs: warm loss, reference loss, `loss_tol`, scale of the loss), and make the gate relative to the loss scale and cheap. A fallback should be rare and visible in the result (`path`, counts).
- Target fallback rates are in section 4.

### WP10 - Optional: parallel refit with repair
- If WP7-WP9 still miss the target: refit changed slices in parallel against the previous snapshot's neighbors (Jacobi-style), then run one sequential pass that checks `calendarSafe` between adjacent slices and refits only the violating ones. Deterministic reduction. At most 4 threads, shared with the collectors.

### WP11 - Remaining items from the parent spec
- Single-slice cold latency is 2.3 ms against the 1 ms target (hard limit 5 ms met). About 73% of the remaining cold time is in the MM-bound and search-hint calls (98,620 calls on the 40 x 100 chain). Reduce the hint cost or replace the hint with an equivalent cheaper envelope, keeping cold outputs within the parent spec's tolerance.
- Commit the work: the branch `perf/essvi-calibrator` currently has no commits (tracked files modified, `bench/` and the docs untracked, `tests/baseline_essvi_40x100.csv` untracked). Group into logical commits and open the PR with the filled-in tables.

## 6. Open questions for Yuan

1. For market making, should the surface be centered on the mid when the band loss is zero (a secondary objective that pulls toward the mid), or is any in-band surface acceptable? This changes WP7 and the objective.
2. Are the scenario sizes in section 3 representative? The real distribution of 5-minute IV moves for SPY/IBIT can be measured from the collected snapshots and used to set them.
3. Should the warm API also accept ticks that arrive faster than 5 minutes (sub-second updates)? That would make "quiet" the dominant case.

## 7. Process notes

- Do not edit existing tests' expectations; if one seems wrong, name the misread requirement and change it as its own step.
- No change to D17-D22 semantics or the CSV contract headers.
- Branch stays `perf/essvi-calibrator`; no force-push.
