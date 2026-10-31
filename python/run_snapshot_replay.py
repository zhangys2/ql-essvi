#!/usr/bin/env python3
"""Prepare and replay every uninterrupted five-minute IBIT/SPY session chain."""

import argparse
import csv
import json
import math

import subprocess
import tempfile
from collections import defaultdict
from pathlib import Path

from replay_snapshots import prepare_data


def read_manifest(path):
    with Path(path).open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream, delimiter="\t"))
    numeric_fields = (
        "timestamp_ms",
        "missing_intervals",
        "valid_quotes",
        "excluded_locked",
        "excluded_crossed",
        "excluded_other",
    )
    for row in rows:
        for field in numeric_fields:
            row[field] = int(row[field] or 0)
    return sorted(
        rows,
        key=lambda row: (row["asset"], row["session_date"], row["timestamp_ms"]),
    )


def split_replay_segments(rows):
    """Partition rows so every segment begins with a cold fit."""
    segments = []
    current = []
    current_session = None

    def finish_segment():
        nonlocal current
        if current:
            segments.append(current)
            current = []

    for row in rows:
        session = (row["asset"], row["session_date"])
        if session != current_session:
            finish_segment()
            current_session = session
        mode = row["mode"]
        if mode == "skip":
            finish_segment()
        elif mode == "cold":
            finish_segment()
            if row["status"] != "usable" or not row["quote_file"]:
                raise ValueError("cold replay row lacks usable normalized quotes")
            current = [row]
        elif mode == "warm":
            if not current:
                raise ValueError("warm replay row has no preceding cold segment")
            if row["status"] != "usable" or not row["quote_file"]:
                raise ValueError("warm replay row lacks usable normalized quotes")
            current.append(row)
        else:
            raise ValueError(f"unknown replay mode: {mode}")
    finish_segment()
    return segments


def _percentile(values, quantile):
    if not values:
        return None
    ordered = sorted(values)
    index = quantile * (len(ordered) - 1)
    low = int(index)
    high = min(low + 1, len(ordered) - 1)
    fraction = index - low
    return ordered[low] + fraction * (ordered[high] - ordered[low])


def _combine_metrics(summaries, field):
    metric_rows = [summary[field] for summary in summaries if field in summary]
    count = sum(row.get("quote_count", 0) for row in metric_rows)
    if not count:
        return {
            "quote_count": 0,
            "above_cap_count": 0,
            "maximum_excess_ratio": 0.0,
            "rms_excess_ratio": 0.0,
            "summed_band_loss": 0.0,
            "rms_excess_vol_points": 0.0,
            "maximum_excess_vol_points": 0.0,
        }
    return {
        "quote_count": count,
        "above_cap_count": sum(row.get("above_cap_count", 0) for row in metric_rows),
        "maximum_excess_ratio": max(
            row.get("maximum_excess_ratio", 0.0) for row in metric_rows
        ),
        "rms_excess_ratio": math.sqrt(
            sum(row.get("rms_excess_ratio", 0.0) ** 2 * row.get("quote_count", 0)
                for row in metric_rows)
            / count
        ),
        "summed_band_loss": sum(
            row.get("summed_band_loss", 0.0) for row in metric_rows
        ),
        "rms_excess_vol_points": math.sqrt(
            sum(
                row.get("rms_excess_vol_points", 0.0) ** 2
                * row.get("quote_count", 0)
                for row in metric_rows
            )
            / count
        ),
        "maximum_excess_vol_points": max(
            row.get("maximum_excess_vol_points", 0.0) for row in metric_rows
        ),
    }


def _ratio(numerator, denominator):
    if denominator == 0.0:
        return 1.0 if numerator == 0.0 else None
    return numerator / denominator


def _daily_result(rows, summaries):
    warm_quality = _combine_metrics(summaries, "warm_quality")
    cold_quality = _combine_metrics(summaries, "cold_quality")
    paired_updates = sum(summary.get("paired_updates", 0) for summary in summaries)
    samples = sum(summary.get("samples", 0) for summary in summaries)
    path_counts = {
        name: sum(summary.get("path_counts", {}).get(name, 0) for summary in summaries)
        for name in ("cold", "warm", "fallback")
    }
    warm_times = [
        value
        for summary in summaries
        for value in summary.get("warm_seconds_samples", [])
    ]
    cold_times = [
        value
        for summary in summaries
        for value in summary.get("cold_seconds_samples", [])
    ]
    caps = []
    for index, cap in enumerate((0.05, 0.1, 0.2)):
        warm_above = sum(
            summary.get("cap_sensitivity", [{}] * 3)[index].get(
                "warm_above_cap_count", 0
            )
            for summary in summaries
        )
        cold_above = sum(
            summary.get("cap_sensitivity", [{}] * 3)[index].get(
                "cold_above_cap_count", 0
            )
            for summary in summaries
        )
        warm_share = warm_above / warm_quality["quote_count"] if warm_quality["quote_count"] else None
        cold_share = cold_above / cold_quality["quote_count"] if cold_quality["quote_count"] else None
        caps.append(
            {
                "cap": cap,
                "paired_updates": paired_updates,
                "warm_above_cap_count": warm_above,
                "cold_above_cap_count": cold_above,
                "warm_share_above_cap": warm_share,
                "cold_share_above_cap": cold_share,
                "share_delta_pp": (
                    100.0 * (warm_share - cold_share)
                    if warm_share is not None and cold_share is not None
                    else None
                ),
            }
        )

    warm_pairs = samples
    return {
        "asset": rows[0]["asset"],
        "session_date": rows[0]["session_date"],
        "snapshots": len(rows),
        "usable_snapshots": sum(row["status"] == "usable" for row in rows),
        "skipped_snapshots": sum(row["status"] != "usable" for row in rows),
        "warm_pairs": warm_pairs,
        "paired_updates": paired_updates,
        "cold_restarts": sum(row["mode"] == "cold" for row in rows),
        "gap_events": sum(
            row["mode"] == "cold" and row["restart_reason"] != "session_start"
            for row in rows
        ),
        "missing_intervals": sum(row["missing_intervals"] for row in rows),
        "excluded_locked": sum(row["excluded_locked"] for row in rows),
        "excluded_crossed": sum(row["excluded_crossed"] for row in rows),
        "excluded_other": sum(row["excluded_other"] for row in rows),
        "warm_p50_ms": None if not warm_times else 1000.0 * _percentile(warm_times, 0.50),
        "warm_p99_ms": None if not warm_times else 1000.0 * _percentile(warm_times, 0.99),
        "cold_p50_ms": None if not cold_times else 1000.0 * _percentile(cold_times, 0.50),
        "cold_p99_ms": None if not cold_times else 1000.0 * _percentile(cold_times, 0.99),
        "fallback_updates": path_counts["fallback"],
        "fallback_rate_percent": (
            100.0 * path_counts["fallback"] / warm_pairs if warm_pairs else None
        ),
        "warm_failures": sum(summary.get("warm_failures", 0) for summary in summaries),
        "cold_failures": sum(summary.get("cold_failures", 0) for summary in summaries),
        "warm_invalid_surfaces": sum(
            summary.get("warm_invalid_surfaces", 0) for summary in summaries
        ),
        "cold_invalid_surfaces": sum(
            summary.get("cold_invalid_surfaces", 0) for summary in summaries
        ),
        "warm_dense_grid_violations": sum(
            summary.get("warm_dense_grid_violations", 0) for summary in summaries
        ),
        "cold_dense_grid_violations": sum(
            summary.get("cold_dense_grid_violations", 0) for summary in summaries
        ),
        "warm_quality": warm_quality,
        "cold_quality": cold_quality,
        "summed_band_loss_ratio": _ratio(
            warm_quality["summed_band_loss"], cold_quality["summed_band_loss"]
        ) if paired_updates else None,
        "rms_excess_iv_delta": (
            warm_quality["rms_excess_vol_points"]
            - cold_quality["rms_excess_vol_points"]
        ) if paired_updates else None,
        "cap_sensitivity": caps,
    }


def run_replay(bench, manifest_path, output_dir):
    root = Path(__file__).resolve().parents[1]
    benchmark = Path(bench)
    if not benchmark.is_absolute():
        benchmark = root / benchmark
    if not benchmark.is_file():
        raise FileNotFoundError(f"benchmark binary not found: {benchmark}")

    output_dir = Path(output_dir).resolve()
    rows = read_manifest(manifest_path)
    sessions = defaultdict(list)
    for row in rows:
        sessions[(row["asset"], row["session_date"])].append(row)
    segments = split_replay_segments(rows)
    segment_results = defaultdict(list)

    with tempfile.TemporaryDirectory(
        prefix=".snapshot-replay-", dir=output_dir
    ) as temp_root:
        temp_root = Path(temp_root)
        for index, segment in enumerate(segments):
            segment_dir = temp_root / f"segment-{index:04d}"
            segment_dir.mkdir()
            for row in segment:
                source = output_dir / row["quote_file"]
                if not source.is_file():
                    raise FileNotFoundError(f"prepared quote file missing: {source}")
                (segment_dir / row["quote_file"]).symlink_to(source.resolve())
            timing_path = temp_root / f"segment-{index:04d}.json"
            result = subprocess.run(
                [
                    str(benchmark),
                    "--replay-dir",
                    str(segment_dir),
                    "--timings-out",
                    str(timing_path),
                ],
                cwd=root,
                check=False,
                capture_output=True,
                text=True,
            )
            if result.returncode:
                raise RuntimeError(
                    f"replay failed for {segment[0]['asset']} "
                    f"{segment[0]['session_date']} segment {index}: "
                    f"exit {result.returncode}; {result.stderr.strip()}"
                )
            payload = json.loads(timing_path.read_text(encoding="utf-8"))
            summary = payload["runs"][0]
            session = (segment[0]["asset"], segment[0]["session_date"])
            segment_results[session].append(summary)

    daily = [
        _daily_result(session_rows, segment_results.get(key, []))
        for key, session_rows in sorted(sessions.items())
    ]
    result_path = output_dir / "daily_replay_results.json"
    with result_path.open("w", encoding="utf-8") as stream:
        json.dump(daily, stream, indent=2, sort_keys=True, allow_nan=False)
        stream.write("\n")

    csv_path = output_dir / "daily_replay_quality.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        fields = [
            "asset",
            "session_date",
            "cap",
            "snapshots",
            "usable_snapshots",
            "skipped_snapshots",
            "warm_pairs",
            "cold_restarts",
            "gap_events",
            "missing_intervals",
            "excluded_locked",
            "excluded_crossed",
            "excluded_other",
            "warm_p50_ms",
            "warm_p99_ms",
            "fallback_rate_percent",
            "warm_share_above_cap",
            "cold_share_above_cap",
            "share_delta_pp",
            "warm_maximum_excess_ratio",
            "warm_rms_excess_ratio",
            "summed_band_loss_ratio",
            "rms_excess_iv_delta",
        ]
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for result in daily:
            for sensitivity in result["cap_sensitivity"]:
                writer.writerow(
                    {
                        **{key: result[key] for key in fields if key in result},
                        "cap": sensitivity["cap"],
                        "warm_share_above_cap": sensitivity["warm_share_above_cap"],
                        "cold_share_above_cap": sensitivity["cold_share_above_cap"],
                        "share_delta_pp": sensitivity["share_delta_pp"],
                        "warm_maximum_excess_ratio": result["warm_quality"][
                            "maximum_excess_ratio"
                        ],
                        "warm_rms_excess_ratio": result["warm_quality"][
                            "rms_excess_ratio"
                        ],
                    }
                )

    print(
        "asset session snapshots usable skipped warm_pairs cold_restarts "
        "gap_events missing_intervals excluded_locked excluded_crossed excluded_other "
        "warm_p50_ms warm_p99_ms fallback_percent"
    )
    for result in daily:
        print(
            f"{result['asset']} {result['session_date']} {result['snapshots']} "
            f"{result['usable_snapshots']} {result['skipped_snapshots']} "
            f"{result['warm_pairs']} {result['cold_restarts']} {result['gap_events']} "
            f"{result['missing_intervals']} {result['excluded_locked']} "
            f"{result['excluded_crossed']} {result['excluded_other']} "
            f"{result['warm_p50_ms']} {result['warm_p99_ms']} "
            f"{result['fallback_rate_percent']}"
        )
        for sensitivity in result["cap_sensitivity"]:
            print(
                f"  cap={sensitivity['cap']} paired_updates={sensitivity['paired_updates']} "
                f"warm_share={sensitivity['warm_share_above_cap']} "
                f"cold_share={sensitivity['cold_share_above_cap']} "
                f"delta_pp={sensitivity['share_delta_pp']} "
                f"max_ratio={result['warm_quality']['maximum_excess_ratio']} "
                f"rms_ratio={result['warm_quality']['rms_excess_ratio']} "
                f"band_loss_ratio={result['summed_band_loss_ratio']} "
                f"rms_excess_iv_delta={result['rms_excess_iv_delta']}"
            )
    print(f"replay_results={result_path}")
    print(f"replay_quality={csv_path}")
    return daily


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--data-root", type=Path, default=Path.home() / "vol-rv" / "data"
    )
    parser.add_argument(
        "--output-dir", type=Path, default=Path("build/snapshot-replay")
    )
    parser.add_argument("--bench", type=Path, default=Path("build/essvi_bench"))
    args = parser.parse_args()
    manifest, measurements, _, _ = prepare_data(args.data_root, args.output_dir)
    run_replay(args.bench, manifest, args.output_dir)
    print(f"move_measurements={measurements}")


if __name__ == "__main__":
    main()
