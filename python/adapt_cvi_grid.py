#!/usr/bin/env python3
"""Adapt CVI grid output to the shared dense-grid contract."""
import argparse
import csv
import math


def f(value):
    return float(value)


parser = argparse.ArgumentParser()
parser.add_argument("normalized")
parser.add_argument("cvi_grid")
parser.add_argument("output")
args = parser.parse_args()

meta = []
for row in csv.DictReader(open(args.normalized, newline="", encoding="utf-8")):
    t = f(row["time_to_expiry"])
    item = next((x for x in meta if abs(x["t"] - t) < 1e-8), None)
    if item is None:
        item = {"t": t, "forward": f(row["forward"]), "discount_factor": f(row["discount_factor"]), "lo": math.inf, "hi": -math.inf}
        meta.append(item)
    if row["included"] == "true":
        k = math.log(f(row["strike"]) / f(row["forward"]))
        item["lo"] = min(item["lo"], k)
        item["hi"] = max(item["hi"], k)


def nearest(t):
    return min(meta, key=lambda x: abs(x["t"] - t))


fields = ["time_to_expiry", "forward", "discount_factor", "log_moneyness", "strike", "fitted_total_variance", "fitted_iv", "is_tail"]
with open(args.output, "w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(handle, fieldnames=fields)
    writer.writeheader()
    for row in csv.DictReader(open(args.cvi_grid, newline="", encoding="utf-8")):
        t = f(row["time_to_expiry"])
        m = nearest(t)
        k = f(row["log_moneyness"])
        row.update(
            time_to_expiry=f"{t:.17f}",
            forward=f"{m['forward']:.17f}",
            discount_factor=f"{m['discount_factor']:.17f}",
            strike=f"{m['forward'] * math.exp(k):.17f}",
            is_tail="true" if k < m["lo"] or k > m["hi"] else "false",
        )
        writer.writerow({field: row.get(field, "") for field in fields})
