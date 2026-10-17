# Calibrator speedup delivery tickets

Source of truth: the approved [implementation spec](../implementation-spec.md). Decision-only intake; tickets cite `DEC-*` IDs only.

| Ticket | Depends on |
| --- | --- |
| [CAL-B1 — Metrics and baseline](CAL-B1.md) | None |
| [CAL-B7 — Measured scenarios and replay](CAL-B7.md) | [CAL-B1](CAL-B1.md) |
| [CAL-B2 — Per-quote gate and reuse](CAL-B2.md) | [CAL-B7](CAL-B7.md) |
| [CAL-B3 — Local solver and reasons](CAL-B3.md) | [CAL-B2](CAL-B2.md) |
| [CAL-B4 — Staged bounded fallback](CAL-B4.md) | [CAL-B3](CAL-B3.md) |
| [CAL-B5 — Regression coverage](CAL-B5.md) | [CAL-B4](CAL-B4.md) |
| [CAL-B6 — Cold-path A/B](CAL-B6.md) | [CAL-B5](CAL-B5.md) |

Only the frontier ticket with all dependencies complete may be claimed. `docs/delivery/DELIVERY.md` requires one ticket at a time in the primary checkout.
