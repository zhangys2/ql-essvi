#!/usr/bin/env python3
"""Refit fixed IBIT training snapshots under approved quote-cleaning profiles."""

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

PROFILES = {
    **{rule: (rule,) for rule in CLEANING_RULES},
    "combined": CLEANING_RULES,
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

    profile_results = {}
    for profile_name, cleaning_rules in PROFILES.items():
        observations = []
        per_snapshot_exclusions = []
        cleaning_counts = {rule: 0 for rule in CLEANING_RULES}
        combined_count = 0
        input_contract_count = 0
        valid_quote_count = 0
        excluded_locked = 0
        excluded_crossed = 0
        excluded_other = 0
        snapshot_count = 0
        cold_fits_succeeded = 0
        sequence = 0

        for source_file in files:
            with gzip.open(source_file, "rt", encoding="utf-8") as stream:
                for line_number, line in enumerate(stream, start=1):
                    if not line.strip():
                        continue
                    snapshot = json.loads(line)
                    sequence += 1
                    snapshot_count += 1
                    contracts = snapshot.get("contracts", [])
                    input_contract_count += len(contracts)
                    prepared, fitted_observations = fit_snapshot(
                        snapshot,
                        source_file,
                        fit_binary,
                        sequence,
                        cleaning_rules=cleaning_rules,
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
                    valid_quote_count += prepared.valid_quotes
                    excluded_locked += prepared.excluded_locked
                    excluded_crossed += prepared.excluded_crossed
                    excluded_other += prepared.excluded_other
                    for rule in CLEANING_RULES:
                        cleaning_counts[rule] += prepared.cleaning_exclusions[rule]
                    combined_count += prepared.excluded_cleaning_combined
                    per_snapshot_exclusions.append(
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
                    if fitted_observations:
                        cold_fits_succeeded += 1
                        observations.extend(fitted_observations)

        if not observations:
            raise ValueError(f"profile {profile_name} produced no fitted quotes")
        profile_results[profile_name] = {
            "rules": list(cleaning_rules),
            "coverage": {
                "snapshot_count": snapshot_count,
                "cold_fits_succeeded": cold_fits_succeeded,
                "input_contract_count": input_contract_count,
                "valid_quotes": valid_quote_count,
                "excluded_locked": excluded_locked,
                "excluded_crossed": excluded_crossed,
                "excluded_other": excluded_other,
                "cleaning_exclusions_by_rule": cleaning_counts,
                "cleaning_exclusions_combined": combined_count,
            },
            "fit_quality": quality(observations),
            "per_snapshot_exclusions": per_snapshot_exclusions,
        }

    return {
        "asset": "IBIT",
        "training_dates": list(TRAINING_DATES),
        "source_files": [path.name for path in files],
        "method": {
            "fit": "independent cold essvi_fit CLI call per usable snapshot and profile",
            "quality_metrics": "unweighted excess-ratio cap, RMS, maximum, and squared total-variance excess",
            "holdout": "2026-10-15 and 2026-10-16 are not read",
        },
        "profiles": profile_results,
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
    print(json.dumps(evaluate(args.data_root, args.fit_binary), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
