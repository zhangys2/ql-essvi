#!/usr/bin/env python3
"""Compare unit-weight and vega-weighted cold fits on cleaned IBIT training data."""

from __future__ import annotations

import argparse
import gzip
import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "python"))

from convert_ibit_snapshots import CLEANING_RULES
from scripts.analyze_fit_quality import TRAINING_DATES, fit_snapshot, quality

ARMS = {"unweighted": "unit", "vega_weighted": "vega"}


def _empty_arm():
    return {
        "snapshot_count": 0,
        "cold_fits_succeeded": 0,
        "input_contract_count": 0,
        "valid_quotes": 0,
        "excluded_locked": 0,
        "excluded_crossed": 0,
        "excluded_other": 0,
        "cleaning_exclusions_by_rule": {rule: 0 for rule in CLEANING_RULES},
        "cleaning_exclusions_combined": 0,
        "summed_fit_objective": 0.0,
        "per_snapshot_exclusions": [],
        "observations": [],
    }


def evaluate(data_root: Path, fit_binary: Path) -> dict[str, Any]:
    data_root = Path(data_root).expanduser().resolve()
    fit_binary = Path(fit_binary).expanduser()
    if not fit_binary.is_absolute():
        fit_binary = (ROOT / fit_binary).resolve()
    files = [data_root / "ibit" / f"chain_{date}.jsonl.gz" for date in TRAINING_DATES]
    missing = [str(path) for path in files if not path.is_file()]
    if missing:
        raise FileNotFoundError("missing training snapshot files: " + ", ".join(missing))
    if not fit_binary.is_file():
        raise FileNotFoundError(f"cold-fit binary not found: {fit_binary}")

    arms = {name: _empty_arm() for name in ARMS}
    for source_file in files:
        with gzip.open(source_file, "rt", encoding="utf-8") as stream:
            for line_number, line in enumerate(stream, start=1):
                if not line.strip():
                    continue
                snapshot = json.loads(line)
                contracts = snapshot.get("contracts", [])
                for arm_name, weight_mode in ARMS.items():
                    arm = arms[arm_name]
                    sequence = arm["snapshot_count"] + 1
                    arm["snapshot_count"] += 1
                    arm["input_contract_count"] += len(contracts)
                    prepared, observations = fit_snapshot(
                        snapshot,
                        source_file,
                        fit_binary,
                        sequence,
                        cleaning_rules=CLEANING_RULES,
                        weight_mode=weight_mode,
                    )
                    if (
                        prepared.valid_quotes
                        + prepared.excluded_locked
                        + prepared.excluded_crossed
                        + prepared.excluded_other
                        != len(contracts)
                    ):
                        raise ValueError(
                            f"{source_file}:{line_number}: quote/exclusion counts "
                            "do not cover the input contracts"
                        )
                    arm["valid_quotes"] += prepared.valid_quotes
                    arm["excluded_locked"] += prepared.excluded_locked
                    arm["excluded_crossed"] += prepared.excluded_crossed
                    arm["excluded_other"] += prepared.excluded_other
                    for rule in CLEANING_RULES:
                        arm["cleaning_exclusions_by_rule"][rule] += (
                            prepared.cleaning_exclusions[rule]
                        )
                    arm["cleaning_exclusions_combined"] += (
                        prepared.excluded_cleaning_combined
                    )
                    arm["per_snapshot_exclusions"].append(
                        {
                            "source": f"{source_file.name}:{line_number}",
                            "input_contracts": len(contracts),
                            "valid_quotes": prepared.valid_quotes,
                            "excluded_locked": prepared.excluded_locked,
                            "excluded_crossed": prepared.excluded_crossed,
                            "excluded_other": prepared.excluded_other,
                            "excluded_cleaning_by_rule": {
                                rule: prepared.cleaning_exclusions[rule]
                                for rule in CLEANING_RULES
                            },
                            "excluded_cleaning_combined": (
                                prepared.excluded_cleaning_combined
                            ),
                        }
                    )
                    if observations:
                        arm["cold_fits_succeeded"] += 1
                        arm["summed_fit_objective"] += observations[0]["fit_objective"]
                        arm["observations"].extend(observations)

    report_arms = {}
    for name, weight_mode in ARMS.items():
        arm = arms[name]
        if not arm["observations"]:
            raise ValueError(f"{name} arm produced no fitted quotes")
        report_arms[name] = {
            "weight_mode": weight_mode,
            "coverage": {
                key: value
                for key, value in arm.items()
                if key not in {"observations", "summed_fit_objective", "per_snapshot_exclusions"}
            },
            "summed_fit_objective": arm["summed_fit_objective"],
            "fit_quality": quality(arm["observations"]),
            "per_snapshot_exclusions": arm["per_snapshot_exclusions"],
        }

    return {
        "asset": "IBIT",
        "training_dates": list(TRAINING_DATES),
        "source_files": [path.name for path in files],
        "method": {
            "fit": "two independent cold essvi_fit runs per usable snapshot",
            "cleaning_profile": "combined",
            "cleaning_rules": list(CLEANING_RULES),
            "weight_normalization": "raw Black-Scholes dC/dw divided by included-quote snapshot median",
            "unweighted_cap": 0.1,
            "price_error": "raw dC/dw times absolute total-variance excess; ticks use $0.01/share and contracts use 100 shares",
            "vega_weighted_metric": "weight times total-variance excess, using the shared snapshot-normalized weight in both arms",
            "holdout": "2026-10-15 and 2026-10-16 are not read",
        },
        "arms": report_arms,
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
        help="cold-fit CLI binary",
    )
    args = parser.parse_args()
    print(json.dumps(evaluate(args.data_root, args.fit_binary), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
