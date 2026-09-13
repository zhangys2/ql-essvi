# ql-essvi

Standalone C++ eSSVI surface evaluator and benchmark harness for comparison with the existing CVI fitter.

## Current status

The project is being implemented incrementally. The current WSL/QuantLib smoke test builds the eSSVI evaluator and validates its explicit Gatheral–Jacquier butterfly and Hendriks–Martini/Corbetta adjacent-slice calendar constraints.

## Build on WSL

Create the Python environment with `uv`:

```bash
uv venv .venv --python 3.12
uv pip install --python .venv/bin/python numpy pandas scipy cvxpy clarabel pytest nbclient nbformat jupyter ipykernel
```

Build the C++ portion:

```bash
export QUANTLIB_BUILD="$HOME/QuantLib/build/release"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
.venv/bin/python -m pytest -q
```

The existing QuantLib build is consumed through its CMake package; no QuantLib source is modified.

Prepare and fit the real dataset:

```bash
.venv/bin/python python/prepare_inputs.py \
  /mnt/c/Users/zhang/source/repos/CVI-Vol-Surface-Fitting/option_chain.csv \
  /mnt/c/Users/zhang/source/repos/CVI-Vol-Surface-Fitting/sofrDFs.csv normalized_quotes.csv
./build/essvi_fit normalized_quotes.csv parameters.csv fitted_quotes.csv surface_grid.csv
.venv/bin/python python/score_outputs.py normalized_quotes.csv \
  --fitted fitted_quotes.csv --grid surface_grid.csv --output score.json
```
