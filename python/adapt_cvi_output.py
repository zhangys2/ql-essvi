#!/usr/bin/env python3
"""Adapt the additive CVI fitted-quotes export to the shared quote schema."""
import argparse
import csv
import math


def num(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


parser = argparse.ArgumentParser()
parser.add_argument("normalized")
parser.add_argument("cvi")
parser.add_argument("output")
args = parser.parse_args()

fit = {}
with open(args.cvi, newline="", encoding="utf-8") as handle:
    for row in csv.DictReader(handle):
        key = (round(num(row["time_to_expiry"]), 6), round(num(row["strike"]), 8))
        fit[key] = row

fields = ["quote_id", "time_to_expiry", "strike", "log_moneyness", "fitted_total_variance", "fitted_iv", "status"]
with open(args.output, "w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(handle, fieldnames=fields)
    writer.writeheader()
    with open(args.normalized, newline="", encoding="utf-8") as source:
        for quote in csv.DictReader(source):
            key = (round(num(quote["time_to_expiry"]), 6), round(num(quote["strike"]), 8))
            row = fit.get(key)
            log_k = quote.get("log_moneyness", "")
            if not log_k and num(quote.get("forward")) and num(quote.get("strike")):
                log_k = math.log(num(quote["strike"]) / num(quote["forward"]))
            status = "ok" if row and quote["included"] == "true" else ("excluded" if quote["included"] != "true" else "not_fitted")
            writer.writerow({
                "quote_id": quote["quote_id"],
                "time_to_expiry": quote["time_to_expiry"] if status == "ok" else "",
                "strike": quote["strike"] if status == "ok" else "",
                "log_moneyness": log_k if status != "excluded" else "",
                "fitted_total_variance": row.get("fitted_total_variance", "") if row else "",
                "fitted_iv": row.get("fitted_iv", "") if row else "",
                "status": status,
            })
