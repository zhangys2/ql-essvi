import math
import sys
from pathlib import Path

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "python"))

from scripts.analyze_fit_quality import quality


def test_quality_reports_unweighted_vega_and_price_space_metrics():
    metrics = quality(
        [
            {
                "excess_ratio": 0.25,
                "excess": 0.2,
                "weight": 2.0,
                "raw_price_sensitivity": 3.0,
            },
            {
                "excess_ratio": 0.05,
                "excess": 0.1,
                "weight": 0.5,
                "raw_price_sensitivity": 2.0,
            },
        ]
    )

    assert metrics["quote_count"] == 2
    assert metrics["above_cap_count"] == 1
    assert metrics["share_above_cap"] == 0.5
    assert math.isclose(metrics["rms_excess_ratio"], math.sqrt(0.0325))
    assert metrics["maximum_excess_ratio"] == 0.25
    assert math.isclose(metrics["sum_squared_total_variance_excess"], 0.05)
    assert math.isclose(metrics["sum_squared_vega_weighted_excess"], 0.1625)
    assert math.isclose(metrics["vega_weighted_excess_rms"], math.sqrt(0.08125))
    assert metrics["vega_weighted_excess_maximum"] == 0.4
    assert math.isclose(metrics["price_excess_dollars_per_share_rms"], math.sqrt(0.2))
    assert math.isclose(metrics["price_excess_dollars_per_share_maximum"], 0.6)
    assert math.isclose(metrics["price_excess_ticks_rms"], math.sqrt(2000.0))
    assert math.isclose(metrics["price_excess_ticks_maximum"], 60.0)
    assert math.isclose(metrics["price_excess_dollars_per_contract_rms"], math.sqrt(2000.0))
    assert math.isclose(metrics["price_excess_dollars_per_contract_maximum"], 60.0)


def test_quality_metrics_are_zero_for_empty_quote_set():
    metrics = quality([])

    assert metrics["quote_count"] == 0
    assert metrics["share_above_cap"] == 0.0
    assert metrics["vega_weighted_excess_rms"] == 0.0
    assert metrics["price_excess_dollars_per_contract_maximum"] == 0.0
