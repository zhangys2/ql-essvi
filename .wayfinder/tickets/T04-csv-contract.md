---
id: T04
title: Define the shared CSV contract between the two fitters
type: Grilling (HITL)
status: open
blocked_by: [T07]
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

What exactly are the input and output CSV schemas that both the C++ eSSVI fitter and the Python CVI
fitter must read and write, so a single scorer can compare them?

## Why it matters

D4 made this contract the entire basis of the comparison. If the two fitters consume even slightly
different inputs — a different forward, a different filter, a different IV inversion — the benchmark
measures preprocessing differences rather than the models.

## Scope

- Input schema: per quote, at minimum expiry, time to expiry, forward, discount factor, strike,
  option type, bid IV, ask IV, mid IV, and an inclusion flag from filtering.
- Output schema: fitted IV or total variance per quote, plus fitted values on a dense grid for
  arbitrage checking beyond the listed strikes.
- Diagnostics each fitter must emit: per-slice parameters, convergence status, timing (indicative
  only per D4).
- Numeric conventions: units (vol points vs variance), precision, date format, and how missing or
  filtered quotes are represented.

## Done when

Both schemas are written down with column names, types and units, and a tiny hand-made example file
exists for each so T06 has an unambiguous target.
