#!/usr/bin/env python3
"""Resample two model grids onto one identical maturity/k grid for comparison."""
import argparse
import csv
import math
from collections import defaultdict
import numpy as np

FIELDS = ["time_to_expiry", "forward", "discount_factor", "log_moneyness", "strike", "fitted_total_variance", "fitted_iv", "is_tail"]


def read_rows(path):
    with open(path, newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def nearest(groups, t):
    return min(groups, key=lambda x: abs(x - t))


def write_common(normalized, source, output, maturities):
    quotes = read_rows(normalized)
    observed = defaultdict(list)
    for row in quotes:
        if row["included"] == "true":
            observed[round(float(row["time_to_expiry"]), 8)].append(math.log(float(row["strike"]) / float(row["forward"])))
    rows = read_rows(source)
    grouped = defaultdict(list)
    for row in rows:
        grouped[round(float(row["time_to_expiry"]), 8)].append(row)
    with open(output, "w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS)
        writer.writeheader()
        for t in maturities:
            key = nearest(grouped, t)
            source_rows = sorted(grouped[key], key=lambda r: float(r["log_moneyness"]))
            x = np.array([float(r["log_moneyness"]) for r in source_rows])
            w = np.array([float(r["fitted_total_variance"]) for r in source_rows])
            fwd = float(source_rows[0]["forward"])
            discount = float(source_rows[0]["discount_factor"])
            bounds = observed.get(nearest(observed, t), [-math.inf, math.inf]) if observed else [-math.inf, math.inf]
            for i in range(101):
                k = -0.5 + i * 0.01
                variance = float(np.interp(k, x, w))
                writer.writerow({
                    "time_to_expiry": f"{t:.17f}", "forward": f"{fwd:.17f}", "discount_factor": f"{discount:.17f}",
                    "log_moneyness": f"{k:.17f}", "strike": f"{fwd * math.exp(k):.17f}",
                    "fitted_total_variance": f"{variance:.17f}", "fitted_iv": f"{math.sqrt(max(variance, 0.0) / t):.17f}",
                    "is_tail": str(k < min(bounds) or k > max(bounds)).lower(),
                })


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("normalized")
    parser.add_argument("essvi_grid")
    parser.add_argument("cvi_grid")
    parser.add_argument("essvi_output")
    parser.add_argument("cvi_output")
    args = parser.parse_args()
    essvi = read_rows(args.essvi_grid)
    cvi = read_rows(args.cvi_grid)
    et = sorted({float(r["time_to_expiry"]) for r in essvi})
    ct = sorted({float(r["time_to_expiry"]) for r in cvi})
    maturities = [t for t in et if any(abs(t - u) < 1e-4 for u in ct)]
    if not maturities:
        raise SystemExit("no common maturities")
    write_common(args.normalized, args.essvi_grid, args.essvi_output, maturities)
    write_common(args.normalized, args.cvi_grid, args.cvi_output, maturities)
    print(f"common grid: {len(maturities)} maturities x 101 points")


if __name__ == "__main__":
    main()
