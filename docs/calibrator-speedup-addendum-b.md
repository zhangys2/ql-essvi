# ql-essvi Calibrator Speedup — Addendum B (gate quality, bounded fallback)

Extends [[2026-10-15-ql-essvi-calibrator-speedup-addendum]] (Addendum A) and [[2026-10-15-ql-essvi-calibrator-speedup-design]]. Written 2026-10-16 after re-testing commit `49fed2b` ("perf: speed up ESSVI calibration") on vps1. In the repo: `docs/calibrator-speedup-addendum-b.md`.

## 1. Result of the re-test

Fresh out-of-tree build of `49fed2b` in `/tmp/essvi_verify2/build` (Release `-O2`, GCC 15.2), core library without QuantLib. The repo's own tests pass (ctest 4/4). The market-like check is the same program as for Addendum A (`/tmp/essvi_verify/realistic_warm.cpp`, copy in the vault at `scratch/realistic_warm.cpp`): 40 expiries x 100 quotes, 200 updates per scenario, default `WarmOptions`, one run per scenario, load average 1.5 rising to 2.6 (partly the test itself, which runs 4-thread cold fits).

| Scenario | Warm median, before -> after | Warm p99, before -> after | Fallback updates, before -> after | Summed band loss, warm / cold, before -> after |
|---|---|---|---|---|
| quiet | 3.6 -> 0.47 ms | 17.6 -> 7.6 ms | 4 -> 4 / 200 | 1.07 -> 3.9 |
| normal | 21.3 -> 2.3 ms | 52.8 -> 169 ms | 0 -> 49 / 200 | 1.06 -> 5.0 |
| fast | 25.4 -> 30.3 ms | 169 -> 93 ms | 79 -> 147 / 200 | 0.94 -> 1.2 |
| front | 11.7 -> 0.9 ms | 29.3 -> 23.7 ms | 0 -> 48 / 200 | 1.01 -> 20 |

The repo's own scenario report (`docs/perf-results.md`, WP6-WP11 section) agrees in direction: its loss ratios are 1.75 (quiet), 56.8 (normal), 1.03 (fast), 155 (front), and it states the misses.

Also measured: single-slice cold 2.9 ms median, 3.2 ms p99 (earlier 2.3 ms; the repo's own run reports 4.4 ms under higher load); full-chain cold 58 ms median (49 ms earlier the same day). Both may be noise or a regression from the search-hint change; WP-B6 settles it.

Summary: the median warm update is now fast in three of four scenarios, but the p99 targets are missed everywhere, fallbacks are frequent, and fit quality is much worse than the cold search.

## 2. Findings from reading the code (`cpp/calibrator.cpp`)

1. **The quality gate is very loose.** `bandLossScale()` is the sum over quotes of (ask variance - bid variance)^2, i.e. the loss you would get if every quote were missed by one full band width. The gate accepts a warm candidate whose loss is at most `loss_tol * scale` with `loss_tol = 0.05`. If the excess is spread evenly, that allows an RMS miss of about sqrt(0.05) = 22% of the band width at every quote (derived by me, assuming uniform excess). The reuse test in WP7 uses the same tolerance, so a slice can be kept while sitting well outside its bands. This explains the 4x to 20x loss ratios.
2. **Yet fallbacks are frequent.** A cold slice search (about 1.7 to 3 ms each) runs whenever a candidate's loss exceeds that tolerance. 193 of 200 updates in `fast` and 23% to 30% of updates in `normal` and `front` hit it. In `fast` the repo reports 11,678 local attempts and 7,178 fallback slices for 200 updates (about 37 of 40 slices per update), so the local solver is wasted work there, and warm p99 (356 ms in the repo's run) exceeds cold p99.
3. **A fallback is a full cold search of that slice.** One update with many failing slices costs tens of milliseconds to hundreds of milliseconds. There is no bound on the work per update.

Unverified hypotheses for the agent to test: the local solver often stops at a constraint boundary (calendar lower bound against a changed predecessor) or on the rho-psi ridge; the Jacobi prefit against previous-snapshot predecessors fails repair often when many slices move together.

## 3. Revised acceptance criteria (override Addendum A section 4 where they differ)

For each of the four scenarios (quiet, normal, fast, front), seeds fixed, 200 updates, default options, measured outside the collectors' busy windows (US hours 13:35 to 20:00 UTC) with the load average reported:

| Metric | Target | Hard limit |
|---|---|---|
| Per-quote cap: share of quotes with excess ratio above the cap (cap 0.1), warm minus cold | <= 0.5 percentage points | <= 2 percentage points |
| Maximum excess ratio over all quotes, warm vs cold | warm <= max(0.1, cold + 0.01) | warm <= max(0.2, cold + 0.05) |
| RMS excess ratio (secondary) | <= 0.02 | <= 0.05 |
| Summed band loss, warm / cold (secondary) | <= 1.05 | <= 1.20 |
| Per-quote RMS excess outside the band, warm vs cold, in vol points | within +0.02 vol pt of cold | within +0.05 |
| Warm p99, quiet / normal / front | <= 3 ms | <= 10 ms |
| Warm p99, fast | <= 10 ms | <= 20 ms |
| Fallback updates | <= 1% (quiet, normal, front); <= 10% (fast) | 5% / 25% |
| Warm failures, invalid surfaces | 0 | 0 |
| Dense-grid butterfly and calendar violations | 0 | 0 |

Quality is gating, not advisory: a change that improves latency but breaks the per-quote cap rows counts as a miss. Report results as median over 5 seeds with the range.

Priority under the 5-minute cadence (decision 3): `normal`, `front` and `fast` carry the targets above. `quiet` must not get worse than the `49fed2b` numbers (median 0.47 ms, p99 7.6 ms) but has no tuning target. Because the whole chain is re-quoted every 5 minutes, "reuse an unchanged slice" matters less than a good warm start and a cheap, bounded fit of changed slices; a rare full cold refit (about 60 ms) is acceptable in wall-clock terms but must still show up in the fallback-rate rows.

## 4. Work packages

### WP-B1 - Honest quality metrics first (do before changing the solver)
- Add to the bench output, per scenario: warm and cold per-quote RMS and maximum excess outside the band converted to implied-vol points; the per-quote excess ratio statistics defined in WP-B2 (share above cap, maximum, RMS); the loss ratio; and a pass/fail table against section 3 (`--check` returns non-zero on a miss).
- Add `scripts/verify_scenarios.sh` that runs all four scenarios over 5 seeds and prints the table. Commit it with the baseline numbers for `49fed2b`.

### WP-B2 - Per-quote cap gate and reuse test
- Definitions, per included quote with a band: `width = ask_variance - bid_variance` (floored at 1e-6, as today), `excess = max(0, bid_variance - w_fit, w_fit - ask_variance)`, `ratio = excess / width`.
- New gate: a warm candidate slice is accepted only if `max ratio <= cap` over its quotes, with `cap = 0.1` by default (configurable `WarmOptions::quote_cap`). Secondary reported metric and target: RMS ratio <= 0.02.
- Infeasible quotes: a single stale, crossed or mispriced quote can exceed the cap even for the best feasible fit. Handle it so the gate cannot force endless fallbacks: the gate compares against `max(cap, cold_ratio_for_that_quote + 0.01)` where `cold_ratio` comes from the cold search on that slice (computed only when the gate fails, then cached for that snapshot). Also confirm the Python preparation filter removes crossed and locked quotes and report how many it removes per snapshot.
- Apply the same per-quote cap to the reuse test (WP7), and add a drift limit: a slice's parameters may be reused unchanged for at most N consecutive updates (default 12) even if still within the cap.
- Keep the old `loss_tol` field for compatibility but mark it deprecated; the bench reports the old and new gates side by side for `49fed2b`.
- Expect fallbacks to rise after this change; WP-B3 and WP-B4 bring them down. Report the intermediate numbers.

### WP-B3 - Better local solver for changed slices
- Log the reason for every fallback and show a histogram per scenario: infeasible local result, loss above gate, calendar repair failed, bound check failed.
- Improve the local solver where the histogram points: multi-start inside the window (rho +- 0.05, three starts), anchor theta from the nearest ATM quote as the cold search does, damping (Levenberg-Marquardt), stop when the loss reaches zero, and a budget of about 25 objective evaluations per slice (was 15).
- Never accept an infeasible point, and keep determinism.

### WP-B4 - Bounded, staged fallback
- Replace the single "full cold slice search" fallback with stages: (1) local multi-start; (2) a cold search restricted to a rho window around the previous value (+-0.15) and the feasible psi interval; (3) the full cold search only if stage 2 still fails the gate.
- Add a per-update budget: at most `max_fallback_slices` full cold searches (default 8) and an optional wall-clock deadline. When the budget runs out, return the best feasible surface found with `path = degraded`, a `degraded_slices` count, and the achieved loss; the unfinished slices are refit in subsequent updates (oldest first). A degraded result must still pass `validate()`.
- Skip local attempts when the previous update had a high fallback rate (for example above 50% of slices) and the move size is large, going straight to stage 2. This removes the wasted local attempts in `fast`.
- Approved by Yuan (decision 1). A degraded result reports `degraded_slices`, the maximum and RMS ratio of its worst slice, and an age (updates since that slice last passed the gate). Degraded slices must still be feasible and arbitrage-free, and they are refit first in the next update. A slice that stays degraded for more than 3 consecutive updates triggers a full cold refit of that slice regardless of budget.

### WP-B5 - Regression tests
- Unit tests that fail when the loss ratio exceeds the hard limit on a scripted 4-scenario sequence of 50 updates each (fast to run), plus a test that `degraded` results are valid and carry correct counts.
- Do not change any existing test's expectation; if one seems wrong, name the misread requirement and change it as its own step.

### WP-B6 - Cold-path regression check
- `~/git/ql-essvi/build/wp5-parallel` holds an earlier binary. A/B time it against the current build on the same machine, same seed, same flags: 40 x 100 cold (9 repeats) and 1 x 100 cold (20 repeats), report both with load averages.
- If the current build is slower beyond noise, find the cause (candidate-search hint change in WP11 is the first suspect) and restore: single-slice cold median <= 2.5 ms, full-chain cold median <= 55 ms, with unchanged output within the parent spec's tolerance.

### WP-B7 - Scenario sizes and replay from real 5-minute data
- From the collected IBIT snapshots (`~/vol-rv/data/ibit/`, one every 5 minutes during 09:45 to 16:00 ET since 2026-10-12, excluding the forced weekend snapshot of 2026-10-10T20:45Z) and the SPY snapshots in `data/zdte/`, measure the distribution of changes between consecutive snapshots: common IV level change per expiry, tilt change, and per-quote noise (IV points). Use Black-Scholes European IV as the stand-in and label it.
- Set the synthetic scenarios from these: `normal` at the median absolute move, `fast` at the 95th percentile, `front` from the observed front-versus-back ratio, `quiet` from the 10th percentile. Record the measured numbers in `docs/perf-results.md`.
- Make a full-session replay the primary realistic benchmark: every consecutive pair of snapshots over all days available (about 78 transitions per full session), reporting the section 3 table per day. Note that skipped snapshots (no valid two-sided quotes) break the chain; restart the warm sequence with a cold fit after a gap and count the restarts.
- Report the sensitivity to the cap (0.05, 0.1, 0.2).

## 5. Order and process

1. WP-B1 (metrics and baseline for `49fed2b`), 2. WP-B7 (real 5-minute scenario sizes and replay, so everything after is tuned on realistic data), 3. WP-B2 (per-quote cap gate), 4. WP-B3 (solver, driven by the fallback histogram), 5. WP-B4 (staged fallback with degraded results), 6. WP-B5, 7. WP-B6.
- Small commits on `perf/essvi-calibrator`, one logical change each; no force-push; tests before implementation for WP-B2 and WP-B4.
- Update `docs/perf-results.md` with a new section: same table layout as section 3, with measured values and misses stated plainly.

## 6. Decisions (Yuan, 2026-10-16)

1. **Degraded results are allowed.** A deadline-bounded update may return a feasible surface that is above the quality gate (`path = degraded`) and finish the refit over later updates. WP-B4 is on by default; every degraded result carries its achieved quality numbers.
2. **The quality gate is a per-quote cap**, not an average. See WP-B2 (rewritten below); the RMS excess becomes a reported metric and a secondary target.
3. **Tune for 5-minute updates.** The scenarios that matter are the ones where the whole chain has moved since the last snapshot (`normal`, `front`, `fast`). `quiet` (sub-second style updates) must not regress but is no longer a tuning target. WP-B7 sets the scenario sizes from measured 5-minute moves.

## 7. Open items

1. Cap size: 0.1 of the quote's own band width is my default; tune on the real replay (WP-B7) and report the sensitivity (cap 0.05, 0.1, 0.2).
2. Whether the IB option bid/ask in our snapshots is the consolidated NBBO has not been checked; the band is that contract's quoted bid/ask as delivered.
