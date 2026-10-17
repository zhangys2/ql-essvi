#!/usr/bin/env python3
"""Prepare benchmark-only IBIT/SPY snapshot replay inputs and IV move data.

Prices are inverted with European Black-Scholes as a calibration-path stand-in,
not as NBBO or American-option pricing estimates. Same-session expiry dates use
a fixed one-day date-based year fraction so expiry identity stays stable across
intraday replay snapshots.
"""

import argparse
import csv
import datetime as dt
import gzip
import json
import math
import statistics
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from zoneinfo import ZoneInfo

from convert_ibit_snapshots import (
    CLEANING_RULES,
    HEADER,
    add_cleaning_exclusions,
    empty_cleaning_counts,
    expiry_date,
    finite_number,
    quote_row,
    snapshot_time,
    vega_weights,
)


CADENCE_MS = 5 * 60 * 1000
NEW_YORK = ZoneInfo("America/New_York")
METRICS = ("level", "tilt", "noise")
MANIFEST_HEADER = [
    "asset",
    "session_date",
    "timestamp_ms",
    "timestamp",
    "status",
    "mode",
    "restart_reason",
    "missing_intervals",
    "quote_file",
    "valid_quotes",
    "excluded_locked",
    "excluded_crossed",
    "excluded_other",
    *(f"excluded_cleaning_{rule}" for rule in CLEANING_RULES),
    "excluded_cleaning_combined",
    "source_file",
]


@dataclass(frozen=True)
class SnapshotQuote:
    contract_id: str
    expiry_date: str
    time_years: float
    strike: float
    forward: float
    mid_iv: float


@dataclass
class PreparedSnapshot:
    asset: str
    timestamp: dt.datetime
    session_date: str
    quotes: list[SnapshotQuote]
    csv_rows: list[list[str]]
    excluded_locked: int
    excluded_crossed: int
    excluded_other: int
    cleaning_exclusions: dict[str, int]
    source_file: str = ""
    quote_file: str = ""
    mode: str = ""
    restart_reason: str = ""
    missing_intervals: int = 0

    @property
    def timestamp_ms(self):
        return int(self.timestamp.timestamp() * 1000.0)

    @property
    def valid_quotes(self):
        return len(self.quotes)

    @property
    def status(self):
        return "usable" if self.quotes else "unusable"

    @property
    def excluded_cleaning_combined(self):
        return self.cleaning_exclusions["combined"]


@dataclass(frozen=True)
class FitObservation:
    asset: str
    session_date: str
    expiry_date: str
    level: float
    tilt: float
    noise: float
    residuals: tuple[float, ...]
    quote_count: int


def discover_snapshot_files(data_root):
    """Return the supported source files in deterministic asset/date order."""
    root = Path(data_root)
    sources = []
    for asset, directory, pattern in (
        ("IBIT", root / "ibit", "chain_*.jsonl.gz"),
        ("SPY", root / "zdte", "spy_*.jsonl.gz"),
    ):
        sources.extend((asset, path) for path in sorted(directory.glob(pattern)))
    if not sources:
        raise FileNotFoundError(
            f"no supported chain_*.jsonl.gz or spy_*.jsonl.gz snapshots under {root}"
        )
    return sources


def _spot_reference(snapshot):
    spot = finite_number(snapshot.get("spot_ref"))
    if spot is not None and spot > 0.0:
        return spot
    underlying = snapshot.get("underlying_start") or {}
    bid = finite_number(underlying.get("bid"))
    ask = finite_number(underlying.get("ask"))
    last = finite_number(underlying.get("last"))
    if bid is not None and ask is not None and bid > 0.0 and ask > 0.0:
        spot = 0.5 * (bid + ask)
    else:
        spot = last
    return spot if spot is not None and spot > 0.0 else None


def _year_fraction(expiry, session_date):
    days = (expiry - session_date).days
    return max(days, 1) / 365.0


def prepare_snapshot(snapshot, asset, source_file="", *, cleaning_rules=CLEANING_RULES):
    """Convert one snapshot, applying the selected cleaning rules after IV checks."""
    timestamp = snapshot_time(snapshot)
    session_day = timestamp.astimezone(NEW_YORK).date()
    spot = _spot_reference(snapshot)
    contracts = snapshot.get("contracts")
    if not isinstance(contracts, list):
        raise ValueError(f"{source_file}: snapshot contracts is not a list")

    quotes = []
    rows = []
    cleaning_counts = empty_cleaning_counts()
    locked = crossed = other = 0
    for index, contract in enumerate(contracts, start=1):
        if not isinstance(contract, dict):
            other += 1
            continue
        bid = finite_number(contract.get("bid"))
        ask = finite_number(contract.get("ask"))
        if bid is not None and ask is not None and bid == ask:
            locked += 1
            continue
        if bid is not None and ask is not None and bid > ask:
            crossed += 1
            continue
        if spot is None:
            other += 1
            continue

        expiry_text = ""
        time_years = -1.0
        raw_expiry = contract.get("expiry")
        if raw_expiry:
            try:
                expiry = expiry_date(raw_expiry)
                expiry_text = expiry.isoformat()
                time_years = _year_fraction(expiry, session_day)
            except (TypeError, ValueError):
                pass
        row, included = quote_row(
            contract,
            index,
            session_day,
            expiry_text,
            time_years,
            spot,
            cleaning_rules=cleaning_rules,
        )
        rows.append(row)
        add_cleaning_exclusions(cleaning_counts, row[12])
        if not included:
            other += 1
            continue
        quotes.append(
            SnapshotQuote(
                contract_id=row[0],
                expiry_date=row[2],
                time_years=float(row[3]),
                strike=float(row[6]),
                forward=float(row[4]),
                mid_iv=float(row[10]),
            )
        )

    # Stable ordering is required because calibrateWarm compares quote identity
    # positionally within each expiry group.
    rows.sort(key=lambda row: (row[2], row[0]))
    quotes.sort(key=lambda quote: (quote.expiry_date, quote.contract_id))
    return PreparedSnapshot(
        asset=asset,
        timestamp=timestamp,
        session_date=session_day.isoformat(),
        quotes=quotes,
        csv_rows=vega_weights.normalize_snapshot_weights(rows),
        excluded_locked=locked,
        excluded_crossed=crossed,
        excluded_other=other,
        cleaning_exclusions=cleaning_counts,
        source_file=Path(source_file).name if source_file else "",
    )


def matched_iv_changes(previous, current):
    """Return timestamped IV changes for matched expiry/contract identities."""
    if (
        previous.asset != current.asset
        or previous.session_date != current.session_date
        or current.timestamp_ms - previous.timestamp_ms != CADENCE_MS
    ):
        return []
    old_quotes = {
        (quote.expiry_date, quote.contract_id): quote for quote in previous.quotes
    }
    new_quotes = {
        (quote.expiry_date, quote.contract_id): quote for quote in current.quotes
    }
    changes = []
    for key in sorted(old_quotes.keys() & new_quotes.keys()):
        old = old_quotes[key]
        new = new_quotes[key]
        old_k = math.log(old.strike / old.forward)
        new_k = math.log(new.strike / new.forward)
        changes.append(
            {
                "asset": current.asset,
                "session_date": current.session_date,
                "timestamp_ms": current.timestamp_ms,
                "expiry_date": new.expiry_date,
                "contract_id": new.contract_id,
                "x": 0.5 * (old_k + new_k),
                "delta_iv": new.mid_iv - old.mid_iv,
            }
        )
    return changes


def robust_level_tilt(points):
    """Fit a deterministic Theil-Sen level/tilt model and return residuals."""
    if len(points) < 3:
        return None
    slopes = []
    for i, (x0, y0) in enumerate(points):
        for x1, y1 in points[i + 1 :]:
            dx = x1 - x0
            if abs(dx) > 1e-12:
                slopes.append((y1 - y0) / dx)
    if not slopes:
        return None
    slope = statistics.median(slopes)
    level = statistics.median(y - slope * x for x, y in points)
    residuals = tuple(y - level - slope * x for x, y in points)
    residual_center = statistics.median(residuals)
    noise = 1.4826 * statistics.median(
        abs(value - residual_center) for value in residuals
    )
    return level, slope, noise, residuals


def assign_replay_modes(rows):
    """Mark each row cold, warm, or skipped; any gap forces a cold restart."""
    ordered = sorted(
        (dict(row) for row in rows),
        key=lambda row: (row["asset"], row["session_date"], row["timestamp_ms"]),
    )
    previous = {}
    output = []
    for row in ordered:
        key = (row["asset"], row["session_date"])
        prior = previous.get(key)
        row["missing_intervals"] = 0
        delta = None if prior is None else row["timestamp_ms"] - prior["timestamp_ms"]
        if delta is not None and delta > CADENCE_MS:
            row["missing_intervals"] = max(0, int(round(delta / CADENCE_MS)) - 1)
        if row["status"] != "usable":
            row["mode"] = "skip"
            row["restart_reason"] = "unusable_snapshot"
            row["status"] = "unusable"
        elif prior is None:
            row["mode"] = "cold"
            row["restart_reason"] = "session_start"
        elif prior["status"] != "usable":
            row["mode"] = "cold"
            row["restart_reason"] = "unusable_snapshot"
        elif delta == CADENCE_MS:
            row["mode"] = "warm"
            row["restart_reason"] = ""
        else:
            row["mode"] = "cold"
            row["restart_reason"] = "timestamp_gap"
        previous[key] = row
        output.append(row)
    return output


def _robust_observations(snapshots):
    samples = {name: defaultdict(list) for name in METRICS}
    front_back_ratios = defaultdict(list)
    fit_rows = []
    by_session = defaultdict(list)
    for snapshot in snapshots:
        by_session[(snapshot.asset, snapshot.session_date)].append(snapshot)

    five_minute_pairs = defaultdict(int)
    for (asset, session_date), session in by_session.items():
        session.sort(key=lambda snapshot: snapshot.timestamp_ms)
        previous = None
        for current in session:
            if current.status != "usable":
                previous = None
                continue
            if previous is None:
                previous = current
                continue
            changes = matched_iv_changes(previous, current)
            if current.timestamp_ms - previous.timestamp_ms == CADENCE_MS:
                five_minute_pairs[(asset, session_date)] += 1
            by_expiry = defaultdict(list)
            for change in changes:
                by_expiry[change["expiry_date"]].append(
                    (change["x"], change["delta_iv"])
                )
            pair_fits = []
            for expiry, points in sorted(by_expiry.items()):
                fitted = robust_level_tilt(points)
                if fitted is None:
                    continue
                level, slope, noise, residuals = fitted
                # The synthetic tilt multiplies clamp(2*k, -1, 1), so half the
                # fitted slope is the comparable wing-level amplitude.
                tilt = 0.5 * slope
                samples["level"][(asset, session_date)].append(level)
                samples["tilt"][(asset, session_date)].append(tilt)
                samples["noise"][(asset, session_date)].extend(residuals)
                observation = FitObservation(
                    asset,
                    session_date,
                    expiry,
                    level,
                    tilt,
                    noise,
                    residuals,
                    len(points),
                )
                fit_rows.append(observation)
                pair_fits.append(observation)
            if len(pair_fits) >= 2:
                ordered = sorted(pair_fits, key=lambda row: row.expiry_date)
                bucket = max(1, len(ordered) // 4)
                front = statistics.median(abs(row.level) for row in ordered[:bucket])
                back = statistics.median(abs(row.level) for row in ordered[-bucket:])
                if back > 1e-10:
                    front_back_ratios[(asset, session_date)].append(front / back)
            previous = current
    return samples, front_back_ratios, fit_rows, five_minute_pairs


def _weighted_quantile(grouped_values, quantile, equal_assets=False):
    groups = {key: values for key, values in grouped_values.items() if values}
    if not groups:
        return 0.0
    asset_days = defaultdict(set)
    for asset, session_date in groups:
        asset_days[asset].add(session_date)
    assets = sorted(asset_days)
    weighted = []
    for (asset, session_date), values in groups.items():
        if equal_assets:
            group_weight = 1.0 / (len(assets) * len(asset_days[asset]))
        else:
            group_weight = 1.0 / len(asset_days[asset])
        per_value_weight = group_weight / len(values)
        weighted.extend((value, per_value_weight) for value in values)
    weighted.sort(key=lambda item: item[0])
    threshold = quantile * sum(weight for _, weight in weighted)
    cumulative = 0.0
    for value, weight in weighted:
        cumulative += weight
        if cumulative >= threshold:
            return value
    return weighted[-1][0]


def _distribution(grouped_values, equal_assets=False):
    nonempty = {key: values for key, values in grouped_values.items() if values}
    return {
        "observations": sum(len(values) for values in nonempty.values()),
        "asset_days": len(nonempty),
        "p10_signed": _weighted_quantile(nonempty, 0.10, equal_assets),
        "p50_signed": _weighted_quantile(nonempty, 0.50, equal_assets),
        "p95_signed": _weighted_quantile(nonempty, 0.95, equal_assets),
        "p10_abs": _weighted_quantile(
            {key: [abs(value) for value in values] for key, values in nonempty.items()},
            0.10,
            equal_assets,
        ),
        "p50_abs": _weighted_quantile(
            {key: [abs(value) for value in values] for key, values in nonempty.items()},
            0.50,
            equal_assets,
        ),
        "p95_abs": _weighted_quantile(
            {key: [abs(value) for value in values] for key, values in nonempty.items()},
            0.95,
            equal_assets,
        ),
    }


def analyze_snapshots(snapshots):
    samples, ratios, fits, five_minute_pairs = _robust_observations(snapshots)
    by_asset = {}
    for asset in sorted({key[0] for values in samples.values() for key in values}):
        by_asset[asset] = {
            name: _distribution(
                {key: values for key, values in samples[name].items() if key[0] == asset}
            )
            for name in METRICS
        }
    pooled = {
        name: _distribution(samples[name], equal_assets=True) for name in METRICS
    }
    pooled_ratio = _weighted_quantile(ratios, 0.50, equal_assets=True)
    scenario_sizes = {
        "quiet": {
            name: pooled[name]["p10_abs"] for name in METRICS
        },
        "normal": {
            name: pooled[name]["p50_abs"] for name in METRICS
        },
        "fast": {
            name: pooled[name]["p95_abs"] for name in METRICS
        },
        "front": {
            "front_back_ratio": pooled_ratio,
            "front_level": pooled["level"]["p50_abs"] * pooled_ratio,
            "back_level": pooled["level"]["p50_abs"],
            "tilt": pooled["tilt"]["p50_abs"],
            "noise": pooled["noise"]["p50_abs"],
        },
    }
    return {
        "weighting": (
            "within each asset, each session day has equal total weight; pooled "
            "quantiles then give each asset equal total weight; observations "
            "within an asset-day share that day's weight equally"
        ),
        "fit": (
            "per expiry and consecutive five-minute pair, Theil-Sen regression "
            "of matched mid-IV change on mean log-moneyness; level is the median "
            "intercept, tilt is half the slope to match clamp(2*k), and per-quote "
            "noise is the signed residual distribution"
        ),
        "by_asset": by_asset,
        "pooled": pooled,
        "front_back_ratio_p50": pooled_ratio,
        "scenario_sizes": scenario_sizes,
        "fit_rows": [
            {
                "asset": row.asset,
                "session_date": row.session_date,
                "expiry_date": row.expiry_date,
                "level": row.level,
                "tilt": row.tilt,
                "noise_mad_sigma": row.noise,
                "quote_count": row.quote_count,
            }
            for row in fits
        ],
        "five_minute_pairs_by_asset_day": {
            f"{asset}/{session_date}": count
            for (asset, session_date), count in sorted(five_minute_pairs.items())
        },
    }


def _manifest_row(snapshot):
    row = {
        "asset": snapshot.asset,
        "session_date": snapshot.session_date,
        "timestamp_ms": snapshot.timestamp_ms,
        "timestamp": snapshot.timestamp.isoformat(timespec="milliseconds").replace(
            "+00:00", "Z"
        ),
        "status": snapshot.status,
        "quote_file": snapshot.quote_file,
        "valid_quotes": snapshot.valid_quotes,
        "excluded_locked": snapshot.excluded_locked,
        "excluded_crossed": snapshot.excluded_crossed,
        "excluded_other": snapshot.excluded_other,
        "excluded_cleaning_combined": snapshot.excluded_cleaning_combined,
        "source_file": snapshot.source_file,
    }
    row.update(
        {
            f"excluded_cleaning_{rule}": snapshot.cleaning_exclusions[rule]
            for rule in CLEANING_RULES
        }
    )
    return row


def prepare_data(data_root, output_dir, *, cleaning_rules=CLEANING_RULES):
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    snapshots = []
    for asset, path in discover_snapshot_files(data_root):
        with gzip.open(path, "rt", encoding="utf-8") as stream:
            for line_number, line in enumerate(stream, start=1):
                if not line.strip():
                    continue
                try:
                    raw = json.loads(line)
                except json.JSONDecodeError as error:
                    raise ValueError(f"{path}:{line_number}: invalid JSON: {error}") from error
                snapshot = prepare_snapshot(
                    raw, asset, str(path), cleaning_rules=cleaning_rules
                )
                snapshot.source_file = f"{path.name}:{line_number}"
                if snapshot.valid_quotes:
                    snapshot.quote_file = (
                        f"normalized_quotes_{snapshot.timestamp_ms:013d}_"
                        f"{asset}_{line_number:06d}.csv"
                    )
                    with (output_dir / snapshot.quote_file).open(
                        "w", newline="", encoding="utf-8"
                    ) as target:
                        writer = csv.writer(target)
                        writer.writerow(HEADER)
                        writer.writerows(snapshot.csv_rows)
                snapshots.append(snapshot)

    snapshots.sort(
        key=lambda snapshot: (
            snapshot.asset,
            snapshot.session_date,
            snapshot.timestamp_ms,
        )
    )
    manifest_rows = assign_replay_modes(
        [_manifest_row(snapshot) for snapshot in snapshots]
    )
    modes = {
        (row["asset"], row["session_date"], row["timestamp_ms"]): row
        for row in manifest_rows
    }
    for snapshot in snapshots:
        mode = modes[(snapshot.asset, snapshot.session_date, snapshot.timestamp_ms)]
        snapshot.mode = mode["mode"]
        snapshot.restart_reason = mode["restart_reason"]
        snapshot.missing_intervals = mode["missing_intervals"]

    manifest_path = output_dir / "replay_manifest.tsv"
    with manifest_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(
            stream,
            fieldnames=MANIFEST_HEADER,
            delimiter="\t",
            lineterminator="\n",
        )
        writer.writeheader()
        for snapshot in snapshots:
            row = _manifest_row(snapshot)
            row.update(
                mode=snapshot.mode,
                restart_reason=snapshot.restart_reason,
                missing_intervals=snapshot.missing_intervals,
            )
            writer.writerow(row)
            cleaning_text = " ".join(
                f"excluded_cleaning_{rule}={snapshot.cleaning_exclusions[rule]}"
                for rule in CLEANING_RULES
            )
            print(
                f"{snapshot.asset} {snapshot.session_date} {row['timestamp']} "
                f"status={snapshot.status} quotes={snapshot.valid_quotes} "
                f"excluded_locked={snapshot.excluded_locked} "
                f"excluded_crossed={snapshot.excluded_crossed} "
                f"excluded_other={snapshot.excluded_other} "
                f"{cleaning_text} "
                f"excluded_cleaning_combined={snapshot.excluded_cleaning_combined} "
                f"mode={snapshot.mode} reason={snapshot.restart_reason or 'none'} "
                f"source={snapshot.source_file}"
            )

    report = analyze_snapshots(snapshots)
    report["data_root"] = str(Path(data_root))
    report["snapshots"] = len(snapshots)
    report["usable_snapshots"] = sum(snapshot.valid_quotes > 0 for snapshot in snapshots)
    report["unusable_snapshots"] = len(snapshots) - report["usable_snapshots"]
    report["excluded_locked"] = sum(s.excluded_locked for s in snapshots)
    report["excluded_crossed"] = sum(s.excluded_crossed for s in snapshots)
    report["excluded_other"] = sum(s.excluded_other for s in snapshots)
    report["excluded_cleaning_combined"] = sum(
        snapshot.excluded_cleaning_combined for snapshot in snapshots
    )
    report["excluded_cleaning_by_rule"] = {
        rule: sum(snapshot.cleaning_exclusions[rule] for snapshot in snapshots)
        for rule in CLEANING_RULES
    }
    report["cleaning_exclusions_by_snapshot"] = [
        {
            "asset": snapshot.asset,
            "session_date": snapshot.session_date,
            "timestamp_ms": snapshot.timestamp_ms,
            "source_file": snapshot.source_file,
            "by_rule": {
                rule: snapshot.cleaning_exclusions[rule] for rule in CLEANING_RULES
            },
            "combined": snapshot.excluded_cleaning_combined,
        }
        for snapshot in snapshots
    ]
    report_path = output_dir / "move_measurements.json"
    with report_path.open("w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write("\n")
    print(f"manifest={manifest_path}")
    print(f"measurements={report_path}")
    return manifest_path, report_path, snapshots, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data-root", type=Path, default=Path.home() / "vol-rv" / "data"
    )
    parser.add_argument(
        "--output-dir", type=Path, default=Path("build/snapshot-replay")
    )
    args = parser.parse_args()
    prepare_data(args.data_root, args.output_dir)


if __name__ == "__main__":
    main()
