# Calibrator performance baseline

Measured before WP1 on 2026-10-15. This is the unoptimized `master` calibration algorithm with the WP0 benchmark and profiling instrumentation only.

## Environment

- CPU: AMD EPYC 9354P, 4 vCPUs exposed to a KVM guest
- Kernel: Linux 7.0.0-34-generic, x86_64
- Compiler: GCC/G++ 16.2.0 (conda-forge)
- CMake: 4.4.4, installed in the project virtual environment
- Build: `CMAKE_BUILD_TYPE=Release`, C++20, effective `-O2`, no `-march=native`
- QuantLib was not installed; CMake disabled the optional adapter and built the core library/tests.
- Load average sampled at 15:59 UTC: `1.08 1.22 1.22` (1/5/15 min).

The generated compile commands contain CMake's `-O3 -DNDEBUG` followed by the project's explicit `-O2`; GCC applies the last optimization option. The native `-O3 -march=native` option was off.

## Deterministic synthetic chain

The benchmark uses 40 expiries from 0.05 to 2.0 years, 100 log-moneyness quotes from -0.5 to 0.5 per expiry, and a fixed xorshift64 seed. The valid generating slices use `theta = 0.02 + 0.03 * expiry`, `psi = 0.05 + 0.02 * expiry`, and `rho = -0.35`. Bid/ask IVs are the generating IV ±0.00005; mid-IV receives seeded noise in ±0.00001. The fit converged with zero band loss. `tests/baseline_essvi_40x100.csv` stores the cold fitted parameters used for the WP2 behavior-preservation check.

## Results

Uninstrumented build, 9 repeated `calibrate()` calls after one cold call:

| Metric | Result |
|---|---:|
| Cold full chain | 1.2589 s |
| Repeated median | 1.1921 s |
| Repeated p90 | 1.2265 s |
| Repeated p99 | 1.2346 s |
| Repeated max | 1.2355 s |
| Candidate/objective evaluations | 48,158 |
| Objective | 0 |

Instrumented build, one cold call (the profiler's `steady_clock` timer runs inside every `mmPsi2Bound()` call):

| Metric | Result |
|---|---:|
| `mmPsi2Bound()` calls | 102,208 |
| Time inside `mmPsi2Bound()` | 1.1056 s |
| Fraction of instrumented cold wall time | 94.2% |

This confirms the section 4 hypothesis: the MM bound dominates the baseline. `perf stat` could not access hardware events because `perf_event_paranoid=4`; the compile-time per-call timer is the available profile. Baseline core CTest: `essvi_tests` passed (1/1, 0.18 s).
