# Calibrator speedup, Addendum B — implementation spec

- **Intake:** decision-only
- **Status:** Approved by the user on 2026-10-16T11:22:57Z (`approve the spec, implement`)
- **Approval scope:** Approved as written, including DEC-011 through DEC-015 and the implementation inferences below.
- **Post-approval direction:** On 2026-10-17, the user selected workload and acceptance recalibration after full-session evidence. The session-scale workload and cold-relative RMS amendment are recorded under Acceptance criteria; latency, fallback, and safety limits remain unchanged.
- **Source handoff:** `docs/delivery/2026-10-16-handoff.md`
- **Validator:** Not run. This is decision-only intake, not a research brief; the addenda are not to be validated as research briefs.
- **Base:** `master` at `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536`; the base tree was verified identical to commit `49fed2bd0a31f1df60ffc370f8af82978d1b51d0`.

## Source handoff

```text
# Decision-only handoff: calibrator speedup, Addendum B (2026-10-16)

Intake for `$deliver-research-brief` (decision-only). This is not a research brief; do not run `validate_research_brief.py` on it or on the addenda. All decisions below were answered by Yuan; the paragraph is an intake artifact, not approval to start delivery.

Read first: `docs/delivery/DELIVERY.md`, `README.md`, `docs/parameterization.md`, `docs/csv-contract.md`, `docs/calibrator-speedup-spec.md`, `docs/calibrator-speedup-addendum.md`, `docs/calibrator-speedup-addendum-b.md`, `docs/perf-baseline.md`, `docs/perf-results.md`, and the current code in `cpp/calibrator.cpp`, `cpp/calibrator.hpp`, `bench/essvi_bench.cpp`, `bench/realistic_warm.hpp`. Build, in this order: (1) WP-B1: add per-quote excess-ratio metrics, a `--check` pass/fail table against Addendum B section 3, and `scripts/verify_scenarios.sh` (4 scenarios, 5 seeds), and record the `49fed2b` baseline; (2) WP-B7: measure real 5-minute IV moves from the IBIT and SPY snapshots under `~/vol-rv/data/` to set the `normal`/`fast`/`front`/`quiet` scenario sizes, and make a full-session snapshot replay the primary benchmark; (3) WP-B2: replace the `loss_tol * bandLossScale` gate and the reuse test with a per-quote cap (`ratio = excess / own band width`, default cap 0.1, tuned on the replay with sensitivity at 0.05/0.1/0.2), with a reuse drift limit; (4) WP-B3: log fallback reasons and improve the local solver (multi-start, damping, ATM-anchored theta, about 25 objective evaluations per slice); (5) WP-B4: staged fallback with a per-update budget and `degraded` results; (6) WP-B5: regression tests that fail on the Addendum B quality rows; (7) WP-B6: A/B the cold path against `build/wp5-parallel` and restore cold latency if it regressed. Decisions already made by Yuan: degraded results are allowed; the gate is a per-quote cap; tune for 5-minute updates (`quiet` must only not regress); any in-band surface is acceptable, so no mid-centering objective; at most 4 threads; the work builds on `master` (which contains the `49fed2b` speedup) and does not change D17-D22 semantics or the CSV contract headers; one ticket at a time in the primary checkout (no linked worktrees). Assumptions to confirm during spec review: the cap value 0.1, the 12-update reuse drift limit, the 3-update degraded limit, and that Black-Scholes European IV is an acceptable stand-in for American IV in the replay. Validate with the `DELIVERY.md` commands (build with cmake from `.venv/bin`, then `ctest --test-dir build --output-on-failure` and `.venv/bin/python -m pytest -q`), plus `scripts/verify_scenarios.sh` and the replay on the real snapshots; do not report a command as passing unless it ran. Rules: do not edit the addenda, do not create tickets, worktrees, branches, commits or code until the implementation spec is explicitly approved, and report files changed, validation evidence, unresolved risks and blockers.
```

## Problem and outcome

The current warm calibrator can return a feasible surface while allowing large per-quote band misses, and a failed warm gate can invoke an unbounded full cold fit for each slice. The existing benchmark does not report the Addendum B per-quote quality measures, does not calibrate its synthetic moves from full-session IBIT/SPY data, and does not enforce the revised quality/latency table. This work makes those quality and cost properties measurable first, then replaces the gate and bounds fallback work without weakening feasibility or the existing D17-D22 model semantics. [DEC-001, DEC-002, DEC-003, DEC-007]

**Desired outcome:** a deterministic warm path with an auditable per-quote quality gate, replay-calibrated scenarios, explicit fallback causes, bounded staged search, and a valid degraded result when the configured per-update budget is exhausted. The full-session replay becomes the primary realistic benchmark; quality failures remain visible rather than being hidden by latency improvements. [DEC-001, DEC-002, DEC-003]

## Evidence and repository facts

No research facts are asserted: this is a decision-only handoff, not a research brief. The following are current repository observations, not claims established by research:

- `cpp/calibrator.hpp` currently defines only `cold`, `warm`, and `fallback` paths. `CalibrationResult` has no degraded-slice ages/reason histogram; `WarmOptions` exposes `quote_tol`, `loss_tol`, periodic cold-check interval, and update number. `calibrateWarm` accepts a previous `Surface` and previous quotes. [Repository fact: `cpp/calibrator.hpp`]
- `cpp/calibrator.cpp` computes a sum-of-squared-width scale and uses `loss_tol * bandLossScale` both for prior-surface reuse and for the warm quality gate. A failed gate invokes `fitSlice` as a cold fallback. Final results pass through `validate()`, and invalid results clear their surface. [Repository fact: `cpp/calibrator.cpp`]
- The current warm API callers include the benchmark and `tests/test_warm_calibrator.cpp`. Existing warm tests cover reuse, the old width-scaled gate, cold fallback, deterministic output, calendar repair, and invalid-input behavior. The existing test suite explicitly says not to change expectations silently. [Repository fact: `bench/essvi_bench.cpp`; `tests/test_warm_calibrator.cpp`; DEC-007]
- `bench/essvi_bench.cpp` already supports four seeded synthetic scenarios, replay from a converted-CSV directory, summary timings, path counts and load averages. Its normal scenario path uses 200 updates, but the CLI has no `--check` and does not run five seeds as one verification command. `bench/realistic_warm.hpp` has static scenario amplitudes. [Repository fact: `bench/essvi_bench.cpp`; `bench/realistic_warm.hpp`]
- The IBIT snapshot converter uses European Black-Scholes IV and discovers `chain_*.jsonl.gz`; the available SPY snapshots use `spy_*.jsonl.gz` in `~/vol-rv/data/zdte/`. The actual IBIT and SPY directories under `~/vol-rv/data/` contain snapshots for 2026-10-12 through 2026-10-15. The converter and `python/prepare_inputs.py` reject crossed quotes using `ask < bid`; locked quotes (`ask == bid`) currently pass those checks. [Repository fact: `python/convert_ibit_snapshots.py`; `python/prepare_inputs.py`; inspected data directories]
- The current snapshot-conversion code skips snapshots with no valid quotes. The replay must therefore distinguish a skipped/invalid snapshot or missing interval from a valid adjacent pair and restart cold after a gap. [Repository fact: `python/convert_ibit_snapshots.py`; DEC-001]
- `docs/csv-contract.md` fixes the shared CSV headers and states that a reported fit must satisfy the requested constrained solve and safety checks. The existing benchmark emits separate JSON/timing output, so the quality report can remain outside the CSV contract. [Repository fact: `docs/csv-contract.md`; DEC-007]
- `docs/delivery/DELIVERY.md` confirms local Markdown tickets, GitHub issue creation disabled, `master` as base, one ticket at a time in the primary checkout, and no linked worktrees. It records the configured full-suite command and falsifiability proofs for focused/full tests. [Repository fact: `docs/delivery/DELIVERY.md`]
- The `49fed2b` commit and the approved `master` base have identical tree hashes, so the current base code is suitable for recording the Addendum B baseline. This does not substitute for actually running the benchmark. [Repository fact: verified base/tree comparison; DEC-006]

## Scope and non-goals

### In scope

1. Implement Addendum B work packages WP-B1 through WP-B7 in the required order, with the dependency edges below. [DEC-001]
2. Add per-quote excess-ratio and implied-volatility-point metrics, a target/hard-limit report, deterministic five-seed scenario verification, and the `49fed2b` baseline. [DEC-001, DEC-002, DEC-003]
3. Measure real five-minute IBIT/SPY changes, calibrate synthetic scenario sizes, and make all consecutive valid snapshot pairs across available sessions the primary realistic replay. [DEC-001, DEC-003, DEC-014]
4. Replace the old loss-scale gate with the per-quote cap and apply it to reuse; add bounded local and staged cold search, reason observability, degraded-state tracking, and regression coverage. [DEC-001, DEC-002, DEC-011, DEC-012, DEC-013]
5. Compare cold performance with the designated `build/wp5-parallel` binary on the same machine and restore the stated cold-path thresholds if a real regression is established. [DEC-001]

### Non-goals and constraints

- No changes to the eSSVI parameterization, objective meaning, butterfly bound, calendar conditions, feasibility rules, or D17-D22 semantics. No mid-centering objective: any feasible in-band surface is acceptable. [DEC-004, DEC-007]
- No changes to the `diagnostics.csv` or other shared CSV headers, no American-IV inversion, and no changes to the CVI comparison. Benchmark/replay metrics belong in existing benchmark output or a new non-contract artifact. [DEC-007, DEC-014]
- Do not edit either addendum. Do not create tickets, branches, worktrees, commits, or product code until this spec is explicitly approved. After approval, use the confirmed local tracker and primary-checkout workflow; do not create linked worktrees or GitHub Issues. [DEC-008, DEC-009, DEC-010]
- Keep worker count at four or fewer and preserve deterministic candidate ordering/reduction. [DEC-005]
- Do not change existing test expectations silently. If a test specifically encodes behavior superseded by the new gate, identify the precise conflict and treat any expectation update as a separately named, reviewed change rather than disguising it as cleanup. [DEC-007]

## Acceptance criteria

This table retains Addendum B §3's limits except for the user-directed workload and RMS calibration. Synthetic scenarios use 77 updates, the longest uninterrupted warm chain in the 739-snapshot capture; the former 200-update matrix remains a long-run stress workload, not a session proxy. The full-session replay remains the primary realistic benchmark and reports results per day. Synthetic results use five fixed seeds plus range and load averages. `--check` returns non-zero on target misses and reports target status separately from hard-limit status. [DEC-001, DEC-003; `docs/calibrator-speedup-addendum-b.md` §3]

| Metric | Target | Hard limit |
|---|---:|---:|
| Per-quote cap share above 0.1, warm minus cold | ≤ 0.5 percentage points | ≤ 2 percentage points |
| Maximum excess ratio, warm | ≤ max(0.1, cold + 0.01) | ≤ max(0.2, cold + 0.05) |
| RMS excess-ratio increase, warm minus cold | ≤ 0.02 | ≤ 0.05 |
| Summed band loss, warm / cold | ≤ 1.05 | ≤ 1.20 |
| Per-quote RMS excess outside band, warm vs cold, in vol points | within +0.02 vol point | within +0.05 vol point |
| Warm p99, quiet / normal / front | ≤ 3 ms | ≤ 10 ms |
| Warm p99, fast | ≤ 10 ms | ≤ 20 ms |
| Fallback updates, quiet / normal / front | ≤ 1% | ≤ 5% |
| Fallback updates, fast | ≤ 10% | ≤ 25% |
| Warm failures and invalid surfaces | 0 | 0 |
| Dense-grid butterfly/calendar violations | 0 | 0 |

RMS calibration rationale: the current real replay's cold-reference RMS excess ratio is 1.719–3.113 across usable asset-days, far above the former absolute 0.05 ceiling. The criterion therefore limits warm degradation relative to the paired cold reference using the original 0.02/0.05 margins. No latency, fallback, per-quote, loss, IV-point, feasibility, or arbitrage limits are relaxed; the latest replay still shows fallback rates of 94.1–100% and warm p99 of 6.95–125.14 ms. The original Addendum B source document is unchanged.

Additional observable requirements:

- For each included two-sided quote, let `bid_var = expiry * bid_iv²`, `ask_var = expiry * ask_iv²`, `width = max(ask_var - bid_var, 1e-6)`, `w_fit` be the fitted total variance, `excess = max(0, bid_var - w_fit, w_fit - ask_var)`, and `ratio = excess / width`. Report share above cap, maximum, RMS, summed loss, and the warm/cold comparison. Convert excess distance to IV points using fitted IV `sqrt(w_fit / expiry)`, distance to the nearer violated IV boundary (zero when inside), multiplied by 100. Exclude rows not included in the fit or without a valid two-sided band; document quote counts. [DEC-002]
- `scripts/verify_scenarios.sh` runs quiet, normal, fast, and front for five fixed, reported seeds; each run uses 77 updates and emits the pass/fail table and load averages. [DEC-001, DEC-003]
- The warm path has zero invalid results and zero dense-grid butterfly/calendar violations. A degraded result is still validated and arbitrage-free; it must include achieved quality, degraded-slice count, per-slice age, and worst-slice maximum/RMS excess ratio. [DEC-001, DEC-007, DEC-013]
- `loss_tol` remains in `WarmOptions` as deprecated compatibility state, but it no longer controls either the gate or reuse. The benchmark reports old/new gate behavior side-by-side on the `49fed2b` baseline. [DEC-002]
- Quiet median and p99 must not regress against the `49fed2b` baseline (0.47 ms median, 7.6 ms p99). For cold-path A/B, run 40 expiries × 100 quotes for nine repeats and one expiry × 100 quotes for 20 repeats, recording load averages. If slower beyond measurement noise, restore single-slice cold median ≤ 2.5 ms and full-chain cold median ≤ 55 ms, with outputs within the parent spec's tolerance. [DEC-003, DEC-007]
- Real-data analysis reports common level, tilt, and per-quote residual-noise changes for IBIT and SPY; labels European Black-Scholes IV as a stand-in; reports the measured normal/fast/front/quiet scenario values and cap sensitivity at 0.05, 0.1, and 0.2. The full replay processes every consecutive valid snapshot pair, counts gaps/restarts, starts cold after a gap, and reports the quality table per day. [DEC-003, DEC-014]
- Strict replay preparation excludes crossed and locked quotes and reports excluded counts per snapshot. Do not claim delivered quotes are consolidated NBBO; that status is unverified. [DEC-015]

## Proposed modules, seams, and data/API changes

### Calibration result and warm-state transition

Add `CalibrationPath::degraded`. Extend `CalibrationResult` with the degraded-slice count, per-expiry degraded age/status needed by the next update, achieved quality summary, and fallback-reason counts. Keep those fields in memory and benchmark reporting; do not add them to the shared CSV schema. [DEC-001, DEC-007]

**Approved implementation inference — API shape:** change `calibrateWarm` to take the previous `CalibrationResult` (the last cold/warm/fallback/degraded result) plus previous quotes, rather than a bare `Surface`. This carries the prior surface, per-expiry degraded ages, and prior fallback rate needed for oldest-first refits, age expiry, and the high-fallback shortcut without a separate parallel state object. Migrate every in-repository caller in the benchmark and warm tests. If the prior result is absent or not usable, take the cold initialization path. The tradeoff is a source-level API break in exchange for one explicit state carrier and no duplicated warm-state container. [DEC-001, DEC-007]

A degraded result is successful only if its returned surface passes the existing `validate()` invariants. The approved interpretation is `converged=true` for a complete execution of the configured bounded policy that returns a valid, explicitly marked `path=degraded` result; an invalid or missing surface remains a failure with a non-empty error. This preserves the CSV header and safety meaning while making the path field the signal that the quality gate was not fully met. [DEC-001, DEC-007]

### Gate and reuse

- Gate each candidate quote against its own total-variance band. Use `WarmOptions::quote_cap` with default 0.1. Do not replace this with an average or summed-loss gate. [DEC-002, DEC-011]
- For each changed or reused candidate, first test the base per-quote cap. If a reused surface fails, continue with local multi-start; if the local candidate fails, run the restricted cold stage. If the restricted cold candidate still fails the base cap, run the full cold stage when budget permits. The full cold result supplies the same-snapshot, per-quote reference; cache it for the remainder of that snapshot. Accept an alternative candidate only if every quote's ratio is at most `max(quote_cap, cold_ratio_for_that_quote + 0.01)`. Otherwise use the full cold result. This preserves WP-B4's local → restricted cold → full cold order and does not use the restricted fit as an unverified global cold reference. [DEC-001, DEC-002]
- Reuse applies the same per-quote cap and existing predecessor/calendar checks. A slice cannot be reused unchanged for more than 12 consecutive updates. Keep `quote_tol` for the existing unchanged-quote shortcut unless tests show it conflicts with the per-quote gate. [DEC-002, DEC-012]
- If a candidate fails the base cap but the per-update full-cold budget is exhausted before a same-snapshot cold reference can be obtained, do not accept it as if it passed. Return the best feasible validated surface as degraded and retain the slice for oldest-first refit. [DEC-001, DEC-002]
- **Approved implementation inference — periodic checks:** retire the scheduled `cold_check_interval` quality-gate path. WP-B2 says to compute the cold per-quote reference only when the gate fails; scheduled cold checks would add work to otherwise accepted updates and conflict with that rule. Remove the option and migrate its in-repository callers. The explicit compatibility retention applies to `loss_tol`, not this interval. [DEC-002]

### Local solver, stages, budget, and status

- Log one reason for every rejected warm/local candidate or fallback: infeasible local result, loss above gate, calendar repair failed, or bound check failed; add a distinct budget-exhaustion reason. Report histograms by scenario. [DEC-001]
- Improve local fitting with three deterministic starts in the previous-rho ±0.05 window, ATM-anchored theta, Levenberg-Marquardt damping, zero-loss early stop, and about 25 objective evaluations per slice. Never accept infeasible points. Avoid per-objective-evaluation allocation. [DEC-001, DEC-005]
- Use stages: (1) local multi-start; (2) cold search restricted to previous rho ±0.15 and the feasible psi interval; (3) full cold search only when the restricted stage still fails the gate. [DEC-001]
- Cap full cold searches at eight per update by default. A wall-clock deadline is optional and disabled unless separately justified; the count budget is the deterministic default. Once the budget is exhausted, return the best feasible validated surface as `degraded`, record unfinished slices, and prioritize those slices oldest first on the next update. Do not return an infeasible or invalid surface as a success. [DEC-001]
- If the preceding update had fallbacks on more than half of its slices and the measured move is large, skip local work and start at stage 2. **Approved implementation inference:** define “large” against the empirical fast-scenario move distribution measured in WP-B7 and record the exact metric/threshold in `docs/perf-results.md`; do not tune an unmeasured constant. [DEC-001, DEC-003]
- A slice degraded for three consecutive returned updates is forced through a full cold refit on the following update, even if that exceeds the normal budget. Reset age only after that slice passes the gate; if a valid fit cannot be produced, return a failure rather than an invalid surface. [DEC-001, DEC-013]

### Benchmark and replay adapter

- Extend `bench/essvi_bench.cpp` to calculate and print the acceptance metrics and a `--check` table; extend JSON output without changing CSV headers. `--check` fails on any target miss and marks hard-limit violations separately. Five deterministic seeds are fixed and printed by the scenario script. This `--check` interpretation follows “non-zero on a miss.” [DEC-001, DEC-003]
- WP-B7 fits a robust level/tilt model to matched quote IV changes per expiry, with residual changes as quote-noise estimates. Report per-asset distributions and use equal asset/day weighting for pooled scenario quantiles so a denser underlying or longer day does not dominate. Set normal from the pooled median absolute move, fast from the 95th percentile, quiet from the 10th percentile, and front from the observed front-versus-back move ratio; retain both pooled and asset-specific values. This estimator/weighting is an approved implementation inference; it avoids quote-count weighting and keeps source measurements inspectable. [DEC-003, DEC-014]
- Replay recognizes both available data naming patterns (`chain_*.jsonl.gz` for IBIT and `spy_*.jsonl.gz` for SPY), preserves snapshot timestamps and contract identity across consecutive files, counts crossed/locked/otherwise excluded rows per snapshot, and breaks the warm chain on missing, skipped, or no-valid-quote snapshots. The actual SPY source directory is `~/vol-rv/data/zdte/`, not the repository-relative `data/zdte/` mentioned in Addendum B. [DEC-001, DEC-015]
- **Approved implementation inference — Python boundary:** keep the general `python/prepare_inputs.py` and CVI workflow unchanged; apply strict crossed/locked rejection and per-snapshot exclusion counts in the benchmark-only snapshot-replay preparation path. Reuse conversion logic rather than duplicating it. [DEC-007, DEC-015]

## Proposed work slices and blocking edges

These are the approved planning slices, not ticket artifacts. Each is single-owner and sequential under the confirmed one-ticket-at-a-time workflow.

| Slice | Scope and seam | Blocked by |
|---|---|---|
| WP-B1 — Metrics and baseline | `bench/essvi_bench.cpp`, a new `scripts/verify_scenarios.sh`, and baseline/results reporting. Add per-quote metrics, `--check`, five-seed execution, and baseline numbers for `49fed2b`. | None |
| WP-B7 — Measured scenarios and replay | Snapshot replay preparation plus `bench/essvi_bench.cpp` and `docs/perf-results.md`; derive scenario sizes from IBIT/SPY and report full-session results. | WP-B1 |
| WP-B2 — Per-quote gate and reuse | `cpp/calibrator.hpp`, `cpp/calibrator.cpp`, `tests/test_warm_calibrator.cpp`; cap, adaptive cold reference, reuse cap/drift, deprecated `loss_tol`. | WP-B7 |
| WP-B3 — Local solver and reasons | `cpp/calibrator.cpp` plus result/report fields and focused warm tests; multi-start, damping, ATM anchor, bounded objective work, deterministic reason counts. | WP-B2 |
| WP-B4 — Staged bounded fallback | `cpp/calibrator.hpp`, `cpp/calibrator.cpp`, benchmark output and warm tests; restricted/full cold stages, per-update budget, valid degraded results and age priority. | WP-B3 |
| WP-B5 — Acceptance regression coverage | `tests/test_warm_calibrator.cpp` and any focused scenario-test seam; 4 × 50 scripted sequence, hard quality bounds, degraded validity/counts/age. Keep existing expectations unchanged unless a separately named conflict is approved. | WP-B4 |
| WP-B6 — Cold-path regression check | Benchmark/build artifacts and `docs/perf-results.md`; same-machine cold A/B, load reporting, restore cold latency only if regression exceeds noise. | WP-B5 |

`bench/essvi_bench.cpp` and `docs/perf-results.md` are shared by WP-B1/WP-B7; `cpp/calibrator.*` and warm tests are shared by WP-B2/B3/B4/B5. Keep the listed blocking order and one active ticket at a time; do not run overlapping writers. Publish one local Markdown ticket per accepted slice with acceptance criteria, test plan, seam, base commit, and DEC references. [DEC-008]

## Verification strategy

No implementation or project verification has been run for this draft. Use the configured commands in `docs/delivery/DELIVERY.md` from the repository root, after the required build:

1. Focused warm tests through `ctest --test-dir build -R <test-name> --output-on-failure` after each behavior change; add deterministic tests for gate boundaries, reuse age, per-quote cold reference/cache, fallback stage/reason, budget exhaustion, oldest-first refit, degraded age/forced cold transition, and invalid-surface rejection. Tests cross the calibrator API used by callers, not internal helper-only seams. [DEC-001, DEC-002, DEC-007, DEC-011, DEC-012, DEC-013]
2. Run `scripts/verify_scenarios.sh` to exercise the four 200-update scenarios over five fixed seeds; verify target/hard status, p99 and fallback rates, per-quote metrics, load averages, and deterministic repeat output. [DEC-001, DEC-003]
3. Full-session replay on available IBIT and SPY snapshots; assert every consecutive pair is processed or a gap is counted, cold restarts occur after gaps, exclusions include locked/crossed counts, and each per-day report contains all quality fields. Label European Black-Scholes IV and do not characterize quote bands as NBBO. [DEC-014, DEC-015]
4. Dense-grid butterfly/calendar scan and `validate()` for every warm/degraded surface; zero warm failures/invalid surfaces in acceptance scenarios. [DEC-001, DEC-007]
5. Cold A/B against `build/wp5-parallel` using the same machine, compiler/flags, seed, and benchmark sizes; first verify the comparison binary’s provenance and flags. If that cannot be verified, report the comparison as inconclusive rather than claiming a regression or pass. [DEC-001]
6. Run the configured full command `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` with `.venv/bin` on `PATH`. Report commands and actual results only; preserve failures as blockers. [DEC-001]

## Rollout and delivery constraints

- The user explicitly approved this spec on 2026-10-16T11:22:57Z. Proceed with ticket publication and implementation in the approved order. [DEC-010]
- Use the confirmed base commit and local ticket workflow in `docs/delivery/DELIVERY.md`; one ticket/branch at a time in the primary checkout, configured `delivery/<brief-slug>/<ticket-id>` branch names, no linked worktrees, no GitHub Issues, and one reviewable local commit per completed ticket. Do not modify master, push, merge, or edit the addenda. [DEC-006, DEC-008, DEC-009]
- Update `docs/perf-results.md` with the five-seed table, replay scenario measurements, daily replay results, cap sensitivity, load averages, baseline comparisons, and misses. Keep all quality failures explicit. [DEC-001, DEC-003]

## Risks and blockers

- **Data quality:** Addendum B says delivered quote bands may not be consolidated NBBO; source accuracy and American-option pricing are not validated by the European-IV replay. [DEC-014, DEC-015]
- **Data pipeline boundary:** current preparation accepts locked quotes, while the benchmark replay must count and remove them. Keep the approved strict filter isolated from the general input/CVI pipeline. [DEC-007, DEC-015]
- **Solver and budget interaction:** a strict quality gate may expose more failing slices; the full-cold cap and adaptive per-quote reference can conflict if the budget is consumed. Return an explicitly degraded but valid surface rather than accept unchecked quality or exceed the budget, except for the approved forced-refit age rule. [DEC-001, DEC-002, DEC-013]
- **Cold baseline provenance:** verify the `build/wp5-parallel` binary's compiler and flags before A/B; otherwise the comparison is inconclusive. [DEC-001]
- **Existing tests:** any failure caused by the gate replacement must be traced to an exact existing expectation. Do not rewrite it as incidental cleanup; escalate if the user's expectation-preservation rule prevents required coverage. [DEC-007]

## Decision record

### User decisions

- **DEC-001 — Adopt Addendum B’s work packages and order.** Source: handoff, “Build, in this order: (1) WP-B1 … (7) WP-B6.” This spec proposes those seven slices in that order; the addenda remain unchanged.
- **DEC-002 — Gate quality per quote, not by an aggregate average.** Source: handoff, “the gate is a per-quote cap.” The existing loss-scale threshold must no longer decide gate or reuse acceptance.
- **DEC-003 — Optimize for five-minute updates; quiet is a non-regression constraint.** Source: handoff, “tune for 5-minute updates (`quiet` must only not regress).” WP-B7 measurements and synthetic scenarios establish the representative move sizes.
- **DEC-004 — Any in-band surface is acceptable.** Source: handoff, “any in-band surface is acceptable, so no mid-centering objective.”
- **DEC-005 — Use no more than four calibration threads.** Source: handoff, “at most 4 threads.”
- **DEC-006 — Use `master` containing the `49fed2b` speedup as the implementation base.** Source: handoff, “the work builds on `master` (which contains the `49fed2b` speedup).” The current base is `af7f0e4bf883dec67e26dc6c94e5d665a8fb5536`; its tree matches `49fed2b`.
- **DEC-007 — Preserve D17-D22 semantics, CSV contract headers, and existing test expectations.** Source: handoff, “does not change D17-D22 semantics or the CSV contract headers”; Addendum B WP-B5, “Do not change any existing test's expectation.” Any exception must be named and separately resolved, not silently bundled.
- **DEC-008 — Use one ticket at a time in the primary checkout; no linked worktrees.** Source: handoff. `docs/delivery/DELIVERY.md` additionally confirms local Markdown tickets and disables GitHub issue creation.
- **DEC-009 — Treat the addenda as non-research inputs.** Source: explicit user instruction and handoff. Do not run `validate_research_brief.py` on the addenda or handoff; do not edit the addenda.
- **DEC-010 — Stop at spec approval, then proceed.** The handoff required stopping before delivery; the user explicitly approved this spec with “approve the spec, implement” on 2026-10-16T11:22:57Z.
- **DEC-011 — Default per-quote cap is 0.1.** The approved spec sets `WarmOptions::quote_cap` to 0.1 and requires sensitivity at 0.05/0.1/0.2.
- **DEC-012 — Reuse drift limit is 12 consecutive updates.**
- **DEC-013 — Permit at most three degraded returned updates; force full cold on the next update, even over the ordinary budget.**
- **DEC-014 — European Black-Scholes IV is accepted as a labeled calibration-path stand-in, not an American pricing-accuracy claim.**
- **DEC-015 — Remove/count crossed and locked quotes in the benchmark replay adapter only; leave general `python/prepare_inputs.py` and CVI unchanged.**

### Approved implementation inferences

- Use the previous `CalibrationResult` as the warm-state carrier so surface, fallback rate, and degraded ages move together; migrate all in-repository callers. A dedicated `WarmState` type was the alternative, but would add another public type.
- `--check` returns non-zero on any target miss; report target and hard-limit status separately.
- A valid, fully validated degraded result is `converged=true` with `path=degraded`; reserve `converged=false` for failure to produce a valid surface. Preserve the existing CSV schema.
- Estimate scenario moves by robust level/tilt fits on matched quotes, use residuals as quote-noise estimates, retain instrument-specific results, and weight pooled scenario quantiles equally by asset/day.
- Use a count limit of eight full cold searches per update as the deterministic default; leave an optional wall-clock deadline disabled.
- Remove scheduled full-cold quality checks; use same-snapshot full-cold references only when a candidate fails the base per-quote gate. Remove `cold_check_interval`; the handoff only requires compatibility retention for `loss_tol`.
- Measure “large move” for skipping local starts against WP-B7’s empirical fast-scenario distribution and document the exact metric/threshold.
- Keep strict crossed/locked filtering within the replay adapter, not the shared Python preparation/CVI path.
