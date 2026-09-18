#!/usr/bin/env python3
"""Adapt the additive CVI fitted-quotes export to the shared quote schema."""
import argparse
import csv
import math
from collections import defaultdict


def num(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def interp_smile(points, k):
    pts = sorted((p for p in points if p[0] is not None and p[1] is not None), key=lambda p: p[0])
    if len(pts) < 2:
        return None
    if k <= pts[0][0]:
        return pts[0]
    if k >= pts[-1][0]:
        return pts[-1]
    for (k0, w0, iv0), (k1, w1, iv1) in zip(pts, pts[1:]):
        if k0 <= k <= k1:
            u = 0.0 if k1 == k0 else (k - k0) / (k1 - k0)
            w = w0 + u * (w1 - w0)
            t_iv = iv0 + u * (iv1 - iv0) if iv0 is not None and iv1 is not None else None
            return (k, w, t_iv)
    return None


parser = argparse.ArgumentParser()
parser.add_argument("normalized")
parser.add_argument("cvi")
parser.add_argument("output")
args = parser.parse_args()

fit = {}
smiles = defaultdict(list)
with open(args.cvi, newline="", encoding="utf-8") as handle:
    for row in csv.DictReader(handle):
        t = num(row["time_to_expiry"])
        strike = num(row["strike"])
        w = num(row.get("fitted_total_variance"))
        iv = num(row.get("fitted_iv"))
        if t is None or strike is None or w is None:
            continue
        key = (round(t, 6), round(strike, 8))
        fit[key] = row
        k = num(row.get("log_moneyness"))
        if k is None:
            continue
        smiles[round(t, 6)].append((k, w, iv))

fields = ["quote_id", "time_to_expiry", "strike", "log_moneyness", "fitted_total_variance", "fitted_iv", "status"]
with open(args.output, "w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(handle, fieldnames=fields)
    writer.writeheader()
    with open(args.normalized, newline="", encoding="utf-8") as source:
        for quote in csv.DictReader(source):
            t = num(quote["time_to_expiry"])
            strike = num(quote["strike"])
            key = (round(t, 6), round(strike, 8)) if t is not None and strike is not None else None
            row = fit.get(key) if key else None
            log_k = quote.get("log_moneyness", "")
            if not log_k and num(quote.get("forward")) and strike:
                log_k = math.log(strike / num(quote["forward"]))
            status = "ok" if row and quote["included"] == "true" else (
                "excluded" if quote["included"] != "true" else "not_fitted")
            w = num(row.get("fitted_total_variance")) if row else None
            iv = num(row.get("fitted_iv")) if row else None
            if status == "not_fitted" and t is not None and log_k != "":
                got = interp_smile(smiles[round(t, 6)], float(log_k))
                if got is not None:
                    _, w, iv = got
                    if iv is None and w is not None and t > 0:
                        iv = math.sqrt(max(w, 0.0) / t)
                    status = "ok"
            writer.writerow({
                "quote_id": quote["quote_id"],
                "time_to_expiry": f"{t:.17f}" if status == "ok" and t is not None else "",
                "strike": f"{strike:.17f}" if status == "ok" and strike is not None else "",
                "log_moneyness": f"{float(log_k):.17f}" if status != "excluded" and log_k != "" else "",
                "fitted_total_variance": f"{w:.17f}" if status == "ok" and w is not None else "",
                "fitted_iv": f"{iv:.17f}" if status == "ok" and iv is not None else "",
                "status": status,
            })
