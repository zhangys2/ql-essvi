---
id: T06
title: Export CVI's fitted surface and diagnostics to the shared contract
type: Task (AFK)
status: resolved
blocked_by: [T04]
claimed_by: null
map: ../maps/essvi-vs-cvi-benchmark.md
---

## Question

What is the smallest change to the CVI project that makes it emit its fitted surface and
diagnostics in the T04 output schema?

## Why it matters

Without this the benchmark has no baseline to compare against. The CVI fitter currently lives
inside `CVI Vol Surface Fitting.ipynb` with **no export calls** and no CLI entry point, so its
results cannot leave the notebook today.

## Scope

- Decide whether to extract the fitting code into an importable module or to add an export cell to
  the notebook, weighing that `CVI-Vol-Surface-Fitting` is a **public** repo and any change there is
  visible once pushed.
- Emit fitted values on both the quote set and the dense grid required by T04.
- Keep the change additive: do not alter CVI's fitting behaviour, or the baseline stops being the
  thing you already trust.

## Done when

Running CVI on `option_chain.csv` produces output files that validate against the T04 schema, and
the change is confined to an agreed location.
