import csv
import gzip
import json
import math
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest
import scripts.analyze_fit_quality as analysis
from convert_ibit_snapshots import black_scholes_price
from scripts.analyze_fit_quality import (
    contribution_percentages,
    grouped_band_width_quality,
    relative_total_variance_band_width,
    source_contract_index,
)


def _fit_adapter_snapshot():
    expiry = "20261019"
    time_years = 7.0 / 365.0

    def contract(con_id, right, strike, tick_time, bid_factor=0.95, ask_factor=1.05):
        mid = black_scholes_price(
            100.0, strike, time_years, 0.8, is_call=right == "C"
        )
        return {
            "conId": con_id,
            "expiry": expiry,
            "strike": strike,
            "right": right,
            "bid": mid * bid_factor,
            "ask": mid * ask_factor,
            "tick_time": tick_time,
        }

    call = contract(101, "C", 105.0, 10.0)
    locked = contract("locked", "C", 110.0, 20.0, 1.0, 1.0)
    put = contract(202, "P", 95.0, 30.0)
    return {
        "iso": "2026-10-12T15:00:00Z",
        "spot_ref": 100.0,
        "contracts": [None, call, locked, put],
    }


def _stub_fit_cli(monkeypatch, *, converged, fitted_variances, omitted_ids=()):
    def fake_run(command, *, check, capture_output, text):
        assert check and capture_output and text
        normalized = Path(command[1])
        parameters = Path(command[2])
        fitted = Path(command[3])
        grid = Path(command[4])
        diagnostics = Path(command[5])
        with normalized.open(newline="", encoding="utf-8") as stream:
            input_rows = list(csv.DictReader(stream))
        included_rows = [row for row in input_rows if row["included"] == "true"]
        assert [row["quote_id"] for row in included_rows] == ["101", "202"]

        with diagnostics.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(
                stream,
                fieldnames=[
                    "model",
                    "converged",
                    "objective",
                    "iterations",
                    "runtime_seconds",
                    "error",
                ],
            )
            writer.writeheader()
            writer.writerow(
                {
                    "model": "eSSVI",
                    "converged": str(converged).lower(),
                    "objective": 0.0,
                    "iterations": 1,
                    "runtime_seconds": 0.01,
                    "error": "" if converged else "fixture failure",
                }
            )
        if converged:
            with fitted.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(
                    stream,
                    fieldnames=[
                        "quote_id",
                        "time_to_expiry",
                        "strike",
                        "log_moneyness",
                        "fitted_total_variance",
                        "fitted_iv",
                        "status",
                    ],
                )
                writer.writeheader()
                for row in reversed(included_rows):
                    quote_id = row["quote_id"]
                    if quote_id in omitted_ids:
                        continue
                    variance = fitted_variances[quote_id]
                    writer.writerow(
                        {
                            "quote_id": quote_id,
                            "time_to_expiry": row["time_to_expiry"],
                            "strike": row["strike"],
                            "log_moneyness": "0",
                            "fitted_total_variance": format(variance, ".17g"),
                            "fitted_iv": format(
                                math.sqrt(variance / float(row["time_to_expiry"])),
                                ".17g",
                            ),
                            "status": "ok",
                        }
                    )
            parameters.write_text("parameters\n", encoding="utf-8")
            grid.write_text("grid\n", encoding="utf-8")

    monkeypatch.setattr(analysis.subprocess, "run", fake_run)


def test_source_contract_index_skips_non_objects_and_preserves_raw_positions():
    contracts = [None, {"conId": 101}, {}, "not-a-contract"]

    indexed = source_contract_index(contracts)

    assert indexed == {
        "101": (2, contracts[1]),
        "3": (3, contracts[2]),
    }


def test_relative_total_variance_band_width_normalizes_by_band_midpoint():
    assert math.isclose(relative_total_variance_band_width(0.09, 0.11), 0.2)
    assert math.isclose(relative_total_variance_band_width(0.06, 0.10), 0.5)


def test_band_width_quality_groups_and_aggregates_quote_misses():
    rows = [
        {
            "relative_total_variance_band_width": 0.2,
            "excess_ratio": 0.05,
            "excess": 1.0,
        },
        {
            "relative_total_variance_band_width": 0.2,
            "excess_ratio": 0.2,
            "excess": 2.0,
        },
        {
            "relative_total_variance_band_width": 0.5,
            "excess_ratio": 0.1,
            "excess": 3.0,
        },
    ]

    groups = grouped_band_width_quality(rows)

    assert groups["<0.25"]["quote_count"] == 2
    assert groups["<0.25"]["share_above_cap"] == 0.5
    assert math.isclose(groups["<0.25"]["rms_excess_ratio"], 0.02125**0.5)
    assert groups["0.5-1"]["quote_count"] == 1
    assert groups["0.5-1"]["share_above_cap"] == 0.0


def test_contribution_percentages_are_not_applicable_when_total_is_zero():
    groups = {
        "expiry": {
            "near": {"sum_squared_total_variance_excess": 0.0},
            "far": {"sum_squared_total_variance_excess": 0.0},
        }
    }

    assert contribution_percentages(groups, total_excess=0.0) == {
        "expiry": {"near": None, "far": None}
    }


def test_contribution_percentages_preserve_positive_total_shares():
    groups = {
        "expiry": {
            "near": {"sum_squared_total_variance_excess": 1.0},
            "far": {"sum_squared_total_variance_excess": 3.0},
        }
    }

    assert contribution_percentages(groups, total_excess=4.0) == {
        "expiry": {"near": 25.0, "far": 75.0}
    }


def test_fit_snapshot_maps_reordered_fit_rows_to_source_contracts_and_metrics(
    tmp_path, monkeypatch
):
    snapshot = _fit_adapter_snapshot()
    source_file = tmp_path / "chain_20261012.jsonl.gz"
    fitted_variances = {"101": 0.05, "202": 0.001}
    _stub_fit_cli(
        monkeypatch, converged=True, fitted_variances=fitted_variances
    )

    prepared, observations = analysis.fit_snapshot(
        snapshot, source_file, tmp_path / "essvi_fit", sequence=1
    )

    assert prepared.valid_quotes == 2
    assert prepared.excluded_locked == 1
    assert prepared.excluded_other == 1
    assert [row["quote_id"] for row in observations] == ["101", "202"]
    assert [row["source_position"] for row in observations] == pytest.approx(
        [1.0 / 3.0, 1.0]
    )
    assert [row["tick_position"] for row in observations] == [0.0, 1.0]
    assert [row["expiry_days"] for row in observations] == [7, 7]

    for observation, input_row in zip(observations, prepared.csv_rows):
        bid_variance = float(input_row[8]) ** 2 * float(input_row[3])
        ask_variance = float(input_row[9]) ** 2 * float(input_row[3])
        fitted_variance = fitted_variances[input_row[0]]
        excess = max(
            bid_variance - fitted_variance,
            0.0,
            fitted_variance - ask_variance,
        )
        assert math.isclose(observation["excess"], excess)
        assert math.isclose(
            observation["excess_ratio"],
            excess / max(ask_variance - bid_variance, 1e-6),
        )
        assert math.isclose(
            observation["relative_total_variance_band_width"],
            (ask_variance - bid_variance)
            / (0.5 * (ask_variance + bid_variance)),
        )


def test_fit_snapshot_rejects_nonconverged_cli_result(tmp_path, monkeypatch):
    snapshot = _fit_adapter_snapshot()
    _stub_fit_cli(monkeypatch, converged=False, fitted_variances={})

    with pytest.raises(RuntimeError, match="cold fit did not converge"):
        analysis.fit_snapshot(
            snapshot,
            tmp_path / "chain_20261012.jsonl.gz",
            tmp_path / "essvi_fit",
            sequence=1,
        )


def test_fit_snapshot_rejects_missing_fitted_quote_id(tmp_path, monkeypatch):
    snapshot = _fit_adapter_snapshot()
    _stub_fit_cli(
        monkeypatch,
        converged=True,
        fitted_variances={"101": 0.05, "202": 0.001},
        omitted_ids={"202"},
    )

    with pytest.raises(ValueError, match="fitted quote missing for id 202"):
        analysis.fit_snapshot(
            snapshot,
            tmp_path / "chain_20261012.jsonl.gz",
            tmp_path / "essvi_fit",
            sequence=1,
        )


def test_analyzer_emits_zero_contributions_as_json_null_and_uses_training_files(
    tmp_path, monkeypatch, capsys
):
    training_dates = ("20261012", "20261013", "20261014")
    source_dir = tmp_path / "ibit"
    source_dir.mkdir()
    for date in training_dates + ("20261015", "20261016"):
        snapshot = {
            "session_date": f"{date[:4]}-{date[4:6]}-{date[6:]}",
            "contracts": [{}, {}],
            "underlying_start": {"bid": 99.99, "ask": 100.01},
            "underlying_end": {"bid": 100.09, "ask": 100.11},
            "fetch_start_ms": 1_000,
            "fetch_end_ms": 2_000,
        }
        source_file = source_dir / f"chain_{date}.jsonl.gz"
        with gzip.open(source_file, "wt", encoding="utf-8") as stream:
            stream.write(json.dumps(snapshot) + "\n")

    fit_binary = tmp_path / "essvi_fit"
    fit_binary.touch()
    training_files = {f"chain_{date}.jsonl.gz" for date in training_dates}

    def fake_fit_snapshot(snapshot, source_file, fit_binary, sequence):
        assert source_file.name in training_files
        session_date = snapshot["session_date"]
        snapshot_id = f"{source_file.name}:{sequence}"
        observations = [
            {
                "snapshot_id": snapshot_id,
                "session_date": session_date,
                "source_position": 0.0,
                "tick_position": 0.0,
                "expiry_days": 5,
                "log_moneyness": -0.1,
                "premium": 0.1,
                "relative_spread": 0.2,
                "relative_total_variance_band_width": 0.2,
                "abs_delta": 0.1,
                "bid": 0.05,
                "option_type": "C",
                "signed_residual": -0.01,
                "excess": 0.0,
                "excess_ratio": 0.0,
            },
            {
                "snapshot_id": snapshot_id,
                "session_date": session_date,
                "source_position": 1.0,
                "tick_position": 1.0,
                "expiry_days": 40,
                "log_moneyness": 0.1,
                "premium": 0.5,
                "relative_spread": 0.4,
                "relative_total_variance_band_width": 0.5,
                "abs_delta": 0.3,
                "bid": 0.3,
                "option_type": "P",
                "signed_residual": 0.01,
                "excess": 0.0,
                "excess_ratio": 0.0,
            },
        ]
        prepared = SimpleNamespace(
            valid_quotes=2,
            excluded_locked=0,
            excluded_crossed=0,
            excluded_other=0,
            csv_rows=[[], []],
            session_date=session_date,
        )
        return prepared, observations

    monkeypatch.setattr(analysis, "fit_snapshot", fake_fit_snapshot)
    monkeypatch.setattr(
        sys,
        "argv",
        [
            "analyze_fit_quality.py",
            "--data-root",
            str(tmp_path),
            "--fit-binary",
            str(fit_binary),
        ],
    )

    assert analysis.main() == 0
    report = json.loads(capsys.readouterr().out)

    assert report["coverage"]["source_files"] == [
        f"chain_{date}.jsonl.gz" for date in training_dates
    ]
    assert report["coverage"]["snapshot_count"] == 3
    assert report["coverage"]["valid_quotes"] == 6
    assert report["fit_quality"]["sum_squared_total_variance_excess"] == 0.0
    assert report["fit_excess_contribution_percent"] == {
        dimension: {bucket: None for bucket in groups}
        for dimension, groups in {
            "expiry_days": {"0-7d": {}, "31-90d": {}},
            "log_moneyness": {"(-0.20,-0.10]": {}, "(0.05,0.10]": {}},
            "mid_premium_usd": {"$0.05-$0.25": {}, "$0.25-$1": {}},
            "relative_spread": {"<0.25": {}, "0.25-0.5": {}},
            "relative_total_variance_band_width": {"<0.25": {}, "0.5-1": {}},
            "option_type": {"C": {}, "P": {}},
        }.items()
    }
