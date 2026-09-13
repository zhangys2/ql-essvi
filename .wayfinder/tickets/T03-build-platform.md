---
id: T03
title: Choose the build and run platform
type: Grilling (HITL)
status: resolved
blocked_by: []
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

Does the C++ eSSVI project build and run on WSL Ubuntu, or on Windows with MSVC?

## Why it matters

This blocks T02, and it splits the toolchain from the data. QuantLib is built and tested only in
WSL (`libQuantLib.so.1.44.0`, `QuantLibConfig.cmake`); Windows currently has no QuantLib library at
all. Meanwhile CVI, its notebook, `option_chain.csv` and `sofrDFs.csv` all sit on the Windows side,
and D4's comparison requires both fitters to read and write a shared CSV.

## Considerations to put to the user

- WSL: QuantLib already builds there in ~2.5 minutes with the test suite green; GCC 13.3; the data
  is reachable through `/mnt/c`, which is slow for compilation but fine for reading a 14k-row CSV.
- Windows/MSVC: VS 2026 with the C++ workload is installed and verified, but QuantLib would need a
  full MSVC build first, and `Build.props` plus the Windows Boost copy were deliberately removed.
- Whether the scoring script (Python) runs on the same side as the C++ fitter, or across the boundary.

## Done when

The platform is chosen and recorded here with its reason, and any follow-on setup work is stated as
a dependency for T02.
