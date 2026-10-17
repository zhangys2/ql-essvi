import csv
import datetime as dt
import gzip
import importlib
import json
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "python"))

import convert_ibit_snapshots as converter
from convert_ibit_snapshots import black_scholes_price
from replay_snapshots import prepare_snapshot


def _contract(identifier, strike, *, expiry="20281011", iv=0.8):
    time_years = 2.0
    mid = black_scholes_price(100.0, strike, time_years, iv, is_call=True)
    return {
        "conId": identifier,
        "expiry": expiry,
        "strike": strike,
        "right": "C",
        "bid": mid * 0.98,
        "ask": mid * 1.02,
    }


def _snapshot(contracts):
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
    snapshot = _snapshot(
        [
            _contract("near", 101.0),
            _contract("mid", 115.0),
            _contract("wing", 150.0),
            _contract("excluded", 100.0),
        ]
    )
    replay = prepare_snapshot(snapshot, "IBIT")
    output_dir = tmp_path / "converted"
    output_dir.mkdir()
    output_path, row_count, included_count, _ = converter.convert_snapshot(
        snapshot, output_dir, "chain_20261012.jsonl.gz", 1
    )

    assert output_path is not None
    assert row_count == 4
    assert included_count == replay.valid_quotes == 3
    with output_path.open(newline="", encoding="utf-8") as stream:
        converted_rows = list(csv.DictReader(stream))
    assert converter.HEADER[-1] == "weight"
    converted_by_id = {row["quote_id"]: row for row in converted_rows}
    replay_by_id = {row[0]: row for row in replay.csv_rows}
    assert converted_by_id.keys() == replay_by_id.keys()
    for quote_id, converted in converted_by_id.items():
        replay_row = replay_by_id[quote_id]
        assert converted["included"] == replay_row[11]
        assert converted["weight"] == replay_row[13]
    assert converted_by_id["excluded"]["included"] == "false"
    assert converted_by_id["excluded"]["weight"] == ""
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


def test_vega_ab_runs_unit_and_weighted_cold_arms_on_training_only(
    tmp_path, monkeypatch
):
    evaluator = importlib.import_module("scripts.evaluate_vega_weighting")
    data_root = tmp_path / "data"
    source_dir = data_root / "ibit"
    source_dir.mkdir(parents=True)
    for date in ("20261012", "20261013", "20261014"):
        snapshot = _snapshot(
            [
                _contract("near", 101.0),
                _contract("mid", 115.0),
                _contract("wing", 150.0),
            ]
        )
        source_path = source_dir / f"chain_{date}.jsonl.gz"
        with gzip.open(source_path, "wt", encoding="utf-8") as stream:
            stream.write(json.dumps(snapshot) + "\n")
    fit_binary = tmp_path / "essvi_fit"
    fit_binary.write_text("fixture", encoding="utf-8")
    seen_weights = []

    def fake_fit(command, *, check, capture_output, text):
        assert check and capture_output and text
        normalized, parameters, fitted, grid, diagnostics = map(Path, command[1:])
        with normalized.open(newline="", encoding="utf-8") as stream:
            rows = list(csv.DictReader(stream))
        included = [row for row in rows if row["included"] == "true"]
        weights = [float(row["weight"]) for row in included]
        seen_weights.append(weights)
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
                    "converged": "true",
                    "objective": "0.25",
                    "iterations": 1,
                    "runtime_seconds": "0.01",
                    "error": "",
                }
            )
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
            for row in included:
                bid_variance = float(row["bid_iv"]) ** 2 * float(
                    row["time_to_expiry"]
                )
                ask_variance = float(row["ask_iv"]) ** 2 * float(
                    row["time_to_expiry"]
                )
                variance = ask_variance + 0.001
                writer.writerow(
                    {
                        "quote_id": row["quote_id"],
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
        return subprocess.CompletedProcess(command, 0, "", "")

    fit_analysis = importlib.import_module("scripts.analyze_fit_quality")
    monkeypatch.setattr(fit_analysis.subprocess, "run", fake_fit)
    report = evaluator.evaluate(data_root, fit_binary)

    assert report["training_dates"] == ["20261012", "20261013", "20261014"]
    assert report["method"]["cleaning_profile"] == "combined"
    assert report["arms"]["unweighted"]["coverage"]["cold_fits_succeeded"] == 3
    assert report["arms"]["vega_weighted"]["coverage"]["cold_fits_succeeded"] == 3
    assert report["arms"]["unweighted"]["fit_quality"]["quote_count"] == 9
    assert report["arms"]["vega_weighted"]["fit_quality"]["quote_count"] == 9
    assert len(seen_weights) == 6
    assert all(weights == [1.0, 1.0, 1.0] for weights in seen_weights[::2])
    assert any(any(weight != 1.0 for weight in weights) for weights in seen_weights[1::2])
