#!/usr/bin/env python3
"""Run the cold-only IBIT Addendum C1 diagnosis on the training split."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import gzip
import json
import math
import statistics
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path
from typing import Any, Callable

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))

from convert_ibit_snapshots import HEADER, expiry_date, finite_number, vega_weights
from replay_snapshots import prepare_snapshot

TRAINING_DATES = ("20261012", "20261013", "20261014")
CAP = 0.1


def percentile(values: list[float], p: float) -> float:
    if not values:
        raise ValueError("cannot compute a percentile of an empty sample")
    ordered = sorted(values)
    position = p * (len(ordered) - 1)
    low = int(position)
    high = min(low + 1, len(ordered) - 1)
    fraction = position - low
    return ordered[low] + fraction * (ordered[high] - ordered[low])


def quality(rows: list[dict[str, Any]]) -> dict[str, Any]:
    ratios = [row["excess_ratio"] for row in rows]
    count = len(ratios)
    if not count:
        return {
            "quote_count": 0,
            "above_cap_count": 0,
            "share_above_cap": 0.0,
            "rms_excess_ratio": 0.0,
            "maximum_excess_ratio": 0.0,
            "sum_squared_total_variance_excess": 0.0,
            "sum_squared_vega_weighted_excess": 0.0,
            "vega_weighted_excess_rms": 0.0,
            "vega_weighted_excess_maximum": 0.0,
            "price_excess_ticks_rms": 0.0,
            "price_excess_ticks_maximum": 0.0,
            "price_excess_dollars_per_share_rms": 0.0,
            "price_excess_dollars_per_share_maximum": 0.0,
            "price_excess_dollars_per_contract_rms": 0.0,
            "price_excess_dollars_per_contract_maximum": 0.0,
        }

    excesses = [row["excess"] for row in rows]
    weighted_excesses = [
        row.get("weight", 1.0) * excess
        for row, excess in zip(rows, excesses)
    ]
    price_excesses = [
        row.get("raw_price_sensitivity", 0.0) * excess
        for row, excess in zip(rows, excesses)
    ]
    price_ticks = [value / vega_weights.TICK_SIZE for value in price_excesses]
    price_contracts = [
        value * vega_weights.CONTRACT_MULTIPLIER for value in price_excesses
    ]
    return {
        "quote_count": count,
        "above_cap_count": sum(value > CAP for value in ratios),
        "share_above_cap": sum(value > CAP for value in ratios) / count,
        "rms_excess_ratio": math.sqrt(sum(value * value for value in ratios) / count),
        "maximum_excess_ratio": max(ratios),
        "sum_squared_total_variance_excess": sum(
            value * value for value in excesses
        ),
        "sum_squared_vega_weighted_excess": sum(
            value * value for value in weighted_excesses
        ),
        "vega_weighted_excess_rms": math.sqrt(
            sum(value * value for value in weighted_excesses) / count
        ),
        "vega_weighted_excess_maximum": max(weighted_excesses),
        "price_excess_ticks_rms": math.sqrt(
            sum(value * value for value in price_ticks) / count
        ),
        "price_excess_ticks_maximum": max(price_ticks),
        "price_excess_dollars_per_share_rms": math.sqrt(
            sum(value * value for value in price_excesses) / count
        ),
        "price_excess_dollars_per_share_maximum": max(price_excesses),
        "price_excess_dollars_per_contract_rms": math.sqrt(
            sum(value * value for value in price_contracts) / count
        ),
        "price_excess_dollars_per_contract_maximum": max(price_contracts),
    }


def grouped_quality(
    rows: list[dict[str, Any]], key: Callable[[dict[str, Any]], str]
) -> dict[str, dict[str, float | int]]:
    groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        groups[key(row)].append(row)
    return {name: quality(group) for name, group in sorted(groups.items())}


def linear_regression(xs: list[float], ys: list[float]) -> dict[str, float]:
    if len(xs) != len(ys) or len(xs) < 2:
        raise ValueError("regression requires matching samples of size at least two")
    mean_x = statistics.fmean(xs)
    mean_y = statistics.fmean(ys)
    xx = sum((value - mean_x) ** 2 for value in xs)
    yy = sum((value - mean_y) ** 2 for value in ys)
    xy = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
    return {
        "slope": xy / xx if xx else 0.0,
        "r_squared": xy * xy / (xx * yy) if xx and yy else 0.0,
    }


def within_snapshot_regression(
    groups: dict[str, list[dict[str, Any]]],
    position: Callable[[dict[str, Any]], float | None],
) -> dict[str, Any]:
    centered_x: list[float] = []
    centered_y: list[float] = []
    slopes: list[float] = []
    for group in groups.values():
        usable = [row for row in group if position(row) is not None]
        if len(usable) < 2:
            continue
        xs = [float(position(row)) for row in usable]
        ys = [row["signed_residual"] for row in usable]
        slopes.append(linear_regression(xs, ys)["slope"])
        mean_x = statistics.fmean(xs)
        mean_y = statistics.fmean(ys)
        centered_x.extend(value - mean_x for value in xs)
        centered_y.extend(value - mean_y for value in ys)
    pooled = linear_regression(centered_x, centered_y)
    return {
        "snapshot_count": len(slopes),
        "within_snapshot_pooled": pooled,
        "snapshot_slope_p10": percentile(slopes, 0.10),
        "snapshot_slope_median": percentile(slopes, 0.50),
        "snapshot_slope_p90": percentile(slopes, 0.90),
    }


def expiry_bucket(days: int) -> str:
    if days <= 7:
        return "0-7d"
    if days <= 30:
        return "8-30d"
    if days <= 90:
        return "31-90d"
    if days <= 180:
        return "91-180d"
    return ">180d"


def moneyness_bucket(value: float) -> str:
    if value <= -0.20:
        return "<=-0.20"
    if value <= -0.10:
        return "(-0.20,-0.10]"
    if value <= -0.05:
        return "(-0.10,-0.05]"
    if value < 0.0:
        return "(-0.05,0)"
    if value <= 0.05:
        return "(0,0.05]"
    if value <= 0.10:
        return "(0.05,0.10]"
    if value <= 0.20:
        return "(0.10,0.20]"
    return ">0.20"


def premium_bucket(value: float) -> str:
    if value < 0.05:
        return "<$0.05"
    if value < 0.25:
        return "$0.05-$0.25"
    if value < 1.0:
        return "$0.25-$1"
    if value < 5.0:
        return "$1-$5"
    return ">=$5"


def spread_bucket(value: float) -> str:
    if value < 0.25:
        return "<0.25"
    if value < 0.50:
        return "0.25-0.5"
    if value < 1.0:
        return "0.5-1"
    if value < 2.0:
        return "1-2"
    return ">=2"


def relative_total_variance_band_width(
    bid_variance: float, ask_variance: float
) -> float:
    midpoint_variance = 0.5 * (bid_variance + ask_variance)
    if midpoint_variance <= 0.0:
        raise ValueError("midpoint total variance must be positive")
    return (ask_variance - bid_variance) / midpoint_variance


def relative_total_variance_band_bucket(value: float) -> str:
    if value < 0.25:
        return "<0.25"
    if value < 0.50:
        return "0.25-0.5"
    if value < 1.0:
        return "0.5-1"
    if value < 2.0:
        return "1-2"
    return ">=2"


def grouped_band_width_quality(
    rows: list[dict[str, Any]],
) -> dict[str, dict[str, float | int]]:
    return grouped_quality(
        rows,
        lambda row: relative_total_variance_band_bucket(
            row["relative_total_variance_band_width"]
        ),
    )


def contribution_percentages(
    groups: dict[str, dict[str, dict[str, Any]]], total_excess: float
) -> dict[str, dict[str, float | None]]:
    if total_excess == 0.0:
        return {
            dimension: {key: None for key in metrics}
            for dimension, metrics in groups.items()
        }
    return {
        dimension: {
            key: 100.0 * metrics["sum_squared_total_variance_excess"] / total_excess
            for key, metrics in buckets.items()
        }
        for dimension, buckets in groups.items()
    }


def underlying_mid(value: dict[str, Any]) -> float | None:
    bid = finite_number(value.get("bid"))
    ask = finite_number(value.get("ask"))
    last = finite_number(value.get("last"))
    if bid is not None and ask is not None and bid > 0.0 and ask > 0.0:
        return 0.5 * (bid + ask)
    return last if last is not None and last > 0.0 else None


def source_contract_index(
    contracts: list[Any],
) -> dict[str, tuple[int, dict[str, Any]]]:
    indexed = {}
    for index, contract in enumerate(contracts, start=1):
        if not isinstance(contract, dict):
            continue
        identifier = str(
            contract.get("conId") or contract.get("localSymbol") or index
        )
        if identifier in indexed:
            raise ValueError(f"duplicate quote id {identifier}")
        indexed[identifier] = (index, contract)
    return indexed


def fit_snapshot(
    snapshot: dict[str, Any],
    source_file: Path,
    fit_binary: Path,
    sequence: int,
    *,
    cleaning_rules: tuple[str, ...] = (),
    weight_mode: str = "unit",
) -> tuple[Any, list[dict[str, Any]]]:
    prepared = prepare_snapshot(
        snapshot, "IBIT", str(source_file), cleaning_rules=cleaning_rules
    )
    if not prepared.valid_quotes:
        return prepared, []
    source_contracts = source_contract_index(snapshot["contracts"])

    with tempfile.TemporaryDirectory(prefix="cal-c1-") as temp_name:
        temp = Path(temp_name)
        stem = f"{sequence:03d}"
        normalized = temp / f"{stem}-normalized.csv"
        parameters = temp / f"{stem}-parameters.csv"
        fitted = temp / f"{stem}-fitted.csv"
        grid = temp / f"{stem}-grid.csv"
        diagnostics = temp / f"{stem}-diagnostics.csv"
        with normalized.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(HEADER)
            writer.writerows(
                vega_weights.rows_for_weight_mode(prepared.csv_rows, weight_mode)
            )
        subprocess.run(
            [
                str(fit_binary),
                str(normalized),
                str(parameters),
                str(fitted),
                str(grid),
                str(diagnostics),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        with diagnostics.open(newline="", encoding="utf-8") as stream:
            diagnostic_rows = list(csv.DictReader(stream))
        if len(diagnostic_rows) != 1 or diagnostic_rows[0]["converged"] != "true":
            raise RuntimeError(f"{source_file}: cold fit did not converge")
        with fitted.open(newline="", encoding="utf-8") as stream:
            fit_by_id = {
                row["quote_id"]: float(row["fitted_total_variance"])
                for row in csv.DictReader(stream)
                if row["status"] == "ok"
            }

    session_day = dt.date.fromisoformat(prepared.session_date)
    contract_count = len(snapshot["contracts"])
    tick_values = [
        finite_number(contract.get("tick_time"))
        for contract in snapshot["contracts"]
        if isinstance(contract, dict)
    ]
    observed_ticks = [value for value in tick_values if value is not None]
    min_tick = min(observed_ticks) if observed_ticks else None
    max_tick = max(observed_ticks) if observed_ticks else None
    observations = []
    for row in prepared.csv_rows:
        if row[11] != "true":
            continue
        identifier = row[0]
        if identifier not in fit_by_id or identifier not in source_contracts:
            raise ValueError(f"{source_file}: fitted quote missing for id {identifier}")
        index, contract = source_contracts[identifier]
        bid = finite_number(contract.get("bid"))
        ask = finite_number(contract.get("ask"))
        if bid is None or ask is None:
            raise ValueError(f"{source_file}: included quote lacks a two-sided market")
        time_years = float(row[3])
        forward = float(row[4])
        strike = float(row[6])
        bid_iv = float(row[8])
        ask_iv = float(row[9])
        mid_iv = float(row[10])
        fitted_variance = fit_by_id[identifier]
        bid_variance = bid_iv * bid_iv * time_years
        ask_variance = ask_iv * ask_iv * time_years
        width = ask_variance - bid_variance
        relative_variance_width = relative_total_variance_band_width(
            bid_variance, ask_variance
        )
        excess = max(bid_variance - fitted_variance, 0.0, fitted_variance - ask_variance)
        residual = fitted_variance - 0.5 * (bid_variance + ask_variance)
        log_moneyness = math.log(strike / forward)
        days = max((expiry_date(contract["expiry"]) - session_day).days, 1)
        premium = 0.5 * (bid + ask)
        relative_spread = (ask - bid) / premium if premium else math.inf
        sigma_root_t = mid_iv * math.sqrt(time_years)
        d1 = (-log_moneyness + 0.5 * sigma_root_t**2) / sigma_root_t
        cdf_d1 = 0.5 * math.erfc(-d1 / math.sqrt(2.0))
        abs_delta = abs(cdf_d1 if row[7] == "C" else cdf_d1 - 1.0)
        tick = finite_number(contract.get("tick_time"))
        tick_position = (
            (tick - min_tick) / (max_tick - min_tick)
            if tick is not None and min_tick is not None and max_tick is not None
            and max_tick > min_tick
            else None
        )
        observations.append(
            {
                "snapshot_id": f"{source_file.name}:{prepared.timestamp_ms}",
                "session_date": prepared.session_date,
                "quote_id": identifier,
                "source_position": (index - 1) / max(contract_count - 1, 1),
                "tick_position": tick_position,
                "expiry_days": days,
                "log_moneyness": log_moneyness,
                "premium": premium,
                "relative_spread": relative_spread,
                "relative_total_variance_band_width": relative_variance_width,
                "abs_delta": abs_delta,
                "bid": bid,
                "option_type": row[7],
                "signed_residual": residual,
                "excess": excess,
                "excess_ratio": excess / max(width, 1e-6),
                "weight": float(row[13]),
                "raw_price_sensitivity": vega_weights.raw_price_sensitivity(
                    mid_iv, forward, strike, time_years, float(row[5])
                ),
                "fit_objective": float(diagnostic_rows[0]["objective"]),
            }
        )
    if len(observations) != prepared.valid_quotes:
        raise ValueError(f"{source_file}: prepared/fitted quote count mismatch")
    return prepared, observations


def filter_summary(
    rows: list[dict[str, Any]],
    snapshots: dict[str, list[dict[str, Any]]],
    keep: Callable[[dict[str, Any]], bool],
) -> dict[str, Any]:
    retained = [row for row in rows if keep(row)]
    excluded_by_snapshot = [
        sum(not keep(row) for row in group) for group in snapshots.values()
    ]
    return {
        "retained_count": len(retained),
        "excluded_count": len(rows) - len(retained),
        "excluded_fraction": (len(rows) - len(retained)) / len(rows),
        "excluded_per_snapshot": {
            "p10": percentile(excluded_by_snapshot, 0.10),
            "median": statistics.median(excluded_by_snapshot),
            "p90": percentile(excluded_by_snapshot, 0.90),
            "maximum": max(excluded_by_snapshot),
        },
        "retained_quality_under_full_data_fit": quality(retained),
    }


def analyze(data_root: Path, fit_binary: Path) -> dict[str, Any]:
    files = [data_root / "ibit" / f"chain_{date}.jsonl.gz" for date in TRAINING_DATES]
    missing = [str(path) for path in files if not path.is_file()]
    if missing:
        raise FileNotFoundError("missing training snapshot files: " + ", ".join(missing))
    if not fit_binary.is_file():
        raise FileNotFoundError(f"cold-fit binary not found: {fit_binary}")

    rows: list[dict[str, Any]] = []
    snapshot_groups: dict[str, list[dict[str, Any]]] = {}
    day_groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
    residual_move_x: list[float] = []
    residual_move_y: list[float] = []
    signed_moves: list[float] = []
    fetch_durations: list[float] = []
    coverage: dict[str, Any] = {
        "asset": "IBIT",
        "training_dates": list(TRAINING_DATES),
        "source_files": [path.name for path in files],
        "snapshot_count": 0,
        "cold_fits_succeeded": 0,
        "input_contract_count": 0,
        "prepared_rows_excluding_locked_crossed": 0,
        "valid_quotes": 0,
        "excluded_locked": 0,
        "excluded_crossed": 0,
        "excluded_other": 0,
    }
    sequence = 0
    for source_file in files:
        with gzip.open(source_file, "rt", encoding="utf-8") as stream:
            for line in stream:
                snapshot = json.loads(line)
                sequence += 1
                coverage["snapshot_count"] += 1
                coverage["input_contract_count"] += len(snapshot.get("contracts", []))
                prepared, observations = fit_snapshot(
                    snapshot, source_file, fit_binary, sequence
                )
                coverage["excluded_locked"] += prepared.excluded_locked
                coverage["excluded_crossed"] += prepared.excluded_crossed
                coverage["excluded_other"] += prepared.excluded_other
                coverage["prepared_rows_excluding_locked_crossed"] += len(
                    prepared.csv_rows
                )
                if not observations:
                    continue
                coverage["cold_fits_succeeded"] += 1
                coverage["valid_quotes"] += len(observations)
                snapshot_id = observations[0]["snapshot_id"]
                snapshot_groups[snapshot_id] = observations
                day_groups[prepared.session_date].extend(observations)
                rows.extend(observations)
                start = underlying_mid(snapshot.get("underlying_start") or {})
                end = underlying_mid(snapshot.get("underlying_end") or {})
                if start is not None and end is not None:
                    move = math.log(end / start)
                    residual_move_x.append(move)
                    residual_move_y.append(
                        statistics.median(row["signed_residual"] for row in observations)
                    )
                    signed_moves.append(move)
                fetch_start = finite_number(snapshot.get("fetch_start_ms"))
                fetch_end = finite_number(snapshot.get("fetch_end_ms"))
                if fetch_start is not None and fetch_end is not None:
                    fetch_durations.append((fetch_end - fetch_start) / 1000.0)

    classified = (
        coverage["valid_quotes"]
        + coverage["excluded_locked"]
        + coverage["excluded_crossed"]
        + coverage["excluded_other"]
    )
    if classified != coverage["input_contract_count"]:
        raise ValueError("prepared quote/exclusion counts do not cover the input contracts")
    if not rows:
        raise ValueError("training split produced no valid quotes")

    spread_p95 = percentile([row["relative_spread"] for row in rows], 0.95)
    filter_rules: dict[str, Callable[[dict[str, Any]], bool]] = {
        "bid_gt_zero": lambda row: row["bid"] > 0.0,
        "mid_premium_ge_0.05": lambda row: row["premium"] >= 0.05,
        "mid_premium_ge_0.10": lambda row: row["premium"] >= 0.10,
        "relative_spread_le_training_p95": lambda row: row["relative_spread"] <= spread_p95,
        "abs_bs_delta_0.02_to_0.95": lambda row: 0.02 <= row["abs_delta"] <= 0.95,
        "abs_bs_delta_0.05_to_0.95": lambda row: 0.05 <= row["abs_delta"] <= 0.95,
        "combined_bid_premium_spread_delta_0.02": lambda row: (
            row["bid"] > 0.0
            and row["premium"] >= 0.05
            and row["relative_spread"] <= spread_p95
            and 0.02 <= row["abs_delta"] <= 0.95
        ),
        "combined_bid_premium_spread_delta_0.05": lambda row: (
            row["bid"] > 0.0
            and row["premium"] >= 0.05
            and row["relative_spread"] <= spread_p95
            and 0.05 <= row["abs_delta"] <= 0.95
        ),
    }
    base_quality = quality(rows)
    grouped = {
        "expiry_days": grouped_quality(
            rows, lambda row: expiry_bucket(row["expiry_days"])
        ),
        "log_moneyness": grouped_quality(
            rows, lambda row: moneyness_bucket(row["log_moneyness"])
        ),
        "mid_premium_usd": grouped_quality(
            rows, lambda row: premium_bucket(row["premium"])
        ),
        "relative_spread": grouped_quality(
            rows, lambda row: spread_bucket(row["relative_spread"])
        ),
        "relative_total_variance_band_width": grouped_band_width_quality(rows),
        "option_type": grouped_quality(rows, lambda row: row["option_type"]),
    }
    total_excess = base_quality["sum_squared_total_variance_excess"]
    contributions = contribution_percentages(grouped, total_excess)
    move_regression = linear_regression(residual_move_x, residual_move_y)
    return {
        "coverage": coverage,
        "fit_quality": base_quality,
        "by_session_date": {
            day: quality(group) for day, group in sorted(day_groups.items())
        },
        "by_expiry_days": grouped["expiry_days"],
        "by_log_moneyness": grouped["log_moneyness"],
        "by_mid_premium_usd": grouped["mid_premium_usd"],
        "by_relative_spread": grouped["relative_spread"],
        "by_relative_total_variance_band_width": grouped[
            "relative_total_variance_band_width"
        ],
        "by_option_type": grouped["option_type"],
        "fit_excess_contribution_percent": contributions,
        "distributions": {
            "bid_usd": {
                "minimum": min(row["bid"] for row in rows),
                "p01": percentile([row["bid"] for row in rows], 0.01),
                "p05": percentile([row["bid"] for row in rows], 0.05),
                "median": percentile([row["bid"] for row in rows], 0.50),
            },
            "mid_premium_usd": {
                str(p): percentile([row["premium"] for row in rows], p)
                for p in (0.05, 0.10, 0.25, 0.50, 0.75, 0.90, 0.95)
            },
            "relative_spread": {
                str(p): percentile([row["relative_spread"] for row in rows], p)
                for p in (0.50, 0.75, 0.90, 0.95, 0.99)
            },
            "abs_bs_delta": {
                str(p): percentile([row["abs_delta"] for row in rows], p)
                for p in (0.01, 0.05, 0.10, 0.25, 0.50, 0.75, 0.90, 0.95, 0.99)
            },
            "abs_log_moneyness": {
                str(p): percentile([abs(row["log_moneyness"]) for row in rows], p)
                for p in (0.50, 0.75, 0.90, 0.95, 0.99)
            },
            "relative_spread_training_p95_cutoff": spread_p95,
        },
        "filter_candidates": {
            name: filter_summary(rows, snapshot_groups, keep)
            for name, keep in filter_rules.items()
        },
        "fetch_position": {
            "position_definition": "(raw contract array index - 1) / (contract count - 1)",
            "signed_residual_definition": "fitted total variance - midpoint of bid/ask total-variance bounds",
            "source_order": within_snapshot_regression(
                snapshot_groups, lambda row: row["source_position"]
            ),
            "tick_time_order": within_snapshot_regression(
                snapshot_groups, lambda row: row["tick_position"]
            ),
        },
        "underlying_move": {
            "definition": "log(end underlying midpoint / start underlying midpoint); response is per-snapshot median signed residual",
            "snapshot_count": len(residual_move_x),
            "slope": move_regression["slope"],
            "r_squared": move_regression["r_squared"],
            "move_p10": percentile(signed_moves, 0.10),
            "move_median": percentile(signed_moves, 0.50),
            "move_p90": percentile(signed_moves, 0.90),
            "maximum_absolute_move": max(abs(value) for value in signed_moves),
            "fetch_duration_seconds": {
                "p10": percentile(fetch_durations, 0.10),
                "median": percentile(fetch_durations, 0.50),
                "p90": percentile(fetch_durations, 0.90),
                "maximum": max(fetch_durations),
            },
        },
        "method": {
            "fit": "independent cold essvi_fit CLI call per prepared snapshot",
            "quote_preparation": "replay_snapshots.prepare_snapshot; locked/crossed quotes excluded before convert_ibit_snapshots.quote_row",
            "loss": "existing unweighted squared total-variance band excess; no vega weights or cleaning filters",
            "cap": CAP,
            "ratio": "outside-band total-variance distance / max(bid-ask total-variance width, 1e-6)",
            "relative_total_variance_band_width": "(ask total variance - bid total variance) / midpoint total variance",
            "holdout": "2026-10-15 and 2026-10-16 are not read",
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data-root",
        type=Path,
        default=Path.home() / "vol-rv" / "data",
        help="root containing ibit/chain_YYYYMMDD.jsonl.gz",
    )
    parser.add_argument(
        "--fit-binary",
        type=Path,
        default=ROOT / "build" / "essvi_fit",
        help="current cold-fit CLI binary",
    )
    args = parser.parse_args()
    data_root = args.data_root.expanduser().resolve()
    fit_binary = args.fit_binary.expanduser()
    if not fit_binary.is_absolute():
        fit_binary = (ROOT / fit_binary).resolve()
    print(json.dumps(analyze(data_root, fit_binary), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
