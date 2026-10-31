import gc
import math
from pathlib import Path

import pytest


TRUTH = (
    (0.25, 0.010, 0.04, -0.50),
    (0.50, 0.020, 0.075, -0.10),
    (1.00, 0.040, 0.14, -0.20),
)


def variance(theta, psi, rho, k):
    return 0.5 * (
        theta + rho * psi * k
        + math.sqrt((psi * k + theta * rho) ** 2 + theta**2 * (1 - rho**2))
    )


def parameters(surface):
    return [(s.expiry, s.theta, s.psi, s.rho) for s in surface.slices]


@pytest.fixture(scope="module")
def quotes(essvi):
    result = essvi.Quotes()
    for expiry, theta, psi, rho in TRUTH:
        for index in range(-6, 7):
            k = 0.05 * index
            iv = math.sqrt(variance(theta, psi, rho, k) / expiry)
            result.append(essvi.Quote(
                id=f"{expiry}-{index}", expiry=expiry, forward=100.0,
                discount_factor=1.0, strike=100.0 * math.exp(k),
                bid_iv=iv - 0.0005, ask_iv=iv + 0.0005, mid_iv=iv,
                included=True, weight=1.0,
            ))
    return result


@pytest.fixture(scope="module")
def cold(quotes, essvi):
    result = essvi.calibrate(quotes)
    assert result.converged, result.error
    return result


def test_cold_fit_recovers_nonmonotone_smile(quotes, cold):
    slices = cold.surface.slices
    assert [s.expiry for s in slices] == [s[0] for s in TRUTH]
    assert slices[0].rho < -0.25 < slices[1].rho
    assert slices[2].rho < slices[1].rho
    for quote in quotes:
        iv = cold.surface.implied_volatility(
            quote.expiry, math.log(quote.strike / quote.forward)
        )
        assert quote.bid_iv - 1e-5 <= iv <= quote.ask_iv + 1e-5


def test_surface_lifetime_and_corbetta_interpolation(quotes, essvi):
    surface = essvi.calibrate(quotes).surface
    gc.collect()
    first, second, last = surface.slices
    expiry = 0.5 * (first.expiry + second.expiry)
    theta = 0.5 * (first.theta + second.theta)
    psi = 0.5 * (first.psi + second.psi)
    rho = 0.5 * (first.rho * first.psi + second.rho * second.psi) / psi
    interpolated = surface.slice_at(expiry)
    assert (interpolated.theta, interpolated.psi, interpolated.rho) == pytest.approx(
        (theta, psi, rho), rel=1e-12
    )
    expected = variance(theta, psi, rho, -0.17)
    assert surface.total_variance(expiry, -0.17) == pytest.approx(expected, rel=1e-12)
    assert surface.implied_volatility(expiry, -0.17) == pytest.approx(
        math.sqrt(expected / expiry), rel=1e-12
    )
    assert surface.slice_at(first.expiry / 2).theta == first.theta
    assert surface.slice_at(last.expiry * 2).theta == last.theta


def test_warm_state_keeps_reuse_age_and_periodic_refit(quotes, cold, essvi):
    previous = cold
    for age in range(1, 13):
        previous = essvi.calibrate_warm(quotes, previous, quotes)
        assert previous.converged, previous.error
        assert previous.path == essvi.CalibrationPath.warm
        assert previous.slices_reused == 3
        assert previous.reuse_ages == [age] * 3
        assert parameters(previous.surface) == parameters(cold.surface)
    refreshed = essvi.calibrate_warm(quotes, previous, quotes)
    assert refreshed.converged, refreshed.error
    assert refreshed.slices_refit == 3
    assert refreshed.slices_reused == 0
    assert refreshed.reuse_ages == [0] * 3


def test_changed_snapshot_refits_changed_expiry(quotes, cold, essvi):
    current = essvi.Quotes()
    for quote in quotes:
        shift = 0.005 if quote.expiry == TRUTH[-1][0] else 0.0
        current.append(essvi.Quote(
            id=quote.id, expiry=quote.expiry, forward=quote.forward,
            discount_factor=quote.discount_factor, strike=quote.strike,
            bid_iv=quote.bid_iv + shift, ask_iv=quote.ask_iv + shift,
            mid_iv=quote.mid_iv + shift, included=quote.included,
            weight=quote.weight,
        ))
    options = essvi.WarmOptions()
    options.quote_tol = 0.0
    result = essvi.calibrate_warm(current, cold, quotes, options)
    assert result.converged, result.error
    assert result.slices_refit >= 1
    assert result.slices_refit + result.slices_reused == 3
    assert result.surface.implied_volatility(1.0, 0.0) == pytest.approx(
        0.205, abs=0.0006
    )
    assert parameters(result.surface)[:2] == parameters(cold.surface)[:2]


def test_none_previous_result_uses_cold_calibration(quotes, cold, essvi):
    result = essvi.calibrate_warm(quotes, None, essvi.Quotes())
    assert result.converged, result.error
    assert result.path == essvi.CalibrationPath.cold
    assert result.reuse_ages == [0] * 3
    assert parameters(result.surface) == parameters(cold.surface)


def test_posterior_covariance_state_is_readonly(quotes, essvi):
    result = essvi.calibrate(quotes)
    assert result.converged, result.error
    covariance_snapshot = result.posterior_covariances
    assert isinstance(covariance_snapshot, tuple)
    assert len(covariance_snapshot) == len(result.surface.slices)
    assert all(len(covariance) == 6 for covariance in covariance_snapshot)
    with pytest.raises(TypeError):
        covariance_snapshot[0][0] = 0.0
    with pytest.raises(AttributeError):
        result.posterior_covariances = []


def test_empty_quotes_return_failed_result(essvi):
    result = essvi.calibrate(essvi.Quotes())
    assert not result.converged
    assert result.surface.slices == []
    assert math.isnan(result.surface.total_variance(0.5, 0.0))


@pytest.mark.parametrize("weight", [0.0, -1.0, math.nan, math.inf])
def test_invalid_included_weight_cannot_produce_surface(essvi, weight):
    quotes = essvi.Quotes([
        essvi.Quote("invalid", 0.5, 100.0, 1.0, 100.0, 0.19, 0.21, 0.20,
                    included=True, weight=weight)
    ])
    result = essvi.calibrate(quotes)
    assert not result.converged
    assert result.surface.slices == []


@pytest.mark.parametrize("field,value", [("quote_tol", -1.0), ("quote_cap", math.nan)])
def test_invalid_warm_options_cannot_produce_surface(quotes, cold, field, value, essvi):
    options = essvi.WarmOptions()
    setattr(options, field, value)
    result = essvi.calibrate_warm(quotes, cold, quotes, options)
    assert not result.converged
    assert result.surface.slices == []


def test_calibration_requires_explicit_native_quote_container(quotes, essvi):
    with pytest.raises(TypeError):
        essvi.calibrate(list(quotes))



def test_parameter_view_is_shared_and_read_only(cold, np):
    view = cold.surface.parameter_view()
    assert isinstance(view, memoryview)
    assert view.readonly
    assert view.format == "d"
    assert view.shape == (len(TRUTH), 4)
    assert view.strides == (32, 8)
    array = np.asarray(view)
    np.testing.assert_array_equal(array, parameters(cold.surface))
    assert array.dtype == np.dtype("float64")
    assert array.flags.c_contiguous
    assert not array.flags.owndata
    assert np.shares_memory(array, np.asarray(cold.surface.parameter_view()))
    selected = array[:, 1:]
    assert np.shares_memory(selected, array)
    assert not selected.flags.c_contiguous
    with pytest.raises(TypeError):
        view[0, 1] = 1.0
    with pytest.raises(ValueError):
        array[0, 1] = 1.0
    with pytest.raises(ValueError):
        array.setflags(write=True)


def test_parameter_view_retains_result_through_numpy_slice(quotes, essvi, np):
    result = essvi.calibrate(quotes)
    assert result.converged, result.error
    view = result.surface.parameter_view()
    values = np.asarray(view)[:, 1:]
    expected = np.array(parameters(result.surface))[:, 1:]
    updated = essvi.calibrate_warm(quotes, result, quotes)
    assert updated.converged, updated.error
    assert not np.shares_memory(values, np.asarray(updated.surface.parameter_view()))
    del result, updated, view
    gc.collect()
    np.testing.assert_array_equal(values, expected)
    assert not values.flags.writeable


def test_empty_surface_parameter_view(essvi, np):
    view = essvi.calibrate(essvi.Quotes()).surface.parameter_view()
    assert isinstance(view, memoryview)
    assert view.shape == (0, 4)
    assert view.readonly
    assert view.nbytes == 0
    array = np.asarray(view)
    assert array.shape == (0, 4)
    assert array.dtype == np.dtype("float64")
    assert not array.flags.writeable



def test_market_fixture_warm_fallback_remains_usable(essvi):
    fixture = Path(__file__).parent / "fixtures/deribit_btc_20261018T1200_quotes.csv"
    quotes = essvi.read_quotes(fixture)
    previous = essvi.calibrate(quotes)
    assert previous.converged, previous.error
    result = essvi.calibrate_warm(quotes, previous, quotes)
    assert result.converged, result.error
    assert result.path == essvi.CalibrationPath.fallback
    assert result.fallback_slices > 0
    assert result.slices_refit + result.slices_reused == 9
    assert result.quality_quote_count == 360
    assert [s.expiry for s in result.surface.slices] == sorted({q.expiry for q in quotes})
    for slice in result.surface.slices:
        for k in (-0.2, 0.0, 0.2):
            assert result.surface.implied_volatility(slice.expiry, k) > 0.0


def test_warm_budget_exhaustion_exposes_degraded_state(essvi):
    previous_quotes = essvi.Quotes()
    state = 11
    mask = (1 << 64) - 1
    for i in range(10):
        expiry = 0.05 + 0.05 * i
        theta = 0.02 + 0.03 * expiry
        psi = 0.05 + 0.02 * expiry
        rho = -0.5 + 0.2 * (i / 9)
        for j in range(100):
            k = -0.5 + j / 99
            state = (state ^ (state << 13)) & mask
            state ^= state >> 7
            state = (state ^ (state << 17)) & mask
            noise = 2.0 * (state >> 11) * (1.0 / 9007199254740992.0) - 1.0
            mid = math.sqrt(variance(theta, psi, rho, k) / expiry) + noise * 0.00005
            half_spread = 0.003 * (1.0 + 2.0 * abs(k))
            previous_quotes.append(essvi.Quote(
                f"{i}-{j}", expiry, 100.0, 1.0, 100.0 * math.exp(k),
                max(0.001, mid - half_spread), mid + half_spread, mid,
            ))
    previous = essvi.calibrate(previous_quotes)
    assert previous.converged, previous.error
    current = essvi.Quotes()
    for index, quote in enumerate(previous_quotes):
        k = math.log(quote.strike / quote.forward)
        direction = 1.0 if index % 2 == 0 else -1.0
        mid = max(0.001, quote.mid_iv + direction * 0.01 * (1.0 + abs(k)))
        current.append(essvi.Quote(
            quote.id, quote.expiry, quote.forward, quote.discount_factor,
            quote.strike, mid, mid, mid,
        ))
    options = essvi.WarmOptions()
    options.quote_tol = 0.0
    options.quote_cap = 0.0
    result = essvi.calibrate_warm(current, previous, previous_quotes, options)
    assert result.converged, result.error
    assert result.path == essvi.CalibrationPath.degraded
    assert result.fallback_reason_counts.budget_exhausted > 0
    assert result.degraded_slices > 0
    assert result.degraded_slices == sum(age > 0 for age in result.degraded_ages)
    assert result.quality_quote_count == 1000
    assert math.isfinite(result.quality_rms_excess_ratio)
    assert [s.expiry for s in result.surface.slices] == [0.05 + 0.05 * i for i in range(10)]


def test_failed_cold_fit_keeps_partial_surface_view_alive(essvi, quotes):
    current = essvi.Quotes(quotes)
    current.append(essvi.Quote(
        "invalid-last-expiry", 1.5, 100.0, 1.0, 100.0, 0.20, 0.21, math.nan,
    ))
    result = essvi.calibrate(current)
    assert not result.converged
    assert [s.expiry for s in result.surface.slices] == [row[0] for row in TRUTH]
    view = result.surface.parameter_view()
    assert view.readonly and view.shape == (3, 4)
    del result
    gc.collect()
    assert [row[0] for row in view.tolist()] == [row[0] for row in TRUTH]
    assert all(math.isfinite(value) for row in view.tolist() for value in row)
