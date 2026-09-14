import csv
import json
from pathlib import Path

ROOT = Path(__file__).parents[1]
FIELDS = ["time_to_expiry", "forward", "discount_factor", "log_moneyness", "strike", "fitted_total_variance", "fitted_iv", "is_tail"]


def test_committed_common_grids_are_identical_contracts():
    grids = []
    for name in ("essvi_common_grid.csv", "cvi_common_grid.csv"):
        with (ROOT / name).open(newline="") as handle:
            rows = list(csv.DictReader(handle))
        assert list(rows[0]) == FIELDS
        assert len(rows) == 5050
        grids.append([(r["time_to_expiry"], r["log_moneyness"]) for r in rows])
    assert grids[0] == grids[1]
    for name in ("essvi_common_score.json", "cvi_common_score.json"):
        score = json.loads((ROOT / name).read_text())
        assert score["grid_points"] == 5050
