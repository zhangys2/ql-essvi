---
id: T02
title: Decide how the project consumes QuantLib
type: Research (AFK)
status: resolved
blocked_by: [T03]
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

How will this C++ project link and use QuantLib, and which QuantLib abstractions should the eSSVI
surface plug into?

## Why it matters

D1 fixed "standalone project linking QuantLib", but nothing is settled about *how*. QuantLib is
currently built only in WSL and was never installed, so the mechanism depends on the platform
choice in T03.

## Scope

- Consumption mechanism: `find_package(QuantLib)` against the existing build tree's
  `QuantLibConfig.cmake`, a proper `cmake --install`, or a rebuild on the chosen platform.
- Which existing types to build on: `SmileSection` (as `SviSmileSection` does), `SviSmileSection`
  itself for per-slice evaluation via `sviTotalVariance`, and whether the surface should present as
  a `BlackVolTermStructure`.
- What QuantLib gives for free that is worth using: Black formula, day counters, the XABR
  calibration machinery.
- Whether QuantLib is genuinely needed for the benchmark, or only convenient — recorded honestly.

## Done when

The linking mechanism is decided and verified by a trivial program that compiles, links and calls
one QuantLib function on the chosen platform; and the base classes the eSSVI surface will implement
are named.

## Resolution

Closed 2026-09-13 (`bd8383d`); resolution recorded 2026-09-16 from the implementation.

- **Linking mechanism:** `find_package(QuantLib CONFIG REQUIRED)` against the uninstalled WSL build
  tree (`$QUANTLIB_BUILD/cmake`, falling back to `~/QuantLib/build/release/cmake`; QuantLib 1.44.0),
  linked as the imported target `QuantLib::QuantLib`. No `cmake --install`, no rebuild, no QuantLib
  source changes. Follows D9.
- **Verification:** the CTest target `quantlib_smoke` (`tests/test_quantlib.cpp`) compiles, links and
  calls `TARGET().isBusinessDay` and `Settings::evaluationDate` on WSL/GCC.
- **D12 adapter (implemented 2026-09-17):**
  - Surface: `essvi::EssviBlackVarianceSurface` (`BlackVarianceTermStructure`),
    `blackVarianceImpl(t, strike)` delegates to the plain evaluator after converting strike to
    log-moneyness with a per-slice forward.
  - Per-expiry slice: `essvi::EssviSmileSection` (`SmileSection`), `varianceImpl` is canonical
    total variance (D14) and `volatilityImpl` is derived from it.
  - Not `SviSmileSection`: it evaluates raw per-slice SVI parameters, which eSSVI's
    `(theta_t, rho_t, psi_t)` form does not produce, and raw SVI is out of scope.
  - Native smiles: `smileSectionImpl` returns a self-contained `EssviSmileSection` (Corbetta
    interpolated slice at the requested time), so the surface need not outlive the section via
    `shared_from_this`.
  - Verification: CTest target `essvi_ql_tests` (`tests/test_essvi_ql.cpp`).
- **Is QuantLib needed?** Not for the benchmark itself. The evaluator and calibrator remain
  plain C++ (`essvi_surface.*`, `calibrator.*`); preprocessing, Black inversion and scoring
  live in Python (D10). QuantLib is used by the D12 adapter and the `quantlib_smoke` link
  check. The adapter is convenient for later QuantLib pricing, not necessary for the
  benchmark result.
