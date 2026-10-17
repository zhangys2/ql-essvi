import math

from scripts.analyze_fit_quality import (
    contribution_percentages,
    grouped_band_width_quality,
    relative_total_variance_band_width,
    source_contract_index,
)


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
