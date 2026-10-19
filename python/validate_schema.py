#!/usr/bin/env python3
"""Validate benchmark CSV shape, types, and missing-value semantics."""
import argparse
import csv
import datetime as dt
import math
import sys

SCHEMAS = {
    "normalized": ["quote_id", "effective_date", "expiry_date", "time_to_expiry", "forward", "discount_factor", "strike", "option_type", "bid_iv", "ask_iv", "mid_iv", "included", "filter_reason"],
    "fitted": ["quote_id", "time_to_expiry", "strike", "log_moneyness", "fitted_total_variance", "fitted_iv", "status"],
    "grid": ["time_to_expiry", "forward", "discount_factor", "log_moneyness", "strike", "fitted_total_variance", "fitted_iv", "is_tail"],
    "parameters": ["time_to_expiry", "theta", "psi", "rho", "phi", "butterfly_safe", "calendar_safe"],
    "diagnostics": ["model", "converged", "objective", "iterations", "runtime_seconds", "error"],
    "cvi_parameters": ["time_to_expiry", "atm_total_variance", "observed_min_log_moneyness", "observed_max_log_moneyness", "grid_points", "fit_rows"],
}


def fail(message):
    raise ValueError(message)


def number(value, field, row, *, allow_empty=False):
    if value == "" and allow_empty:
        return None
    try:
        result = float(value)
    except (TypeError, ValueError):
        fail(f"row {row}: {field} is not numeric: {value!r}")
    if not math.isfinite(result) or (field == "weight" and result <= 0.0):
        fail(
            f"row {row}: {field} is not finite"
            if not math.isfinite(result)
            else f"row {row}: weight must be positive"
        )
    return result


def boolean(value, field, row):
    if value not in {"true", "false"}:
        fail(f"row {row}: {field} must be true/false, got {value!r}")
    return value == "true"


def require_precision(value, field, row, digits=12):
    """Require generated decimal fields to retain at least `digits` significant digits."""
    if value == "":
        fail(f"row {row}: {field} is empty")
    if value.lstrip("+-").isdigit():
        return  # An integer literal is exact, not a low-precision rounded decimal.
    mantissa = value.lower().split("e", 1)[0].lstrip("+-").replace(".", "").lstrip("0")
    if len(mantissa) < digits:
        fail(f"row {row}: {field} has fewer than {digits} significant digits: {value!r}")


def validate(kind, rows):
    for i, row in enumerate(rows, 2):
        if kind == "normalized":
            try:
                effective = dt.date.fromisoformat(row["effective_date"])
                expiry = dt.date.fromisoformat(row["expiry_date"])
            except ValueError as exc:
                fail(f"row {i}: invalid ISO date: {exc}")
            included = boolean(row["included"], "included", i)
            expired_exclusion = (
                not included
                and row["filter_reason"] == "expired_or_same_day"
                and expiry <= effective
            )
            if expiry <= effective and not expired_exclusion:
                fail(f"row {i}: expiry_date must be after effective_date")
            if expired_exclusion:
                if row["time_to_expiry"] != "":
                    fail(f"row {i}: expired quote time_to_expiry must be empty")
            else:
                t = number(row["time_to_expiry"], "time_to_expiry", i)
                if t <= 0:
                    fail(f"row {i}: time_to_expiry must be positive")
            for field in ("forward", "discount_factor", "strike"):
                if number(row[field], field, i) <= 0:
                    fail(f"row {i}: {field} must be positive")
            if row["option_type"] not in {"C", "P"}:
                fail(f"row {i}: option_type must be C/P")
            if "weight" in row and not included and row["weight"] != "":
                fail(f"row {i}: excluded quote weight must be empty")
            fields = ("bid_iv", "ask_iv", "mid_iv")
            if "weight" in row:
                fields += ("weight",)
            for field in fields:
                number(row[field], field, i, allow_empty=not included)
            if included and row["filter_reason"]:
                fail(f"row {i}: included quote cannot have filter_reason")
            if not included and not row["filter_reason"]:
                fail(f"row {i}: excluded quote needs filter_reason")
        elif kind == "fitted":
            status = row["status"]
            if status not in {"ok", "excluded", "not_fitted", "error"}:
                fail(f"row {i}: unsupported fitted status {status!r}")
            allow_empty = status != "ok"
            for field in ("time_to_expiry", "strike", "log_moneyness"):
                number(row[field], field, i, allow_empty=allow_empty)
                if not allow_empty:
                    require_precision(row[field], field, i)
            for field in ("fitted_total_variance", "fitted_iv"):
                number(row[field], field, i, allow_empty=allow_empty)
                if not allow_empty:
                    require_precision(row[field], field, i)
        elif kind == "grid":
            for field in ("time_to_expiry", "forward", "discount_factor", "log_moneyness", "strike", "fitted_total_variance", "fitted_iv"):
                number(row[field], field, i)
                if field != "log_moneyness":
                    require_precision(row[field], field, i)
            boolean(row["is_tail"], "is_tail", i)
        elif kind == "parameters":
            for field in ("time_to_expiry", "theta", "psi", "phi"):
                number(row[field], field, i)
                require_precision(row[field], field, i)
            rho = number(row["rho"], "rho", i)
            if not -1 < rho < 1:
                fail(f"row {i}: rho must be strictly between -1 and 1")
            boolean(row["butterfly_safe"], "butterfly_safe", i)
            boolean(row["calendar_safe"], "calendar_safe", i)
        elif kind == "diagnostics":
            if not row["model"]:
                fail(f"row {i}: model is empty")
            boolean(row["converged"], "converged", i)
            number(row["objective"], "objective", i)
            if int(number(row["iterations"], "iterations", i)) < 0:
                fail(f"row {i}: iterations must be nonnegative")
            if number(row["runtime_seconds"], "runtime_seconds", i) < 0:
                fail(f"row {i}: runtime_seconds must be nonnegative")
        elif kind == "cvi_parameters":
            for field in ("time_to_expiry", "atm_total_variance", "observed_min_log_moneyness", "observed_max_log_moneyness"):
                number(row[field], field, i)
            if int(number(row["grid_points"], "grid_points", i)) <= 0 or int(number(row["fit_rows"], "fit_rows", i)) <= 0:
                fail(f"row {i}: grid_points and fit_rows must be positive")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=SCHEMAS)
    parser.add_argument("file")
    parser.add_argument("--expected-rows", type=int)
    args = parser.parse_args()
    try:
        with open(args.file, newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle)
            if reader.fieldnames != SCHEMAS[args.kind] and not (
                args.kind == "normalized"
                and reader.fieldnames == SCHEMAS[args.kind] + ["weight"]
            ):
                fail(f"header mismatch: {reader.fieldnames}")
            rows = list(reader)
        if not rows:
            fail("empty CSV")
        if args.expected_rows is not None and len(rows) != args.expected_rows:
            fail(f"expected {args.expected_rows} rows, got {len(rows)}")
        validate(args.kind, rows)
    except (OSError, ValueError) as exc:
        print(f"{args.kind}: invalid: {exc}", file=sys.stderr)
        return 1
    print(f"{args.kind}: valid schema and semantics, {len(rows)} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
