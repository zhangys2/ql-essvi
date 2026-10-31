import csv
import math
from pathlib import Path

import pytest


COLUMNS = (
    "quote_id", "time_to_expiry", "forward", "discount_factor", "strike",
    "bid_iv", "ask_iv", "mid_iv", "included",
)
VALID = {
    "quote_id": "valid", "time_to_expiry": "0.5", "forward": "100",
    "discount_factor": "0.99", "strike": "100", "bid_iv": "0.19",
    "ask_iv": "0.21", "mid_iv": "0.20", "included": "true",
}


def write_quotes(path, rows, columns=COLUMNS):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    return path


class QuotePath:
    def __init__(self, path):
        self.path = path

    def __fspath__(self):
        return str(self.path)


@pytest.mark.parametrize("path_type", [Path, QuotePath])
def test_loader_accepts_pathlike_and_preserves_excluded_rows(essvi, tmp_path, path_type):
    excluded = {column: "" for column in COLUMNS}
    excluded.update(quote_id="excluded", included="false")
    path = write_quotes(tmp_path / "quotes.csv", [VALID, excluded])
    quotes = essvi.read_quotes(path_type(path))
    assert [quote.id for quote in quotes] == ["valid", "excluded"]
    assert quotes[0].included and not quotes[1].included
    assert (quotes[0].expiry, quotes[0].forward, quotes[0].discount_factor,
            quotes[0].strike, quotes[0].bid_iv, quotes[0].ask_iv,
            quotes[0].mid_iv) == (0.5, 100.0, 0.99, 100.0, 0.19, 0.21, 0.20)
    assert quotes[0].weight == 1.0
    assert math.isnan(quotes[1].mid_iv)


@pytest.mark.parametrize("kind", ["missing", "directory"])
def test_loader_rejects_unreadable_input(essvi, tmp_path, kind):
    path = tmp_path / "quotes.csv"
    if kind == "directory":
        path.mkdir()
    with pytest.raises(RuntimeError) as error:
        essvi.read_quotes(str(path))
    assert str(path) in str(error.value)


@pytest.mark.parametrize("column", COLUMNS)
def test_loader_requires_quote_columns(essvi, tmp_path, column):
    columns = [name for name in COLUMNS if name != column]
    path = write_quotes(tmp_path / "missing-column.csv", [VALID], columns)
    with pytest.raises(ValueError) as error:
        essvi.read_quotes(str(path))
    assert column in str(error.value)
    assert str(path) in str(error.value)


@pytest.mark.parametrize("column,text", [
    ("bid_iv", "not_a_number"), ("mid_iv", "0.2junk"),
    ("time_to_expiry", "nan"), ("mid_iv", ""),
])
def test_loader_rejects_bad_middle_row_instead_of_dropping_it(essvi, tmp_path, column, text):
    bad = dict(VALID, quote_id="bad")
    bad[column] = text
    path = write_quotes(tmp_path / "bad-row.csv", [
        dict(VALID, quote_id="before"), bad, dict(VALID, quote_id="after"),
    ])
    with pytest.raises(ValueError) as error:
        essvi.read_quotes(str(path))
    assert str(path) in str(error.value)
    assert ":3" in str(error.value)
    assert column in str(error.value)


def test_loader_rejects_truncated_row(essvi, tmp_path):
    path = tmp_path / "short-row.csv"
    path.write_text(",".join(COLUMNS) + "\nvalid,0.5,100,0.99,100,0.19,0.21,0.2\n")
    with pytest.raises(ValueError) as error:
        essvi.read_quotes(str(path))
    assert str(path) in str(error.value)
    assert ":2" in str(error.value)


def test_loader_rejects_invalid_included_flag(essvi, tmp_path):
    path = write_quotes(tmp_path / "bad-included.csv", [dict(VALID, included="maybe")])
    with pytest.raises(ValueError) as error:
        essvi.read_quotes(str(path))
    assert "included" in str(error.value)


@pytest.mark.parametrize("text", ["invalid", "1.5junk"])
def test_loader_retains_bad_weights_for_calibration_rejection(essvi, tmp_path, text):
    path = write_quotes(tmp_path / "bad-weight.csv", [dict(VALID, weight=text)], COLUMNS + ("weight",))
    quotes = essvi.read_quotes(str(path))
    assert [quote.id for quote in quotes] == ["valid"]
    assert math.isnan(quotes[0].weight)
    result = essvi.calibrate(quotes)
    assert not result.converged
    assert result.surface.slices == []


@pytest.mark.parametrize("text,expected", [("", 1.0), ("2", 2.0)])
def test_loader_reads_optional_quote_weights(essvi, tmp_path, text, expected):
    path = write_quotes(tmp_path / "weighted.csv", [dict(VALID, weight=text)], COLUMNS + ("weight",))
    quotes = essvi.read_quotes(str(path))
    assert [quote.id for quote in quotes] == ["valid"]
    assert quotes[0].weight == expected
