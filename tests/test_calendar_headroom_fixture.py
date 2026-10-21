import csv
from datetime import date
from pathlib import Path

import pytest

ROOT = Path(__file__).parents[1]
FIXTURE = ROOT / "tests/fixtures/deribit_btc_20261018T1200_quotes.csv"
LEGACY_HEADER = [
    "quote_id",
    "effective_date",
    "expiry_date",
    "time_to_expiry",
    "forward",
    "discount_factor",
    "strike",
    "option_type",
    "bid_iv",
    "ask_iv",
    "mid_iv",
    "included",
    "filter_reason",
]


def test_fixture_time_to_expiry_matches_calendar_day_contract():
    with FIXTURE.open(newline="") as source:
        rows = list(csv.DictReader(source))
    for row in rows:
        days = (
            date.fromisoformat(row["expiry_date"])
            - date.fromisoformat(row["effective_date"])
        ).days
        assert float(row["time_to_expiry"]) == pytest.approx(days / 365.0, abs=1e-12)


def test_fixture_uses_legacy_schema_for_unit_weights():
    with FIXTURE.open(newline="") as source:
        rows = list(csv.reader(source))
    assert rows[0] == LEGACY_HEADER
    assert all(len(row) == len(LEGACY_HEADER) for row in rows[1:])
