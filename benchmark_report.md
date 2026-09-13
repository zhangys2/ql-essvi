# Benchmark run

Run on WSL2/GCC against the 2024-11-11 snapshot.

| model | included/scored quotes | bid/ask containment | butterfly violations | calendar violations | grid points |
|---|---:|---:|---:|---:|---:|
| eSSVI | 7,740 | 0.7106% | 0 | 0 | 5,151 |
| CVI | 7,631 | 0.0262% | 2,138 | 2,398 | 5,000 |

The eSSVI fit used hard explicit sufficient constraints and NLopt SLSQP. CVI was run through its existing notebook and its additive export cell. Timings are intentionally not compared because cross-language timing is indicative only.

The quote counts differ by 109 because CVI's existing notebook permits some one-sided bid/ask inputs that the strict shared IV-inversion filter excludes. This is a known fairness gap requiring a follow-up adapter decision before treating the scores as a definitive model comparison. The reported values are therefore a pipeline validation run, not a final scientific conclusion.
