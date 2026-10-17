import gzip
import json
import math
import sys
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT))

import scripts.evaluate_quote_cleaning as evaluation


RULES = ("minimum_bid", "minimum_premium", "relative_spread", "delta_window")
TRAINING_DATES = ("20261012", "20261013", "20261014")


def _write_snapshot(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with gzip.open(path, "wt", encoding="utf-8") as stream:
        snapshot = {"contracts": [{"conId": 1}, {"conId": 2}, {"conId": 3}]}
        stream.write(json.dumps(snapshot) + "\n")


def test_evaluation_refits_individual_and_combined_profiles_on_training_only(
    tmp_path, monkeypatch
):
    data_root = tmp_path / "data"
    for date in TRAINING_DATES + ("20261015", "20261016"):
        _write_snapshot(data_root / "ibit" / f"chain_{date}.jsonl.gz")
    fit_binary = tmp_path / "essvi_fit"
    fit_binary.touch()
    training_files = {f"chain_{date}.jsonl.gz" for date in TRAINING_DATES}
    calls = []

    def fake_fit_snapshot(snapshot, source_file, binary, sequence, cleaning_rules):
        assert source_file.name in training_files
        assert binary == fit_binary
        profile = tuple(cleaning_rules)
        calls.append((source_file.name, sequence, profile))
        counts = {rule: int(rule in profile) for rule in RULES}
        counts["combined"] = int(bool(profile))
        prepared = SimpleNamespace(
            valid_quotes=2,
            cleaning_exclusions=counts,
            excluded_locked=0,
            excluded_crossed=0,
            excluded_other=1,
            excluded_cleaning_combined=counts["combined"],
            csv_rows=[[], [], []],
        )
        ratio = 0.01 * (len(profile) + sequence)
        observations = [
            {"excess_ratio": ratio, "excess": ratio / 100.0},
            {"excess_ratio": ratio, "excess": ratio / 100.0},
        ]
        return prepared, observations

    monkeypatch.setattr(evaluation, "fit_snapshot", fake_fit_snapshot)

    report = evaluation.evaluate(data_root, fit_binary)

    expected_profiles = {
        **{rule: (rule,) for rule in RULES},
        "combined": RULES,
    }
    assert list(report["profiles"]) == list(expected_profiles)
    assert len(calls) == len(expected_profiles) * len(TRAINING_DATES)
    for profile_name, expected_rules in expected_profiles.items():
        profile = report["profiles"][profile_name]
        assert profile["rules"] == list(expected_rules)
        assert profile["coverage"]["snapshot_count"] == 3
        assert profile["coverage"]["cold_fits_succeeded"] == 3
        assert profile["coverage"]["valid_quotes"] == 6
        assert profile["coverage"]["cleaning_exclusions_combined"] == 3
        for rule in RULES:
            expected_count = 3 * int(rule in expected_rules)
            assert (
                profile["coverage"]["cleaning_exclusions_by_rule"][rule]
                == expected_count
            )
        assert len(profile["per_snapshot_exclusions"]) == 3
        assert all(
            row["excluded_cleaning_by_rule"] == {
                rule: int(rule in expected_rules) for rule in RULES
            }
            for row in profile["per_snapshot_exclusions"]
        )
        assert profile["fit_quality"]["quote_count"] == 6
        expected_rms = math.sqrt(
            sum(
                (0.01 * (len(expected_rules) + sequence)) ** 2
                for sequence in (1, 2, 3)
            )
            / 3
        )
        assert math.isclose(profile["fit_quality"]["rms_excess_ratio"], expected_rms)
    assert report["training_dates"] == list(TRAINING_DATES)
    assert all(
        "20261015" not in name and "20261016" not in name
        for name, _, _ in calls
    )
    assert not any("20261015" in name or "20261016" in name for name in report["source_files"])
