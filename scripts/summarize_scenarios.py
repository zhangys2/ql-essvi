#!/usr/bin/env python3
"""Summarize the fixed-seed JSON reports emitted by verify_scenarios.sh."""

from __future__ import annotations

import argparse
import json
import statistics
from pathlib import Path
from typing import Any

SCENARIOS = ("quiet", "normal", "fast", "front")
SEEDS = (1, 2, 3, 4, 5)


def metric_range(values: list[float | None]) -> str:
    if not values or any(value is None for value in values):
        return "unavailable"
    available = [float(value) for value in values]
    return (
        f"{statistics.median(available):.6g} "
        f"[{min(available):.6g}, {max(available):.6g}]"
    )


def load_report(path: Path, scenario: str, seed: int) -> dict[str, Any]:
    with path.open(encoding="utf-8") as report_file:
        report = json.load(report_file)
    if report.get("seed") != seed or report.get("updates") != 200:
        raise ValueError(f"{path}: expected seed {seed}, 200 updates")
    runs = report.get("runs", [])
    if len(runs) != 1 or runs[0].get("run") != scenario:
        raise ValueError(f"{path}: expected one {scenario} result")
    if runs[0].get("samples") != 200:
        raise ValueError(f"{path}: expected 200 measured updates")
    return runs[0]


def paired_metrics(run: dict[str, Any]) -> bool:
    warm_count = run["warm_quality"]["quote_count"]
    cold_count = run["cold_quality"]["quote_count"]
    return warm_count > 0 and warm_count == cold_count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    reports: dict[tuple[str, int], dict[str, Any]] = {}
    for scenario in SCENARIOS:
        for seed in SEEDS:
            path = args.directory / f"{scenario}-{seed}.json"
            reports[(scenario, seed)] = load_report(path, scenario, seed)

    print("Five-seed median [minimum, maximum]; timing in milliseconds.")
    print(
        "scenario | warm p50 ms | warm p99 ms | fallback % | "
        "share delta pp | max ratio warm/cold | RMS ratio warm/cold | "
        "band loss warm/cold | RMS IV excess delta pt"
    )
    for scenario in SCENARIOS:
        runs = [reports[(scenario, seed)] for seed in SEEDS]
        p50 = [run["warm_seconds"]["median"] * 1000.0 for run in runs]
        p99 = [run["warm_seconds"]["p99"] * 1000.0 for run in runs]
        fallback = [
            100.0 * run["path_counts"]["fallback"] / run["samples"]
            for run in runs
        ]
        share_delta = [
            100.0
            * (run["warm_quality"]["share_above_cap"]
               - run["cold_quality"]["share_above_cap"])
            if paired_metrics(run)
            else None
            for run in runs
        ]
        max_warm = [
            run["warm_quality"]["maximum_excess_ratio"]
            if paired_metrics(run)
            else None
            for run in runs
        ]
        max_cold = [
            run["cold_quality"]["maximum_excess_ratio"]
            if paired_metrics(run)
            else None
            for run in runs
        ]
        rms_warm = [
            run["warm_quality"]["rms_excess_ratio"]
            if paired_metrics(run)
            else None
            for run in runs
        ]
        rms_cold = [
            run["cold_quality"]["rms_excess_ratio"]
            if paired_metrics(run)
            else None
            for run in runs
        ]
        loss_ratio = [run["summed_band_loss_ratio"] for run in runs]
        iv_delta = [
            run["warm_quality"]["rms_excess_vol_points"]
            - run["cold_quality"]["rms_excess_vol_points"]
            if paired_metrics(run)
            else None
            for run in runs
        ]
        print(
            f"{scenario} | {metric_range(p50)} | {metric_range(p99)} | "
            f"{metric_range(fallback)} | {metric_range(share_delta)} | "
            f"{metric_range(max_warm)} / {metric_range(max_cold)} | "
            f"{metric_range(rms_warm)} / {metric_range(rms_cold)} | "
            f"{metric_range(loss_ratio)} | {metric_range(iv_delta)}"
        )

    print("Old loss-scale gate observations (median [min, max] by scenario):")
    print(
        "scenario | gate checks | checked warm loss | cold reference loss | "
        "band-loss scale | loss tolerance"
    )
    for scenario in SCENARIOS:
        runs = [reports[(scenario, seed)] for seed in SEEDS]
        print(
            f"{scenario} | "
            f"{metric_range([run['old_gate_quality_checks'] for run in runs])} | "
            f"{metric_range([run['old_gate_quality_warm_loss'] for run in runs])} | "
            f"{metric_range([run['old_gate_quality_reference_loss'] for run in runs])} | "
            f"{metric_range([run['old_gate_quality_loss_scale'] for run in runs])} | "
            f"{metric_range([run['old_gate_loss_tolerance'] for run in runs])}"
        )

    print("Load averages by scenario and seed (1/5/15-minute, before -> after):")
    for scenario in SCENARIOS:
        for seed in SEEDS:
            run = reports[(scenario, seed)]
            print(
                f"{scenario} seed={seed}: {run['load_average_before']} -> "
                f"{run['load_average_after']}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
