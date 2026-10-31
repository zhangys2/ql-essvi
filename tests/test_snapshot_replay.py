import datetime as dt
import gzip
import sys
from pathlib import Path

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "python"))

from convert_ibit_snapshots import black_scholes_price
from replay_snapshots import (
    assign_replay_modes,
    discover_snapshot_files,
    matched_iv_changes,
    prepare_snapshot,
)
from run_snapshot_replay import _daily_result, split_replay_segments


def _contract(
    con_id,
    *,
    iv=0.8,
    bid_factor=0.95,
    ask_factor=1.05,
    strike=105.0,
    expiry=dt.date(2026, 10, 16),
):
    time_years = 1.0 / 365.0
    price = black_scholes_price(100.0, strike, time_years, iv, True)
    return {
        "conId": con_id,
        "expiry": expiry.strftime("%Y%m%d"),
        "strike": strike,
        "right": "C",
        "bid": price * bid_factor,
        "ask": price * ask_factor,
    }


def _snapshot(ts, contracts):
    return {"ts": ts, "spot_ref": 100.0, "contracts": contracts}


def test_discovery_supports_both_sources_in_sorted_order(tmp_path):
    (tmp_path / "ibit").mkdir()
    (tmp_path / "zdte").mkdir()
    for rel in (
        "ibit/chain_20261015.jsonl.gz",
        "ibit/chain_20261014.jsonl.gz",
        "zdte/spy_20261015.jsonl.gz",
        "zdte/spy_20261014.jsonl.gz",
    ):
        with gzip.open(tmp_path / rel, "wt", encoding="utf-8") as stream:
            stream.write("{}\n")

    sources = discover_snapshot_files(tmp_path)

    assert [(asset, path.name) for asset, path in sources] == [
        ("IBIT", "chain_20261014.jsonl.gz"),
        ("IBIT", "chain_20261015.jsonl.gz"),
        ("SPY", "spy_20261014.jsonl.gz"),
        ("SPY", "spy_20261015.jsonl.gz"),
    ]


def test_strict_preparation_excludes_locked_crossed_and_other_quotes():
    contracts = [
        _contract(1),
        _contract(2, bid_factor=1.0, ask_factor=1.0),
        _contract(3, bid_factor=1.1, ask_factor=0.9),
        _contract(4, strike=99.0, bid_factor=0.9, ask_factor=1.1),
    ]
    prepared = prepare_snapshot(
        _snapshot(1792071900000, contracts), "SPY"
    )

    assert prepared.status == "usable"
    assert prepared.valid_quotes == 1
    assert prepared.excluded_locked == 1
    assert prepared.excluded_crossed == 1
    assert prepared.excluded_other == 1
    assert sum(
        (prepared.valid_quotes, prepared.excluded_locked,
         prepared.excluded_crossed, prepared.excluded_other)
    ) == len(contracts)
    assert prepared.quotes[0].contract_id == "1"
    assert prepared.quotes[0].time_years == 1.0 / 365.0


def test_iv_changes_match_contract_identity_not_source_row_order():
    first = prepare_snapshot(
        _snapshot(1792071900000, [_contract(11, iv=0.8), _contract(12, iv=0.7)]),
        "IBIT",
    )
    second = prepare_snapshot(
        _snapshot(1792072200000, [_contract(12, iv=0.7), _contract(11, iv=0.82), _contract(13)]),
        "IBIT",
    )

    changes = matched_iv_changes(first, second)

    assert [row["contract_id"] for row in changes] == ["11", "12"]
    change = next(row["delta_iv"] for row in changes if row["contract_id"] == "11")
    assert abs(change - 0.02) < 5e-6
    assert all(row["timestamp_ms"] == second.timestamp_ms for row in changes)


def test_replay_modes_cold_restart_after_skips_and_timestamp_gaps():
    rows = [
        {"asset": "IBIT", "session_date": "2026-10-15", "timestamp_ms": 0, "status": "usable", "quote_file": "q0.csv"},
        {"asset": "IBIT", "session_date": "2026-10-15", "timestamp_ms": 300_000, "status": "usable", "quote_file": "q1.csv"},
        {"asset": "IBIT", "session_date": "2026-10-15", "timestamp_ms": 900_000, "status": "unusable", "quote_file": ""},
        {"asset": "IBIT", "session_date": "2026-10-15", "timestamp_ms": 1_800_000, "status": "usable", "quote_file": "q3.csv"},
        {"asset": "IBIT", "session_date": "2026-10-15", "timestamp_ms": 2_400_000, "status": "usable", "quote_file": "q5.csv"},
        {"asset": "SPY", "session_date": "2026-10-15", "timestamp_ms": 1_800_000, "status": "usable", "quote_file": "s0.csv"},
    ]

    replay_rows = assign_replay_modes(rows)

    assert [row["mode"] for row in replay_rows] == [
        "cold", "warm", "skip", "cold", "cold", "cold"
    ]
    assert replay_rows[3]["restart_reason"] == "unusable_snapshot"
    assert (
        replay_rows[2]["missing_intervals"],
        replay_rows[3]["missing_intervals"],
        replay_rows[4]["missing_intervals"],
    ) == (1, 2, 1)
    assert replay_rows[4]["restart_reason"] == "timestamp_gap"
    assert replay_rows[5]["restart_reason"] == "session_start"

    segments = split_replay_segments(replay_rows)

    assert [[row["timestamp_ms"] for row in segment] for segment in segments] == [[0, 300_000], [1_800_000], [2_400_000], [1_800_000]]


def test_daily_replay_aggregates_surface_movement_metrics_by_update():
    rows = [{
        "asset": "IBIT", "session_date": "2026-10-15",
        "status": "usable", "mode": "cold", "restart_reason": "session_start",
        "missing_intervals": 0, "excluded_locked": 0,
        "excluded_crossed": 0, "excluded_other": 0,
    }]
    summaries = [
        {
            "surface_movement_updates": 2,
            "warm_movement_rms_total_variance": 0.2,
            "warm_movement_matched_expiries": 6,
            "warm_movement_grid_points": 606,
            "cold_movement_rms_total_variance": 0.4,
            "cold_movement_matched_expiries": 6,
            "cold_movement_grid_points": 606,
        },
        {
            "surface_movement_updates": 1,
            "warm_movement_rms_total_variance": 0.5,
            "warm_movement_matched_expiries": 2,
            "warm_movement_grid_points": 202,
            "cold_movement_rms_total_variance": 0.7,
            "cold_movement_matched_expiries": 2,
            "cold_movement_grid_points": 202,
        },
    ]

    result = _daily_result(rows, summaries)

    assert result["surface_movement_updates"] == 3
    assert result["warm_movement_rms_total_variance"] == 0.3
    assert result["warm_movement_matched_expiries"] == 8
    assert result["warm_movement_grid_points"] == 808
    assert result["cold_movement_rms_total_variance"] == 0.5
    assert result["cold_movement_matched_expiries"] == 8
    assert result["cold_movement_grid_points"] == 808


def test_daily_replay_reports_unavailable_surface_movement_as_null():
    rows = [{
        "asset": "IBIT", "session_date": "2026-10-15",
        "status": "usable", "mode": "cold", "restart_reason": "session_start",
        "missing_intervals": 0, "excluded_locked": 0,
        "excluded_crossed": 0, "excluded_other": 0,
    }]

    result = _daily_result(rows, [])

    assert result["surface_movement_updates"] == 0
    for path in ("warm", "cold"):
        assert result[f"{path}_movement_rms_total_variance"] is None
        assert result[f"{path}_movement_matched_expiries"] is None
        assert result[f"{path}_movement_grid_points"] is None


def test_daily_replay_fallback_rate_includes_degraded_updates():
    rows = [{
        "asset": "IBIT", "session_date": "2026-10-15",
        "status": "usable", "mode": "cold", "restart_reason": "session_start",
        "missing_intervals": 0, "excluded_locked": 0,
        "excluded_crossed": 0, "excluded_other": 0,
    }]
    summaries = [{
        "samples": 4,
        "path_counts": {"cold": 1, "warm": 1, "fallback": 1, "degraded": 1},
    }]

    result = _daily_result(rows, summaries)

    assert result["fallback_updates"] == 2
    assert result["fallback_rate_percent"] == 50.0
