# Delivery Configuration

## Tracker

- Mode: local
- Repository: zhangys2/ql-essvi
- Create GitHub Issues after spec approval: no
- Dependency representation: Markdown links between local tickets (confirmed local mode; GitHub issue creation disabled)

## Verification

- Focused tests: `ctest --test-dir build -R <test-name> --output-on-failure` (cwd: repository root; run the build step first)
  - Proven able to fail: yes, 2026-10-16
- Full suite: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON && cmake --build build -j2 && ctest --test-dir build --output-on-failure && uv pip install --python .venv/bin/python '.[test]' && .venv/bin/python -m pytest -q` (cwd: repository root; needs `.venv/bin` on PATH for cmake; about 30 s plus build)
  - Proven able to fail: yes, 2026-10-16
- Typecheck/lint: not applicable (the build uses -Wall -Wextra -Wpedantic; no linter is configured)
  - Proven able to fail: unproven

## Replay reporting

Run the declared held-out replay from the repository root after the configured build:

`export PATH="$PWD/.venv/bin:$PATH"; cmake --build build --target essvi_bench -j2 && .venv/bin/python python/run_snapshot_replay.py --data-root "$HOME/vol-rv/data" --output-dir build/eba4-heldout-replay --bench build/essvi_bench`

The data root must contain the supported IBIT/SPY snapshot corpus (the default is `$HOME/vol-rv/data`; pass another `--data-root` when needed). The runner prepares normalized quotes and writes JSON/CSV results under the ignored `build/eba4-heldout-replay` directory. Each uninterrupted five-minute segment starts cold; each later snapshot is compared one step ahead against the preceding warm state.

The snapshot replay JSON and CSV report paired warm/cold band loss, feasibility, fallback rate, latency, and per-update RMS total-variance surface movement over exact matched expiries on the deterministic 101-point log-moneyness grid from -0.5 to 0.5. Daily movement RMS values are means across measured updates; matched-expiry and grid-point fields are summed counts. Movement fields are null when no paired update has comparable surfaces. Snapshot IVs use the documented Black--Scholes European stand-in for American IVs; this replay is a pipeline comparison, not market evidence or a performance/predictive claim.

## Review

- Review rounds: 5

## Git workflow

- Base branch: master (`origin/master` at `af7f0e4` contains the speedup work via PR #9; local `master` is aligned at `af7f0e4`)
- Branch pattern: `delivery/<brief-slug>/<ticket-id>`
- Worktree root: the primary checkout, `~/git/ql-essvi`; there are no linked worktrees (Yuan, 2026-10-16)
- Ticket checkout form: one ticket at a time in the primary checkout. Start from a clean tree, then `git switch -c <ticket-branch> <base-commit>`; return to `master` between tickets. No parallel tickets. (The baseline skill's default is a separate worktree outside the source checkout, so this is the "another safe form" it allows; the skill's falsifiability probes still use a throwaway worktree.)
- Commit convention: one reviewable commit per ticket, subject prefixed `feat:`, `fix:`, `perf:`, `docs:` or `test:` as in the existing history

## Learnings

- Store: `docs/delivery/LEARNINGS.md`
- Existing knowledge to consult first: `docs/perf-baseline.md`, `docs/perf-results.md`

## Repository references

- Instructions: `README.md`
- Standards: `docs/csv-contract.md`
- Architecture/domain docs: `docs/parameterization.md`, `docs/calibrator-speedup-spec.md`, `docs/calibrator-speedup-addendum.md`, `docs/calibrator-speedup-addendum-b.md`
- Quality floor: none
