"""Black-Scholes price sensitivity and snapshot-normalized quote weights."""

import math
import statistics

WEIGHT_INDEX = 13
CONTRACT_MULTIPLIER = 100
TICK_SIZE = 0.01


def raw_price_sensitivity(sigma, forward, strike, time_years, discount_factor=1.0):
    """Return dC/dw in option-share dollars per unit total variance."""
    values = (sigma, forward, strike, time_years, discount_factor)
    if not all(math.isfinite(value) for value in values):
        raise ValueError("Black-Scholes sensitivity inputs must be finite")
    if sigma <= 0.0 or forward <= 0.0 or strike <= 0.0 or time_years <= 0.0:
        raise ValueError("Black-Scholes sensitivity inputs must be positive")
    if discount_factor <= 0.0:
        raise ValueError("discount_factor must be positive")

    sigma_root_t = sigma * math.sqrt(time_years)
    d1 = (
        math.log(forward / strike) + 0.5 * sigma_root_t * sigma_root_t
    ) / sigma_root_t
    density = math.exp(-0.5 * d1 * d1) / math.sqrt(2.0 * math.pi)
    sensitivity = discount_factor * forward * density / (2.0 * sigma_root_t)
    if not math.isfinite(sensitivity) or sensitivity <= 0.0:
        raise ValueError("Black-Scholes sensitivity is not finite and positive")
    return sensitivity


def median(values):
    """Return a deterministic statistical median for a nonempty sequence."""
    if not values:
        raise ValueError("cannot take the median of an empty sequence")
    return statistics.median(values)


def normalize_snapshot_weights(rows):
    """Append/update `weight` with raw sensitivity divided by its snapshot median."""
    sensitivities = []
    for row in rows:
        if len(row) == WEIGHT_INDEX:
            row.append("")
        elif len(row) != WEIGHT_INDEX + 1:
            raise ValueError("normalized quote row has an unexpected column count")
        if row[11] != "true":
            row[WEIGHT_INDEX] = ""
            continue
        sensitivity = raw_price_sensitivity(
            float(row[10]),
            float(row[4]),
            float(row[6]),
            float(row[3]),
            float(row[5]),
        )
        sensitivities.append((row, sensitivity))

    if not sensitivities:
        return rows
    scale = median([sensitivity for _, sensitivity in sensitivities])
    for row, sensitivity in sensitivities:
        row[WEIGHT_INDEX] = format(sensitivity / scale, ".17g")
    return rows


def normalize_quote_dict_weights(rows):
    """Normalize quote dictionaries to median-one weights per effective date."""
    sensitivities_by_date = {}
    for row in rows:
        row["weight"] = ""
        if row["included"] != "true":
            continue
        sensitivity = raw_price_sensitivity(
            float(row["mid_iv"]),
            float(row["forward"]),
            float(row["strike"]),
            float(row["time_to_expiry"]),
            float(row["discount_factor"]),
        )
        sensitivities_by_date.setdefault(row["effective_date"], []).append(
            (row, sensitivity)
        )

    for sensitivities in sensitivities_by_date.values():
        scale = median([sensitivity for _, sensitivity in sensitivities])
        for row, sensitivity in sensitivities:
            row["weight"] = format(sensitivity / scale, ".17g")
    return rows


def rows_for_weight_mode(rows, mode):
    """Return independent CSV rows for a unit-weight or vega-weighted fit."""
    if mode not in {"unit", "vega"}:
        raise ValueError(f"unsupported weight mode: {mode!r}")
    if mode == "vega":
        return rows
    result = []
    for row in rows:
        copy = list(row)
        if copy[11] == "true":
            copy[WEIGHT_INDEX] = "1"
        result.append(copy)
    return result
