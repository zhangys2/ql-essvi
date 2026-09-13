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
