# ql-essvi

Standalone C++ eSSVI surface evaluator and benchmark harness for comparison with the existing CVI fitter.

## Status

The benchmark pipeline is implemented and reproducible on WSL. **D18** calibrates free `(theta, rho, psi)` per expiry under the T01 butterfly and calendar conditions, sequentially (Corbetta). **D19** ranks models by mean/median IV distance to the bid/ask band; exact `1e-12` containment remains the in-band count. Shared Python normalization/scoring and an explicit common-quote intersection remain, because the legacy CVI notebook fits only its selected OTM rows.

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

CTest includes `quantlib_smoke`, which compiles and exercises the existing QuantLib CMake
package by constructing a TARGET calendar/date object, and `essvi_ql_tests`, which checks the
D12 adapter (`EssviBlackVarianceSurface` / `EssviSmileSection`) against the plain evaluator.
No QuantLib source is modified.

The existing QuantLib build is consumed through its CMake package; no QuantLib source is modified. The D12 adapter lives in `cpp/essvi_ql.*` and does not change calibration or scoring.

Prepare and fit the real dataset:

```bash
.venv/bin/python python/prepare_inputs.py \
  /mnt/c/Users/zhang/source/repos/CVI-Vol-Surface-Fitting/option_chain.csv \
  /mnt/c/Users/zhang/source/repos/CVI-Vol-Surface-Fitting/sofrDFs.csv normalized_quotes.csv
./build/essvi_fit normalized_quotes.csv parameters.csv fitted_quotes.csv surface_grid.csv diagnostics.csv
.venv/bin/python python/score_outputs.py normalized_quotes.csv \
  --fitted fitted_quotes.csv --grid surface_grid.csv --output score.json
```
