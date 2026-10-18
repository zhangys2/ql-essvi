import csv
import datetime as dt
import gzip
import importlib
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "python"))

import convert_ibit_snapshots as converter
from convert_ibit_snapshots import black_scholes_price
from replay_snapshots import prepare_snapshot


def _contract(identifier, strike, *, expiry="20281011", iv=0.8, time_years=2.0):
    mid = black_scholes_price(100.0, strike, time_years, iv, is_call=True)
    return {
        "conId": identifier,
        "expiry": expiry,
        "strike": strike,
        "right": "C",
        "bid": mid * 0.98,
        "ask": mid * 1.02,
    }


def _snapshot(contracts, *, timestamp=None):
    if timestamp is None:
        timestamp = dt.datetime(2026, 10, 12, 16, 0, tzinfo=dt.timezone.utc)
    return {
        "ts": int(timestamp.timestamp() * 1000),
        "spot_ref": 100.0,
        "contracts": contracts,
    }


def test_raw_sensitivity_is_black_scholes_price_per_total_variance():
    forward, strike, time_years, sigma, discount = 100.0, 105.0, 0.5, 0.8, 0.97
    d1 = (math.log(forward / strike) + 0.5 * sigma**2 * time_years) / (
        sigma * math.sqrt(time_years)
    )
    phi = math.exp(-0.5 * d1**2) / math.sqrt(2.0 * math.pi)
    expected = discount * forward * phi / (2.0 * sigma * math.sqrt(time_years))

    actual = converter.vega_weights.raw_price_sensitivity(
        sigma, forward, strike, time_years, discount
    )

    assert math.isclose(actual, expected, rel_tol=1e-14)


def test_snapshot_weights_are_positive_and_median_one():
    prepared = prepare_snapshot(
        _snapshot(
            [
                _contract("near", 101.0),
                _contract("mid", 115.0),
                _contract("wing", 150.0),
            ]
        ),
        "IBIT",
    )

    included = [row for row in prepared.csv_rows if row[11] == "true"]
    weights = [float(row[13]) for row in included]
    assert len(weights) == 3
    assert all(math.isfinite(weight) and weight > 0.0 for weight in weights)
    assert math.isclose(converter.vega_weights.median(weights), 1.0, rel_tol=1e-14)
    assert len({round(weight, 8) for weight in weights}) > 1


def test_converter_and_replay_emit_identical_vega_weights(tmp_path):
    locked = _contract("locked", 103.0)
    locked["ask"] = locked["bid"]
    snapshot = _snapshot(
        [
            _contract("near", 101.0),
            _contract("mid", 115.0),
            _contract("wing", 150.0),
            _contract("excluded", 100.0),
            _contract("same-day", 101.0, expiry="20261012", time_years=1 / 365),
            _contract("past", 101.0, expiry="20261011", time_years=1 / 365),
            _contract("session-next", 101.0, expiry="20261013", time_years=1 / 365),
            locked,
        ],
        timestamp=dt.datetime(2026, 10, 13, 0, 15, tzinfo=dt.timezone.utc),
    )
    replay = prepare_snapshot(snapshot, "IBIT")
    output_dir = tmp_path / "converted"
    output_dir.mkdir()
    output_path, row_count, included_count, _ = converter.convert_snapshot(
        snapshot, output_dir, "chain_20261012.jsonl.gz", 1
    )

    assert output_path is not None
    assert row_count == 8
    assert included_count == replay.valid_quotes == 4
    with output_path.open(newline="", encoding="utf-8") as stream:
        converted_rows = list(csv.DictReader(stream))
    assert converter.HEADER[-1] == "weight"
    converted_by_id = {row["quote_id"]: row for row in converted_rows}
    replay_by_id = {row[0]: row for row in replay.csv_rows}
    assert set(converted_by_id) - {"locked"} == set(replay_by_id)
    converted_included = {
        quote_id
        for quote_id, row in converted_by_id.items()
        if row["included"] == "true"
    }
    replay_included = {
        quote_id for quote_id, row in replay_by_id.items() if row[11] == "true"
    }
    assert converted_included == replay_included
    for quote_id, replay_row in replay_by_id.items():
        converted = converted_by_id[quote_id]
        assert converted["effective_date"] == replay_row[1]
        assert converted["time_to_expiry"] == replay_row[3]
        assert converted["included"] == replay_row[11]
        assert converted["weight"] == replay_row[13]
    assert all(row["effective_date"] == "2026-10-12" for row in converted_rows)
    for quote_id in ("excluded", "same-day", "past", "locked"):
        assert converted_by_id[quote_id]["included"] == "false"
        assert converted_by_id[quote_id]["weight"] == ""
    assert all(
        float(row["weight"]) > 0.0
        for row in converted_rows
        if row["included"] == "true"
    )


def test_normalized_schema_accepts_legacy_and_weighted_files(tmp_path):
    legacy_header = converter.HEADER[:-1]
    row = [
        "q1",
        "2026-10-12",
        "2028-10-11",
        "2.0",
        "100.0",
        "1.0",
        "105.0",
        "C",
        "0.79",
        "0.81",
        "0.80",
        "true",
        "",
    ]
    validator = ROOT / "python" / "validate_schema.py"
    for name, header, fields in (
        ("legacy", legacy_header, row),
        ("weighted", converter.HEADER, row + ["1.25"]),
    ):
        path = tmp_path / f"{name}.csv"
        with path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(header)
            writer.writerow(fields)
        result = subprocess.run(
            [sys.executable, str(validator), "normalized", str(path)],
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, result.stderr

    for invalid_weight in ("", "0", "-1", "nan", "inf", "-inf"):
        path = tmp_path / f"invalid-weight-{invalid_weight or 'blank'}.csv"
        with path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(converter.HEADER)
            writer.writerow(row + [invalid_weight])
        result = subprocess.run(
            [sys.executable, str(validator), "normalized", str(path)],
            capture_output=True,
            text=True,
        )
        assert result.returncode != 0
        assert "weight" in result.stderr

    excluded = row.copy()
    excluded[8:11] = ["", "", ""]
    excluded[11] = "false"
    excluded[12] = "not_out_of_the_money"
    path = tmp_path / "excluded-weighted.csv"
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(converter.HEADER)
        writer.writerow(excluded + ["1.25"])
    result = subprocess.run(
        [sys.executable, str(validator), "normalized", str(path)],
        capture_output=True,
        text=True,
    )
    assert result.returncode != 0
    assert "excluded quote weight must be empty" in result.stderr


def test_vega_ab_runs_real_cold_fits_on_training_only():
    evaluator = importlib.import_module("scripts.evaluate_vega_weighting")
    with tempfile.TemporaryDirectory(prefix="vega-evaluator-") as temp_name:
        data_root = Path(temp_name) / "data"
        source_dir = data_root / "ibit"
        source_dir.mkdir(parents=True)
        strikes = (101.0, 105.0, 110.0, 115.0, 120.0, 130.0, 140.0, 150.0)
        contracts = [
            _contract(f"q{index}", strike, iv=1.0 if index == 0 else 0.8)
            for index, strike in enumerate(strikes)
        ]
        for date in ("20261012", "20261013", "20261014"):
            source_path = source_dir / f"chain_{date}.jsonl.gz"
            with gzip.open(source_path, "wt", encoding="utf-8") as stream:
                stream.write(json.dumps(_snapshot(contracts)) + "\n")

        report = evaluator.evaluate(data_root, ROOT / "build" / "essvi_fit")

    assert report["training_dates"] == ["20261012", "20261013", "20261014"]
    assert report["method"]["cleaning_profile"] == "combined"
    for arm_name in ("unweighted", "vega_weighted"):
        arm = report["arms"][arm_name]
        assert arm["coverage"]["snapshot_count"] == 3
        assert arm["coverage"]["cold_fits_succeeded"] == 3
        assert arm["coverage"]["input_contract_count"] == 24
        assert arm["coverage"]["valid_quotes"] == 24
        assert arm["fit_quality"]["quote_count"] == 24
        assert len(arm["per_snapshot_exclusions"]) == 3
        assert [item["source"] for item in arm["per_snapshot_exclusions"]] == [
            f"chain_{date}.jsonl.gz:1"
            for date in ("20261012", "20261013", "20261014")
        ]
        assert all(
            item["input_contracts"] == item["valid_quotes"] == 8
            for item in arm["per_snapshot_exclusions"]
        )

    unit = report["arms"]["unweighted"]
    weighted = report["arms"]["vega_weighted"]
    assert math.isclose(
        unit["summed_fit_objective"],
        unit["fit_quality"]["sum_squared_total_variance_excess"],
        rel_tol=1e-8,
        abs_tol=1e-12,
    )
    assert math.isclose(
        weighted["summed_fit_objective"],
        weighted["fit_quality"]["sum_squared_vega_weighted_excess"],
        rel_tol=1e-8,
        abs_tol=1e-12,
    )
    assert not math.isclose(
        unit["summed_fit_objective"],
        weighted["summed_fit_objective"],
        rel_tol=1e-6,
        abs_tol=1e-10,
    )
