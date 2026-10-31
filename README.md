# ql-essvi

Standalone C++ eSSVI surface evaluator and benchmark harness for comparison with the existing CVI fitter.

## Status

The benchmark pipeline is implemented and reproducible on WSL. **D18** calibrates free `(theta, rho, psi)` per expiry sequentially (Corbetta). **D21** uses the Martini–Mingone butterfly infimum. **D19** ranks models by mean/median IV distance to the bid/ask band. **D22** interpolates CVI smiles onto the shared filter's strikes. **D20** retires the unused Mingone joint global solver.

## Python package

Requires Python 3.10+ and a C++20 compiler to build from source. The nanobind extension links the existing standalone calibrator; QuantLib, NumPy, and pandas are not runtime dependencies.

```bash
uv venv .venv --python 3.12
uv pip install --python .venv/bin/python '.[test]'
.venv/bin/python -m pytest -q tests/test_python_bindings.py

# Build a source distribution and a wheel from that distribution.
uv build --python .venv/bin/python
```

The wheel is specific to the Python version, operating system, and architecture used to build it. Distributed builds keep `ESSVI_NATIVE_OPTIMIZATION=OFF` so they do not require the builder's CPU instruction set. A normal standalone CMake build does not build the extension unless `ESSVI_BUILD_PYTHON=ON`.

Run from the source checkout with the included market fixture, or substitute a normalized CSV from the existing pipeline:

```python
import ql_essvi as essvi

quotes = essvi.read_quotes("tests/fixtures/deribit_btc_20261018T1200_quotes.csv")
result = essvi.calibrate(quotes)
if not result.converged:
    raise RuntimeError(result.error)

expiry = result.surface.slices[0].expiry
print(result.surface.implied_volatility(expiry, k=0.0))

options = essvi.WarmOptions()
options.quote_tol = 1e-5
options.quote_cap = 0.1
updated = essvi.calibrate_warm(quotes, result, quotes, options)
if not updated.converged:
    raise RuntimeError(updated.error)
print(updated.path, updated.slices_reused, updated.slices_refit)
```

`calibrate_warm(current_quotes, previous_result, previous_quotes, options)` retains the complete native calibration state, including reuse and degradation ages. Pass the matching previous quote snapshot, and retain the returned result and current quotes for the next update. Passing `None` as `previous_result` requests a cold start. Warm calls can take the existing fallback/degraded paths; inspect `path` and the result's diagnostics rather than assuming reuse.

`read_quotes` accepts strings and `os.PathLike` objects, including `pathlib.Path`. It raises `RuntimeError` for file-open/read failures and `ValueError` for missing required columns, malformed rows, invalid inclusion flags, or invalid non-weight numeric fields. Parse errors identify the input path and, for row errors, the line and field where applicable; no malformed quote row is silently discarded. Malformed weights remain NaN in the native quotes so calibration can reject included invalid weights through its existing fail-closed result.

Always check `converged` before using a fit as a model. A failed cold fit can retain partial surface slices for diagnostics; those slices and their read-only parameter views remain inspectable but do not constitute a successful complete fit.

For in-memory input, construct `essvi.Quotes` from an iterable of `essvi.Quote(id, expiry, forward, discount_factor, strike, bid_iv, ask_iv, mid_iv, included=True, weight=1.0)`. Calibration requires this native container explicitly, avoiding a Python-list-to-C++-vector copy on every fit. Quote fields and result fields are read-only; replace container elements with newly constructed quotes when preparing a new snapshot. `surface.slices`, `reuse_ages`, and `degraded_ages` return Python list snapshots, not mutable views of the native state.

Expiries are year fractions, IVs are decimal volatilities, and evaluator argument `k` is forward log-moneyness, `log(strike / forward)`. The result also exposes solver counts, quality metrics, `timings`, and `fallback_reason_counts`. `WarmOptions` exposes `quote_tol` and `quote_cap`; the deprecated C++ `loss_tol` setting is not exposed.

The read-only `posterior_covariances` property returns an outer tuple aligned with `surface.slices`; each inner tuple packs `[00, 01, 02, 11, 12, 22]` in `(log(theta), atanh(rho), log(psi))` coordinates. At this API stage, every successful result uses the cold diagonal variance `0.25`; it is not yet an observation-updated uncertainty estimate.

Calibration releases the GIL. Do not mutate any input `Quotes` container or `WarmOptions` object while a calibration is reading it. Separate calls can run from Python threads with immutable inputs.

For development, use `uv pip install --python .venv/bin/python -e '.[test]'`. Rerun that command after changing C++ sources.

Native and NumPy test dependencies are loaded during fixture setup, not pytest collection. Collecting tests or running pure-Python modules does not require the extension. Run the package installation step above before the full suite; missing native dependencies are reported as setup errors rather than silently skipped tests.

### Zero-copy parameters

`result.surface.parameter_view()` returns a read-only Python `memoryview` with shape `(n_expiries, 4)`, native `float64` values, and columns `expiry`, `theta`, `psi`, `rho`. It aliases the native surface storage rather than allocating or copying a parameter buffer. The full four-column view is C-contiguous. An empty surface returns a read-only `(0, 4)` view.

NumPy is optional for consumers; it is included in the `test` extra. With NumPy installed:

```python
import numpy as np

buffer = result.surface.parameter_view()
parameters = np.asarray(buffer)
theta_psi_rho = parameters[:, 1:]
```

Both arrays share the native storage and remain valid even after `result` and `buffer` are deleted: the view retains the owning surface/result. Writes, including enabling NumPy's writeable flag, are rejected. With multiple expiries, `theta_psi_rho` has a 32-byte row stride and is not C-contiguous; use `.copy(order="C")` when a writable or packed three-column array is required. `phi = psi / theta` is derived and is not a stored column.

Creating a view still allocates small Python/array metadata objects; zero-copy refers to the numeric parameter data.

## Build on WSL

Create the Python environment with `uv`:

```bash
uv venv .venv --python 3.12
uv pip install --python .venv/bin/python '.[test]' numpy pandas scipy cvxpy clarabel nbclient nbformat jupyter ipykernel
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
