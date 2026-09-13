# Benchmark run

Run on WSL2/GCC against the 2024-11-11 snapshot.

| model | included/scored quotes | bid/ask containment | butterfly violations | calendar violations | grid points |
|---|---:|---:|---:|---:|---:|
| model | common scored quotes | bid/ask containment | butterfly violations | calendar violations | grid points | fit time (s) |
|---|---:|---:|---:|---:|---:|---:|
| eSSVI | 7,365 | 0.6925% | 0 | 0 | 5,151 | 2.389 |
| CVI | 7,365 | 0.2444% | 2,138 | 2,398 | 5,000 | 3.177 |

Both outputs are adapted to the strict 14,364-row fitted-quote schema; scoring uses the common 7,365 rows for which both models produced fitted values. CVI was run through its existing notebook and additive export cell. Timings are indicative only and are not a decisive cross-language metric. Tail/grid metadata is normalized by the shared adapter before scoring.

The legacy CVI notebook still fits only selected OTM rows and permits a different set of one-sided inputs internally. The common-intersection policy makes the reported comparison auditable but means the single-snapshot result should not be generalized beyond this benchmark dataset.
