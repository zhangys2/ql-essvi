# ql-essvi

Standalone C++ eSSVI surface evaluator and benchmark harness for comparison with the existing CVI fitter.

## Current status

The project is being implemented incrementally. The current WSL/QuantLib smoke test builds the eSSVI evaluator and validates its explicit Gatheral–Jacquier butterfly and Hendriks–Martini/Corbetta adjacent-slice calendar constraints.

## Build on WSL

```bash
export QUANTLIB_BUILD="$HOME/QuantLib/build/release"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The existing QuantLib build is consumed through its CMake package; no QuantLib source is modified.
