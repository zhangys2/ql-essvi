#!/usr/bin/env python3
"""Convert IBIT chain snapshots to normalized quote CSVs for replay.

Prices are inverted with European Black-Scholes using spot as the forward and
unit discounting. The resulting IVs are stand-ins for American IVs; they are
intended for calibration-path benchmarking, not pricing-accuracy claims.
"""

import argparse
import csv
import datetime as dt
import gzip
import json
import math
from pathlib import Path

HEADER = [
    "quote_id",
    "effective_date",
    "expiry_date",
    "time_to_expiry",
    "forward",
    "discount_factor",
    "strike",
    "option_type",
    "bid_iv",
    "ask_iv",
    "mid_iv",
    "included",
    "filter_reason",
]
UTC = dt.timezone.utc


def finite_number(value):
    if value is None:
        return None
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def snapshot_time(snapshot):
    milliseconds = finite_number(snapshot.get("ts"))
    if milliseconds is not None:
        seconds = milliseconds / 1000.0 if milliseconds > 1e11 else milliseconds
        return dt.datetime.fromtimestamp(seconds, tz=UTC)
    value = snapshot.get("iso")
    if not value:
        raise ValueError("snapshot has neither a numeric ts nor an iso timestamp")
    return dt.datetime.fromisoformat(str(value).replace("Z", "+00:00")).astimezone(UTC)


def expiry_date(value):
    text = str(value)
    if len(text) == 8 and text.isdigit():
        return dt.datetime.strptime(text, "%Y%m%d").date()
    return dt.date.fromisoformat(text[:10])


def norm_cdf(value):
    return 0.5 * math.erfc(-value / math.sqrt(2.0))


def black_scholes_price(forward, strike, time_years, volatility, is_call):
    if volatility <= 0.0 or time_years <= 0.0:
        return max(forward - strike if is_call else strike - forward, 0.0)
    sigma_root_t = volatility * math.sqrt(time_years)
    d1 = (math.log(forward / strike) + 0.5 * sigma_root_t**2) / sigma_root_t
    d2 = d1 - sigma_root_t
    if is_call:
        return forward * norm_cdf(d1) - strike * norm_cdf(d2)
    return strike * norm_cdf(-d2) - forward * norm_cdf(-d1)


def implied_volatility(price, forward, strike, time_years, is_call):
    if not all(math.isfinite(value) for value in (price, forward, strike, time_years)):
        return None
    if forward <= 0.0 or strike <= 0.0 or time_years <= 0.0:
        return None
    intrinsic = max(forward - strike if is_call else strike - forward, 0.0)
    upper_bound = forward if is_call else strike
    if not intrinsic < price < upper_bound:
        return None

    low, high = 1e-8, 5.0
    if black_scholes_price(forward, strike, time_years, high, is_call) < price:
        return None
    for _ in range(80):
        middle = 0.5 * (low + high)
        if black_scholes_price(forward, strike, time_years, middle, is_call) < price:
            low = middle
        else:
            high = middle
    return 0.5 * (low + high)


def quote_row(contract, quote_index, effective_date, expiry_value, time_years, forward):
    identifier = contract.get("conId") or contract.get("localSymbol") or quote_index
    right = str(contract.get("right") or "").upper()
    option_type = right if right in ("C", "P") else ""
    strike = finite_number(contract.get("strike"))
    bid = finite_number(contract.get("bid"))
    ask = finite_number(contract.get("ask"))
    reason = ""
    bid_iv = ask_iv = None

    if not option_type or strike is None or strike <= 0.0 or forward <= 0.0:
        reason = "invalid_contract_or_forward"
    elif time_years <= 0.0:
        reason = "expired_or_same_day"
    elif bid is None or ask is None or bid < 0.0 or ask < bid or ask <= 0.0:
        reason = "invalid_or_crossed_quote"
    elif (option_type == "C" and strike <= forward) or (
        option_type == "P" and strike >= forward
    ):
        reason = "not_out_of_the_money"
    else:
        is_call = option_type == "C"
        bid_iv = implied_volatility(bid, forward, strike, time_years, is_call)
        ask_iv = implied_volatility(ask, forward, strike, time_years, is_call)
        if bid_iv is None or ask_iv is None:
            reason = "iv_inversion_failed"

    included = not reason
    if not included:
        bid_iv = ask_iv = None
    mid_iv = (bid_iv + ask_iv) / 2.0 if included else None
    format_number = lambda value: "" if value is None else format(value, ".17g")
    return [
        str(identifier),
        effective_date.isoformat(),
        expiry_value,
        format_number(time_years if time_years > 0.0 else None),
        format_number(forward),
        "1",
        format_number(strike),
        option_type,
        format_number(bid_iv),
        format_number(ask_iv),
        format_number(mid_iv),
        "true" if included else "false",
        reason,
    ], included


def convert_snapshot(snapshot, output_dir, source_name, line_number):
    timestamp = snapshot_time(snapshot)
    effective_date = timestamp.date()
    stamp_ms = int(timestamp.timestamp() * 1000.0)
    output_path = output_dir / f"normalized_quotes_{stamp_ms:013d}.csv"
    spot = finite_number(snapshot.get("spot_ref"))
    if spot is None or spot <= 0.0:
        underlying = snapshot.get("underlying_start") or {}
        bid = finite_number(underlying.get("bid"))
        ask = finite_number(underlying.get("ask"))
        last = finite_number(underlying.get("last"))
        spot = (bid + ask) / 2.0 if bid and ask else last
    if spot is None or spot <= 0.0:
        raise ValueError(f"{source_name}:{line_number}: no valid spot reference")

    contracts = snapshot.get("contracts")
    if not isinstance(contracts, list):
        raise ValueError(f"{source_name}:{line_number}: contracts is not a list")

    rows = []
    valid_quotes = 0
    for index, contract in enumerate(contracts, start=1):
        raw_expiry = contract.get("expiry")
        expiry_text = ""
        time_years = -1.0
        if raw_expiry:
            try:
                expiry = expiry_date(raw_expiry)
                expiry_text = expiry.isoformat()
                # Match the repository's date-based year fraction so slices keep
                # identical expiry keys across snapshots from the same day.
                time_years = (expiry - effective_date).days / 365.0
            except (TypeError, ValueError):
                pass
        row, included = quote_row(
            contract,
            index,
            effective_date,
            expiry_text,
            time_years,
            spot,
        )
        rows.append(row)
        valid_quotes += included

    if valid_quotes == 0:
        return None, len(rows), valid_quotes

    with output_path.open("x", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(HEADER)
        writer.writerows(rows)
    return output_path, len(rows), valid_quotes


def convert_directory(input_dir, output_dir, snapshot_limit=None):
    paths = sorted(input_dir.glob("chain_*.jsonl.gz"))
    if not paths:
        raise FileNotFoundError(f"no chain_*.jsonl.gz files in {input_dir}")

    converted = 0
    for path in paths:
        with gzip.open(path, "rt", encoding="utf-8") as stream:
            for line_number, line in enumerate(stream, start=1):
                if not line.strip():
                    continue
                snapshot = json.loads(line)
                output_path, rows, valid = convert_snapshot(
                    snapshot, output_dir, path, line_number
                )
                if output_path is None:
                    print(
                        f"{path}:{line_number} -> skipped: "
                        "no valid OTM two-sided quotes; "
                        "Black-Scholes European stand-in for American IVs"
                    )
                else:
                    print(
                        f"{path}:{line_number} -> {output_path}: "
                        f"{valid}/{rows} valid OTM two-sided quotes; "
                        "Black-Scholes European stand-in for American IVs"
                    )
                converted += 1
                if snapshot_limit is not None and converted >= snapshot_limit:
                    return converted
    return converted


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=Path.home() / "vol-rv" / "data" / "ibit",
    )
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--snapshot-limit",
        type=int,
        help="maximum number of snapshots to convert (for smoke runs)",
    )
    args = parser.parse_args()
    if args.snapshot_limit is not None and args.snapshot_limit <= 0:
        parser.error("--snapshot-limit must be positive")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    convert_directory(args.input_dir, args.output_dir, args.snapshot_limit)


if __name__ == "__main__":
    main()
