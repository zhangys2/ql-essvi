import csv
import gzip
import json
import datetime as dt
import math
import statistics
import sys
from pathlib import Path

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "python"))

from convert_ibit_snapshots import (
    black_scholes_price,
    cleaning_reasons,
    convert_directory,
    convert_snapshot,
    quote_row,
)
from replay_snapshots import prepare_data, prepare_snapshot


EFFECTIVE_DATE = dt.date(2026, 10, 12)
FORWARD = 100.0
RULES = ("minimum_bid", "minimum_premium", "relative_spread", "delta_window")


def _contract(
    contract_id,
    *,
    strike=105.0,
    time_years=2.0,
    volatility=0.8,
    premium=None,
    relative_spread=0.1,
):
    expiry = EFFECTIVE_DATE + dt.timedelta(days=round(time_years * 365))
    mid = premium
    if mid is None:
        mid = black_scholes_price(
            FORWARD, strike, time_years, volatility, is_call=True
        )
    return {
        "conId": contract_id,
        "expiry": expiry.strftime("%Y%m%d"),
        "strike": strike,
        "right": "C",
        "bid": mid * (1.0 - relative_spread / 2.0),
        "ask": mid * (1.0 + relative_spread / 2.0),
    }


def _contract_for_delta(contract_id, target_delta, *, time_years, volatility):
    quantile = statistics.NormalDist().inv_cdf(target_delta)
    log_strike_ratio = (
        0.5 * volatility * volatility * time_years
        - quantile * volatility * math.sqrt(time_years)
    )
    strike = FORWARD * math.exp(log_strike_ratio)
    expiry = EFFECTIVE_DATE + dt.timedelta(days=round(time_years * 365))
    price = black_scholes_price(
        FORWARD, strike, time_years, volatility, is_call=True
    )
    return {
        "conId": contract_id,
        "expiry": expiry.strftime("%Y%m%d"),
        "strike": strike,
        "right": "C",
        "bid": price * (1.0 - 1e-10),
        "ask": price * (1.0 + 1e-10),
    }


def _quote(contract, *, rules):
    expiry = dt.datetime.strptime(contract["expiry"], "%Y%m%d").date()
    time_years = (expiry - EFFECTIVE_DATE).days / 365.0
    return quote_row(
        contract,
        1,
        EFFECTIVE_DATE,
        expiry.isoformat(),
        time_years,
        FORWARD,
        cleaning_rules=rules,
    )


def _snapshot(contracts):
    stamp = dt.datetime(2026, 10, 12, 16, 0, tzinfo=dt.timezone.utc)
    return {
        "ts": int(stamp.timestamp() * 1000),
        "spot_ref": FORWARD,
        "contracts": contracts,
    }


def test_cleaning_rule_boundaries_are_explicit_and_inclusive():
    at_premium_floor = _contract(
        "premium-at-floor", premium=0.05, relative_spread=0.1
    )
    below_premium_floor = _contract(
        "premium-below-floor", premium=0.049999, relative_spread=0.1
    )
    at_spread_cap = _contract(
        "spread-at-cap", premium=1.0, relative_spread=0.6666666667
    )
    above_spread_cap = _contract(
        "spread-above-cap", premium=1.0, relative_spread=0.6666666668
    )
    at_delta_floor = _contract_for_delta(
        "delta-at-floor", 0.02, time_years=2.0, volatility=2.0
    )
    below_delta_floor = _contract_for_delta(
        "delta-below-floor", 0.019, time_years=2.0, volatility=2.0
    )
    at_delta_ceiling = _contract_for_delta(
        "delta-at-ceiling", 0.95, time_years=20.0, volatility=1.0
    )
    above_delta_ceiling = _contract_for_delta(
        "delta-above-ceiling", 0.96, time_years=20.0, volatility=1.0
    )

    assert cleaning_reasons(
        0.0, 0.1, 0.8, FORWARD, 105.0, 2.0, "C", rules=("minimum_bid",)
    ) == ("minimum_bid",)
    assert _quote(at_premium_floor, rules=("minimum_premium",))[1]
    row, included = _quote(below_premium_floor, rules=("minimum_premium",))
    assert not included
    assert row[12] == "minimum_premium"
    assert _quote(at_spread_cap, rules=("relative_spread",))[1]
    row, included = _quote(above_spread_cap, rules=("relative_spread",))
    assert not included
    assert row[12] == "relative_spread"
    assert _quote(at_delta_floor, rules=("delta_window",))[1]
    row, included = _quote(below_delta_floor, rules=("delta_window",))
    assert not included
    assert row[12] == "delta_window"
    assert _quote(at_delta_ceiling, rules=("delta_window",))[1]
    row, included = _quote(above_delta_ceiling, rules=("delta_window",))
    assert not included
    assert row[12] == "delta_window"


def test_converter_and_replay_share_cleaning_reasons_and_counts(tmp_path):
    zero_bid = _contract("zero-bid", premium=0.1, relative_spread=0.0)
    zero_bid["bid"] = 0.0
    contracts = [
        _contract("kept"),
        _contract("low-premium", premium=0.04, relative_spread=0.1),
        _contract("wide-spread", premium=1.0, relative_spread=0.8),
        _contract("overlap", premium=0.04, relative_spread=0.8),
        _contract_for_delta("low-delta", 0.019, time_years=2.0, volatility=2.0),
        zero_bid,
    ]
    snapshot = _snapshot(contracts)

    replay = prepare_snapshot(snapshot, "IBIT", cleaning_rules=RULES)
    output_dir = tmp_path / "converted"
    output_dir.mkdir()
    output_path, row_count, valid_count, converter_counts = convert_snapshot(
        snapshot, output_dir, "synthetic.jsonl", 1, cleaning_rules=RULES
    )

    assert output_path is not None
    assert row_count == len(contracts)
    assert valid_count == replay.valid_quotes == 1
    assert replay.cleaning_exclusions == converter_counts
    assert converter_counts == {
        "minimum_bid": 0,
        "minimum_premium": 2,
        "relative_spread": 2,
        "delta_window": 1,
        "combined": 4,
    }
    with output_path.open(newline="", encoding="utf-8") as source:
        converter_rows = {row["quote_id"]: row for row in csv.DictReader(source)}
    replay_rows = {row[0]: row for row in replay.csv_rows}
    assert set(converter_rows) == set(replay_rows)
    assert {
        quote_id: row["included"] for quote_id, row in converter_rows.items()
    } == {quote_id: row[11] for quote_id, row in replay_rows.items()}
    assert converter_rows["overlap"]["filter_reason"] == (
        "minimum_premium;relative_spread"
    )
    assert converter_rows["overlap"]["included"] == "false"
    assert converter_rows["overlap"]["bid_iv"] == ""
    assert converter_rows["overlap"]["ask_iv"] == ""
    assert converter_rows["overlap"]["mid_iv"] == ""
    assert replay.excluded_cleaning_combined == converter_counts["combined"]
    assert replay.valid_quotes + replay.excluded_other == len(contracts)


def test_converter_and_replay_reports_exclusions_per_snapshot(tmp_path, capsys):
    contracts = [
        _contract("kept"),
        _contract("low-premium", premium=0.04, relative_spread=0.1),
        _contract("wide-spread", premium=1.0, relative_spread=0.8),
        _contract("overlap", premium=0.04, relative_spread=0.8),
    ]
    snapshot = _snapshot(contracts)
    source_root = tmp_path / "source"
    input_dir = source_root / "ibit"
    input_dir.mkdir(parents=True)
    source_file = input_dir / "chain_20261012.jsonl.gz"
    with gzip.open(source_file, "wt", encoding="utf-8") as stream:
        stream.write(json.dumps(snapshot) + "\n")

    converter_dir = tmp_path / "converted"
    converter_dir.mkdir()
    assert convert_directory(input_dir, converter_dir) == 1
    converter_log = capsys.readouterr().out
    replay_dir = tmp_path / "replay"
    manifest_path, report_path, snapshots, report = prepare_data(source_root, replay_dir)
    replay_log = capsys.readouterr().out

    expected = prepare_snapshot(snapshot, "IBIT")
    with manifest_path.open(newline="", encoding="utf-8") as stream:
        manifest_rows = list(csv.DictReader(stream, delimiter="\t"))
    assert len(manifest_rows) == 1
    manifest_row = manifest_rows[0]
    for rule in RULES:
        key = f"excluded_cleaning_{rule}"
        assert int(manifest_row[key]) == expected.cleaning_exclusions[rule]
        assert f"{key}={expected.cleaning_exclusions[rule]}" in replay_log
    assert int(manifest_row["excluded_cleaning_combined"]) == (
        expected.excluded_cleaning_combined
    )
    assert f"excluded_cleaning_combined={expected.excluded_cleaning_combined}" in replay_log
    assert "excluded_minimum_bid=" in converter_log
    assert "excluded_minimum_premium=" in converter_log
    assert "excluded_relative_spread=" in converter_log
    assert "excluded_delta_window=" in converter_log
    assert "excluded_cleaning_combined=" in converter_log
    with report_path.open(encoding="utf-8") as stream:
        serialized_report = json.load(stream)
    assert report["excluded_cleaning_by_rule"] == serialized_report[
        "excluded_cleaning_by_rule"
    ]
    assert report["excluded_cleaning_combined"] == expected.excluded_cleaning_combined
    assert len(report["cleaning_exclusions_by_snapshot"]) == 1
    snapshot_counts = report["cleaning_exclusions_by_snapshot"][0]
    assert snapshot_counts["source_file"] == "chain_20261012.jsonl.gz:1"
    assert snapshot_counts["by_rule"] == {
        rule: expected.cleaning_exclusions[rule] for rule in RULES
    }
    assert snapshot_counts["combined"] == expected.excluded_cleaning_combined
    assert serialized_report["cleaning_exclusions_by_snapshot"] == report[
        "cleaning_exclusions_by_snapshot"
    ]
    assert len(snapshots) == 1


def test_single_rule_profiles_report_rule_local_and_combined_counts():
    contracts = [
        _contract("kept"),
        _contract("low-premium", premium=0.04, relative_spread=0.1),
    ]
    prepared = prepare_snapshot(
        _snapshot(contracts), "IBIT", cleaning_rules=("minimum_premium",)
    )

    assert prepared.valid_quotes == 1
    assert prepared.cleaning_exclusions["minimum_premium"] == 1
    assert prepared.cleaning_exclusions["relative_spread"] == 0
    assert prepared.cleaning_exclusions["combined"] == 1
    assert prepared.excluded_cleaning_combined == 1
