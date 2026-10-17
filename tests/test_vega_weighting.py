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
    snapshot = _snapshot(
        [
            _contract("near", 101.0),
            _contract("mid", 115.0),
            _contract("wing", 150.0),
            _contract("excluded", 100.0),
            _contract("same-day", 101.0, expiry="20261012", time_years=1 / 365),
            _contract("past", 101.0, expiry="20261011", time_years=1 / 365),
            _contract("session-next", 101.0, expiry="20261013", time_years=1 / 365),
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
    assert row_count == 7
    assert included_count == replay.valid_quotes == 4
    with output_path.open(newline="", encoding="utf-8") as stream:
        converted_rows = list(csv.DictReader(stream))
    assert converter.HEADER[-1] == "weight"
    converted_by_id = {row["quote_id"]: row for row in converted_rows}
    replay_by_id = {row[0]: row for row in replay.csv_rows}
    assert converted_by_id.keys() == replay_by_id.keys()
    for quote_id, converted in converted_by_id.items():
        replay_row = replay_by_id[quote_id]
        assert converted["effective_date"] == replay_row[1]
        assert converted["time_to_expiry"] == replay_row[3]
        assert converted["included"] == replay_row[11]
        assert converted["weight"] == replay_row[13]
    assert all(row["effective_date"] == "2026-10-12" for row in converted_rows)
    for quote_id in ("excluded", "same-day", "past"):
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


def test_vega_ab_runs_unit_and_weighted_cold_arms_on_training_only(tmp_path):
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
    weights_log = tmp_path / "fit-weights.jsonl"
    fit_binary.write_text(
        f"""#!/usr/bin/env python3
import csv
import json
import math
import sys
from pathlib import Path

normalized, parameters, fitted, grid, diagnostics = map(Path, sys.argv[1:])
with normalized.open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
included = [row for row in rows if row["included"] == "true"]
with open({str(weights_log)!r}, "a", encoding="utf-8") as stream:
    json.dump([float(row["weight"]) for row in included], stream)
    stream.write(chr(10))
with diagnostics.open("w", newline="", encoding="utf-8") as stream:
    writer = csv.writer(stream)
    writer.writerow(["model", "converged", "objective", "iterations", "runtime_seconds", "error"])
    writer.writerow(["eSSVI", "true", "0.25", "1", "0.01", ""])
with fitted.open("w", newline="", encoding="utf-8") as stream:
    writer = csv.writer(stream)
    writer.writerow(["quote_id", "time_to_expiry", "strike", "log_moneyness", "fitted_total_variance", "fitted_iv", "status"])
    for row in included:
        time_years = float(row["time_to_expiry"])
        strike = float(row["strike"])
        forward = float(row["forward"])
        ask_variance = time_years * float(row["ask_iv"]) ** 2
        variance = ask_variance + 0.001
        writer.writerow([
            row["quote_id"],
            row["time_to_expiry"],
            row["strike"],
            math.log(strike / forward),
            format(variance, ".17g"),
            format(math.sqrt(variance / time_years), ".17g"),
            "ok",
        ])
parameters.write_text("parameters" + chr(10), encoding="utf-8")
grid.write_text("grid" + chr(10), encoding="utf-8")
""",
        encoding="utf-8",
    )
    fit_binary.chmod(0o755)

    report = evaluator.evaluate(data_root, fit_binary)
    seen_weights = [
        json.loads(line) for line in weights_log.read_text(encoding="utf-8").splitlines()
    ]

    assert report["training_dates"] == ["20261012", "20261013", "20261014"]
    assert report["method"]["cleaning_profile"] == "combined"
    assert report["arms"]["unweighted"]["coverage"]["cold_fits_succeeded"] == 3
    assert report["arms"]["vega_weighted"]["coverage"]["cold_fits_succeeded"] == 3
    assert report["arms"]["unweighted"]["fit_quality"]["quote_count"] == 9
    assert report["arms"]["vega_weighted"]["fit_quality"]["quote_count"] == 9
    assert len(seen_weights) == 6
    assert all(weights == [1.0, 1.0, 1.0] for weights in seen_weights[::2])
    assert any(any(weight != 1.0 for weight in weights) for weights in seen_weights[1::2])
