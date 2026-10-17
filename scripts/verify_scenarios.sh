#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
export PATH="$ROOT/.venv/bin:$PATH"

SEEDS=(1 2 3 4 5)
SCENARIO_UPDATES=77
SCENARIOS=(quiet normal fast front)
TMP="$(mktemp -d "${TMPDIR:-/tmp}/essvi-scenarios.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

echo "Building essvi_bench from $ROOT"
cmake --build build --target essvi_bench -j2

target_miss=0
execution_failure=0
for scenario in "${SCENARIOS[@]}"; do
  for seed in "${SEEDS[@]}"; do
    report="$TMP/$scenario-$seed.json"
    log="$TMP/$scenario-$seed.log"
    echo "Running scenario=$scenario seed=$seed updates=$SCENARIO_UPDATES"
    if ./build/essvi_bench --scenario "$scenario" --scenario-updates "$SCENARIO_UPDATES" \
        --seed "$seed" --timings-out "$report" --check >"$log" 2>&1; then
      status=0
    else
      status=$?
    fi
    cat "$log"
    echo "scenario=$scenario seed=$seed check_exit=$status"
    if (( status == 2 )); then
      target_miss=1
    elif (( status != 0 )); then
      execution_failure=1
    fi
  done
done

.venv/bin/python scripts/summarize_scenarios.py "$TMP"
if (( execution_failure )); then
  exit 1
fi
exit "$target_miss"
