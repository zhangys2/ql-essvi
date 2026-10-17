# CAL-C5 — Apply approved IBIT quote cleaning

## State

- Current: `review-approved`
- Last transition: `reviewing` → `review-approved`
- At: `2026-10-17T15:36:05Z`
- Reason/evidence: Spec, standards, and verification reviewers approved the frozen base-to-HEAD range with no findings; the isolated verifier passed the configured full suite (CTest 5/5, pytest 25/25).
- Blocker: None.

## State history

| From | To | At | Evidence |
| --- | --- | --- | --- |
| `draft` | `ready` | `2026-10-17T15:00:36Z` | C1 is finished and the user explicitly approved the less-restrictive DEC-006 policy. |
| `ready` | `baseline-checking` | `2026-10-17T15:04:41Z` | Claimed by `omp coding assistant`; primary checkout branch `delivery/calibrator-speedup-addendum-c/CAL-C5` created at exact base `55442966a4c6b881b4dca50eb5605240f296172f`. |
| `baseline-checking` | `baseline-passed` | `2026-10-17T15:05:35Z` | Exact-base configured full suite passed: Release configure/build, CTest 5/5, pytest 19/19. |
| `baseline-passed` | `implementing` | `2026-10-17T15:06:47Z` | Baseline is green; user-approved policy and shared converter/replay evaluation seam are confirmed. |
| `implementing` | `implemented` | `2026-10-17T15:29:55Z` | Approved shared classifier and reports are complete; 212/212 fits passed in each of five training-only profiles, focused tests passed 19/19, and configured verification passed (CTest 5/5, pytest 25/25). |
| `implemented` | `reviewing` | `2026-10-17T15:31:22Z` | Implementation commit `b501822865cd0652ed1f9d66809a658cbfbd94ae` is under three isolated review axes; the frozen base-to-HEAD snapshot hash is recorded in the review record. |
| `reviewing` | `review-approved` | `2026-10-17T15:36:05Z` | Round 1's isolated spec, standards, and verification axes found no issues; verification passed CTest 5/5 and pytest 25/25 on reviewed HEAD `b501822865cd0652ed1f9d66809a658cbfbd94ae`. |

## Claim

`omp coding assistant`

## Parent

[Addendum C implementation spec](../implementation-spec.md); source handoff: [Addendum C](../../../calibrator-speedup-addendum-c.md)

## Base commit

`55442966a4c6b881b4dca50eb5605240f296172f` — finished C1 review branch head; the C1 analysis and correction tests are included.

## User decision

**DEC-006 — approved less-restrictive cleaning policy (2026-10-17):** `bid > $0`, mid premium `>= $0.05` per option share, relative price spread `(ask-bid)/((bid+ask)/2) <= 0.6666666667`, and absolute European Black–Scholes mid-IV delta in `[0.02, 0.95]`. This is a user-approved policy, not a claim that post-hoc fit quality improves. The measured post-hoc cap/RMS metrics were worse; the refit effect must be measured here.

## What to build

Apply the approved quote-cleaning policy consistently to the shared IBIT conversion and snapshot-replay preparation paths. Count each rule's exclusions and unique combined exclusions per snapshot. Refit the IBIT training snapshots under each rule alone and under the combined policy, then report the measured quality and exclusion effects. Preserve the unfiltered C1 diagnosis as a historical baseline and do not read the Oct 15–16 holdout.

## Acceptance criteria

- The four approved rules and exact boundary semantics are recorded in the implementation spec and CSV contract: minimum bid strictly greater than $0; mid premium at least $0.05; relative price spread at most `0.6666666667`; absolute BS delta from `0.02` through `0.95` inclusive. Premium/spread use raw bid/ask option prices; delta uses the existing European mid-IV, forward, strike, and expiry until WP-C4.
- The shared quote-preparation path applies these rules identically to converter and replay output. Existing locked/crossed/invalid/OTM/inversion handling and source identity remain intact; no live collector, fitting objective, diagnostics header, or weight behavior changes.
- Excluded normalized rows retain empty IV fields and `included=false`; `filter_reason` identifies every cleaning rule that rejected a baseline-valid quote in stable order. Per-rule counts are independent and can overlap; combined exclusion counts count each quote once. Existing invalid-quote counts remain separate, so raw-source reconciliation is preserved.
- Converter output and the replay manifest/report expose per-snapshot exclusions for each individual rule and unique combined exclusions.
- A reproducible IBIT-only analysis refits Oct 12–14 once for each rule alone and once for the combined approved policy. It reports per-rule and combined per-snapshot counts and refitted cold-fit quality metrics. Oct 15–16 are never read; SPY is not a gate.
- The checked-in `docs/fit-quality.md` distinguishes these refit results from C1's post-hoc masked values and documents that no holdout data were used.
- Permanent behavioral tests cover each rule's exact boundary, overlapping exclusions versus unique combined count, reason/schema semantics, and parity between converter and replay preparation. The analysis path has a deterministic synthetic test for profile selection and the Oct 12–14 boundary.
- Configured verification passes; no production fitting objective or live IB collector change.

## Out of scope

- Vega weights or WP-C9 objective/metrics changes.
- Parity forwards, quote synchrony changes, American inversion, capacity tests, WP-C7 holdout, SPY gates, or live collector changes.
- Selecting thresholds different from this explicit approval.

## Test plan

- Baseline: configured full command from `docs/delivery/DELIVERY.md` at the declared base.
- TDD: add synthetic behavior tests through `convert_snapshot` and `prepare_snapshot` for threshold equality/inclusion, rejection just outside each boundary, overlapping rule counts, and quote-ID/reason handling; show Red before production edits and Green afterward.
- Exercise the profile-analysis interface with synthetic training-day files and a controlled fit executable; assert all four individual profiles plus combined, emitted counts/metrics, and exclusion of holdout files.
- Run the real IBIT Oct 12–14 evaluation only; do not open Oct 15–16.
- Run configured full verification and review the full base-to-HEAD diff on three independent axes.

## Expected seam

`python/convert_ibit_snapshots.py::quote_row` is the shared classification seam. `convert_snapshot` and `replay_snapshots.prepare_snapshot` expose its selected-rule results and counts to their existing CSV/manifest outputs. A deterministic cleaning-evaluation CLI reuses the existing cold fit and C1 quality calculation seams to refit individual-rule and combined profiles. Tests assert through the converter/replay and analysis interfaces, not duplicated predicate logic.

## Evidence and decision references

- User decision: DEC-006, less-restrictive policy approved on 2026-10-17.
- Source: Addendum C §5 WP-C5 (cleaning filters/counts/evaluation), §7 DEC-006 (user approval gate); C1 candidate distribution in `docs/fit-quality.md`.

## Review depth

`standard`

## Verification timeline

| At | Phase | Command | CWD | Commit | Result | Evidence |
|---|---|---|---|---|---|---|
| 2026-10-17T15:05:35Z | Baseline | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `55442966a4c6b881b4dca50eb5605240f296172f` | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 19/19 passed. DELIVERY.md marks the full-suite command proven able to fail. |
| 2026-10-17T15:09:55Z | Red | `.venv/bin/python -m pytest -q tests/test_quote_cleaning.py` | `/home/zhangy/git/ql-essvi` | `55442966a4c6b881b4dca50eb5605240f296172f` + test-only working-tree change | expected failure | Collection failed because `convert_ibit_snapshots.cleaning_reasons` does not yet exist; exit code 2. This is the new approved-policy seam expressed by the test; no production changes were made. |
| 2026-10-17T15:29:55Z | Green | `.venv/bin/python -m pytest -q tests/test_quote_cleaning.py tests/test_quote_cleaning_evaluation.py tests/test_analyze_fit_quality.py tests/test_snapshot_replay.py` | `/home/zhangy/git/ql-essvi` | `55442966a4c6b881b4dca50eb5605240f296172f` + C5 working tree | passed | 19 passed. Covers policy boundaries, shared converter/replay reasons and counts, report fields, all five synthetic profiles, training-date boundary, and unfiltered C1 default. |
| 2026-10-17T15:29:55Z | Real-data evaluation | `.venv/bin/python scripts/evaluate_quote_cleaning.py --data-root ~/vol-rv/data --fit-binary build/essvi_fit > /tmp/cal-c5-evaluation.json` | `/home/zhangy/git/ql-essvi` | `55442966a4c6b881b4dca50eb5605240f296172f` + C5 working tree | passed | IBIT Oct 12–14 only: 212/212 cold fits for each of minimum-bid, minimum-premium, relative-spread, delta-window, and combined profiles. 259,978 raw contracts/profile; combined excludes 23,011 uniquely. Results recorded in `docs/fit-quality.md`; holdout and SPY not read. |
| 2026-10-17T15:29:55Z | Refactor | `.venv/bin/python -m pytest -q tests/test_quote_cleaning.py tests/test_quote_cleaning_evaluation.py tests/test_analyze_fit_quality.py tests/test_snapshot_replay.py` | `/home/zhangy/git/ql-essvi` | `55442966a4c6b881b4dca50eb5605240f296172f` + C5 working tree | passed | 19 passed after making the cleaning-profile argument keyword-only and completing the report schema. |
| 2026-10-17T15:29:55Z | Final | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `55442966a4c6b881b4dca50eb5605240f296172f` + C5 working tree | passed | Release configure/build succeeded; CTest 5/5 passed; pytest 25/25 passed. |
| 2026-10-17T15:36:05Z | Review round 1 | `export PATH="$PWD/.venv/bin:$PATH"; cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && .venv/bin/python -m pytest -q` | `/home/zhangy/git/ql-essvi` | `b501822865cd0652ed1f9d66809a658cbfbd94ae` | approved | Three isolated review axes: no findings; verification reviewer independently passed CTest 5/5 and pytest 25/25. Snapshot SHA-256: `374558af8a7491df28a7eebb3147f42dc4a09fbdd84220641d7883ebb7ea52ee`. |

## Review record

### Round 1 — isolated three-axis review

Reviewed range: `55442966a4c6b881b4dca50eb5605240f296172f..b501822865cd0652ed1f9d66809a658cbfbd94ae`. Frozen snapshot: `/tmp/cal-c5-review-snapshot.tar.gz`, SHA-256 `374558af8a7491df28a7eebb3147f42dc4a09fbdd84220641d7883ebb7ea52ee`.

#### Spec review — isolated

**Approved; no findings.** The external WP-C5 and DEC-006 requirements match the implementation: exact thresholds, the four individual profiles plus combined, IBIT Oct 12–14 only, preserved unfiltered C1 baseline, and separated refit results. Review agent: `FormalSpecAxis`.

#### Standards review — isolated

**Approved; no findings.** No weakened checks, suppressions, scope creep, or repository-standard violations. Review agent: `FormalStandardsAxis`.

#### Verification review — isolated

**Approved; no findings.** The isolated verifier ran the configured DELIVERY.md full-suite command on reviewed HEAD; Release configure/build succeeded, CTest 5/5 passed, and pytest 25/25 passed. Load-bearing facts for shared filtering/counting, unfiltered C1 behavior, profile isolation, and reconciliation are rung 4. Real-data results were reviewed from the recorded run, not rerun by the reviewer. Review agent: `FormalVerificationAxis`.

## Review dispositions

| Round | Finding | Disposition | Evidence |
| --- | --- | --- | --- |
| 1 | None | No corrections required | All three isolated axes approved the frozen range. |

## Gate candidates

None.

## Delivery record

- Baseline commit: `55442966a4c6b881b4dca50eb5605240f296172f` — Release build, CTest 5/5, pytest 19/19.
- Implementation commit: `b501822865cd0652ed1f9d66809a658cbfbd94ae`.
- Reviewed HEAD: `b501822865cd0652ed1f9d66809a658cbfbd94ae`; reviewed range: `55442966a4c6b881b4dca50eb5605240f296172f..b501822865cd0652ed1f9d66809a658cbfbd94ae`.
- Review snapshot SHA-256: `374558af8a7491df28a7eebb3147f42dc4a09fbdd84220641d7883ebb7ea52ee`.
- Final verification: configured command passed; Release build, CTest 5/5, pytest 25/25. The independent verification reviewer repeated that command successfully.
- Real-data result: 212/212 cold fits per profile for four individual rules plus combined; only Oct 12–14 IBIT inputs were read. Detailed metrics are in `docs/fit-quality.md`.
- Branch: `delivery/calibrator-speedup-addendum-c/CAL-C5`.
- Worktree: primary checkout, `/home/zhangy/git/ql-essvi`.
- Learning capture: None; no new repository-specific learning met the capture bar.
- Remaining: No WP-C5 blockers. Continue with WP-C9 on the cleaned IBIT baseline; do not read Oct 15–16 until WP-C7.
- Residual risk: The approved policy is input cleaning, not a guarantee of lower error. Combined refits modestly improve share-above-cap/RMS while the maximum ratio is slightly worse; vega-weighted loss is WP-C9.
- Review-approval record commit: pending.
- GitHub Issue: none (local tracker).
