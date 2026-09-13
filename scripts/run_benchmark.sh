#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
: "${QUANTLIB_BUILD:=$HOME/QuantLib/build/release}"
if [[ ! -x .venv/bin/python ]]; then uv venv .venv --python 3.12; uv pip install --python .venv/bin/python numpy pandas scipy cvxpy clarabel pytest matplotlib nbclient nbformat jupyter ipykernel; fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j"$(nproc)"
.venv/bin/python python/prepare_inputs.py "$1" "$2" normalized_quotes.csv
./build/essvi_fit normalized_quotes.csv parameters.csv fitted_quotes.csv surface_grid.csv diagnostics.csv
.venv/bin/python python/score_outputs.py normalized_quotes.csv --fitted fitted_quotes.csv --grid surface_grid.csv --output score.json
.venv/bin/python python/validate_schema.py normalized normalized_quotes.csv
.venv/bin/python python/validate_schema.py parameters parameters.csv
.venv/bin/python python/validate_schema.py fitted fitted_quotes.csv
.venv/bin/python python/validate_schema.py grid surface_grid.csv
.venv/bin/python python/validate_schema.py diagnostics diagnostics.csv
ctest --test-dir build --output-on-failure
.venv/bin/python -m pytest -q
cat score.json
