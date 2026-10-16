#include "calibrator.hpp"
#include "synthetic_chain.hpp"
#include "realistic_warm.hpp"

#undef NDEBUG
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

namespace {

double normalizedCall(double forward, double strike, double variance) {
  if (variance <= 0.0) return std::max(forward - strike, 0.0);
  const double root = std::sqrt(variance);
  const double d1 = (std::log(forward / strike) + 0.5 * variance) / root;
  const double d2 = d1 - root;
  const auto cdf = [](double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
  };
  return forward * cdf(d1) - strike * cdf(d2);
}

void checkDenseGrid(const essvi::Surface& surface) {
  assert(essvi::validate(surface).empty());
  std::array<double, 101> previous_variance{};
  bool has_previous = false;
  for (std::size_t i = 0; i < surface.slices.size(); ++i) {
    const auto& slice = surface.slices[i];
    assert(essvi::butterflySafe(slice));
    std::array<double, 101> variance{};
    std::array<double, 101> call{};
    std::array<double, 101> strike{};
    for (int j = 0; j <= 100; ++j) {
      const double k = -0.5 + 0.01 * j;
      strike[static_cast<std::size_t>(j)] = 100.0 * std::exp(k);
      variance[static_cast<std::size_t>(j)] = essvi::sliceTotalVariance(slice, k);
      assert(variance[static_cast<std::size_t>(j)] > 0.0);
      call[static_cast<std::size_t>(j)] = normalizedCall(
          100.0, strike[static_cast<std::size_t>(j)],
          variance[static_cast<std::size_t>(j)]);
      if (has_previous)
        assert(variance[static_cast<std::size_t>(j)] >=
               previous_variance[static_cast<std::size_t>(j)] - 1e-8);
    }
    for (int j = 1; j < 100; ++j) {
      const auto index = static_cast<std::size_t>(j);
      const double left_slope = (call[index] - call[index - 1]) /
                                (strike[index] - strike[index - 1]);
      const double right_slope = (call[index + 1] - call[index]) /
                                 (strike[index + 1] - strike[index]);
      assert(right_slope >= left_slope - 1e-8);
    }
    if (i) assert(essvi::calendarSafe(surface.slices[i - 1], slice));
    previous_variance = variance;
    has_previous = true;
  }
}

void sameParameters(const essvi::Surface& a, const essvi::Surface& b) {
  assert(a.slices.size() == b.slices.size());
  for (std::size_t i = 0; i < a.slices.size(); ++i) {
    assert(a.slices[i].expiry == b.slices[i].expiry);
    assert(a.slices[i].theta == b.slices[i].theta);
    assert(a.slices[i].psi == b.slices[i].psi);
    assert(a.slices[i].rho == b.slices[i].rho);
  }
}

void warmUpdatesReuseAndRefitSafely() {
  auto previous_quotes = essvi::bench::syntheticChain(40, 100);
  auto cold = essvi::calibrate(previous_quotes);
  assert(cold.converged);
  checkDenseGrid(cold.surface);

  essvi::WarmOptions options;
  options.quote_tol = 1e-6;
  options.loss_tol = 1e-8;
  auto previous_result = cold;
  for (std::size_t update = 1; update <= 100; ++update) {
    auto current_quotes = previous_quotes;
    for (auto& quote : current_quotes) {
      quote.bid_iv += 1e-7;
      quote.ask_iv += 1e-7;
    }
    const bool reuse_allowed = previous_result.reuse_ages.empty() ||
                               previous_result.reuse_ages.front() < 12;
    const auto warm = essvi::calibrateWarm(current_quotes, &previous_result,
                                           previous_quotes, options);
    assert(warm.converged);
    assert(warm.path == essvi::CalibrationPath::warm);
    assert(warm.slices_reused == (reuse_allowed ? 40 : 0));
    assert(warm.slices_refit == (reuse_allowed ? 0 : 40));
    assert(warm.reuse_ages.size() == warm.surface.slices.size());
    for (const auto age : warm.reuse_ages) assert(age <= 12);
    assert(warm.objective <= cold.objective + 1e-12);
    assert(warm.objective_evaluations >= static_cast<std::uint64_t>(warm.iterations));
    assert(std::isfinite(warm.runtime_seconds));
    assert(warm.mm_bound_calls > 0);
    checkDenseGrid(warm.surface);
    previous_result = warm;
    previous_quotes = std::move(current_quotes);
  }

  auto changed_quotes = previous_quotes;
  for (auto& quote : changed_quotes) {
    if (quote.expiry == 0.05) {
      quote.bid_iv += 2e-4;
      quote.ask_iv += 2e-4;
    }
  }
  options.loss_tol = 1e-12;
  const auto warm_refit = essvi::calibrateWarm(changed_quotes, &previous_result,
                                               previous_quotes, options);
  const auto cold_refit = essvi::calibrate(changed_quotes);
  assert(warm_refit.converged && cold_refit.converged);
  assert(warm_refit.slices_refit > 0);
  assert(warm_refit.slices_reused + warm_refit.slices_refit == 40);
  assert(warm_refit.objective <= cold_refit.objective * (1.0 + 1e-3) + 1e-12);
  assert(warm_refit.objective <= cold_refit.objective * 1.05 + 1e-12);
  checkDenseGrid(warm_refit.surface);

  const auto repeated_a = essvi::calibrateWarm(changed_quotes, &previous_result,
                                               previous_quotes, options);
  const auto repeated_b = essvi::calibrateWarm(changed_quotes, &previous_result,
                                               previous_quotes, options);
  assert(repeated_a.converged && repeated_b.converged);
  sameParameters(repeated_a.surface, repeated_b.surface);

  const auto failed = essvi::calibrateWarm({}, &previous_result,
                                            previous_quotes, options);
  assert(!failed.converged);
  assert(!failed.error.empty());
  assert(failed.surface.slices.empty());

  auto no_atm = std::vector<essvi::Quote>{
      {"bad-atm", 0.5, 100.0, 1.0, 100.0, 0.2, 0.21, NAN, true}};
  const auto infeasible = essvi::calibrateWarm(no_atm, &previous_result,
                                                previous_quotes, options);
  assert(!infeasible.converged);
  assert(!infeasible.error.empty());
  assert(infeasible.surface.slices.empty());
}

void changedQuotesReuseWhenPreviousSurfaceStillFits() {
  const auto previous_quotes = essvi::bench::syntheticChain(3, 100);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  const auto no_previous = essvi::calibrateWarm(
      previous_quotes, nullptr, previous_quotes);
  assert(no_previous.converged);
  assert(no_previous.path == essvi::CalibrationPath::cold);
  assert(no_previous.slices_refit == previous.surface.slices.size());
  for (const auto age : no_previous.reuse_ages) assert(age == 0);
  sameParameters(no_previous.surface, previous.surface);

  auto current = previous_quotes;
  for (auto& quote : current) {
    if (quote.expiry != previous.surface.slices.back().expiry) continue;
    quote.bid_iv -= 1e-5;
    quote.ask_iv += 1e-5;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  const auto warm = essvi::calibrateWarm(
      current, &previous, previous_quotes, options);
  assert(warm.converged);
  assert(warm.slices_reused == 3);
  assert(warm.slices_refit == 0);
  assert(warm.quality_checks == 0);
  checkDenseGrid(warm.surface);
}

void perQuoteCapBoundaryAndWidthFloor() {
  const auto previous_quotes = essvi::bench::syntheticChain(3, 100);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  const auto& slice = previous.surface.slices.front();
  const auto quote_index = std::find_if(
      previous_quotes.begin(), previous_quotes.end(), [&](const auto& quote) {
        return quote.expiry == slice.expiry && quote.id == "0-0";
      });
  assert(quote_index != previous_quotes.end());
  const double k = std::log(quote_index->strike / quote_index->forward);
  const double fitted_variance = essvi::sliceTotalVariance(slice, k);

  auto boundary_quotes = previous_quotes;
  auto& boundary_quote = *std::find_if(
      boundary_quotes.begin(), boundary_quotes.end(), [&](const auto& quote) {
        return quote.expiry == slice.expiry && quote.id == "0-0";
      });
  constexpr double width = 1e-3;
  constexpr double target_ratio = 0.1;
  const double ask_variance = fitted_variance - target_ratio * width;
  const double bid_variance = ask_variance - width;
  boundary_quote.bid_iv = std::sqrt(bid_variance / boundary_quote.expiry);
  boundary_quote.ask_iv = std::sqrt(ask_variance / boundary_quote.expiry);
  const double actual_bid = boundary_quote.expiry * boundary_quote.bid_iv *
                            boundary_quote.bid_iv;
  const double actual_ask = boundary_quote.expiry * boundary_quote.ask_iv *
                            boundary_quote.ask_iv;
  const double actual_width = std::max(actual_ask - actual_bid, 1e-6);
  const double excess = std::max(
      0.0, std::max(actual_bid - fitted_variance,
                    fitted_variance - actual_ask));

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = excess / actual_width;
  const auto at_boundary = essvi::calibrateWarm(
      boundary_quotes, &previous, previous_quotes, options);
  assert(at_boundary.converged);
  assert(at_boundary.slices_reused == 3);
  assert(at_boundary.slices_refit == 0);

  auto zero_width_quotes = previous_quotes;
  auto& zero_width_quote = *std::find_if(
      zero_width_quotes.begin(), zero_width_quotes.end(), [&](const auto& quote) {
        return quote.expiry == slice.expiry && quote.id == "0-0";
      });
  zero_width_quote.bid_iv =
      std::sqrt((fitted_variance + 5e-8) / zero_width_quote.expiry);
  zero_width_quote.ask_iv = zero_width_quote.bid_iv;
  const double point_variance = zero_width_quote.expiry *
                                zero_width_quote.bid_iv * zero_width_quote.bid_iv;
  const double floor_ratio =
      std::abs(point_variance - fitted_variance) / 1e-6;
  options.quote_cap = floor_ratio;
  const auto floor_boundary = essvi::calibrateWarm(
      zero_width_quotes, &previous, previous_quotes, options);
  assert(floor_boundary.converged);
  assert(floor_boundary.slices_reused == 3);
  assert(floor_boundary.slices_refit == 0);
}

void perQuoteGateChecksSingleOutlierAgainstColdReference() {
  const auto previous_quotes = essvi::bench::realisticChain(3, 100, 7);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  const double changed_expiry = previous.surface.slices.front().expiry;
  for (auto& quote : current_quotes) {
    if (quote.expiry != changed_expiry || quote.id != "0-0") continue;
    const double k = std::log(quote.strike / quote.forward);
    const double shift = 0.004 * (1.0 + 2.0 * std::abs(k));
    quote.bid_iv += shift;
    quote.ask_iv += shift;
  }

  essvi::WarmOptions options;
  assert(options.quote_cap == 0.1);
  options.quote_tol = 0.01;
  options.quote_cap = 0.1;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  const auto cold = essvi::calibrate(current_quotes);
  assert(warm.converged && cold.converged);
  assert(warm.quality_checks > 0);
  assert(warm.slices_refit > 0);
  assert(warm.slices_reused + warm.slices_refit == 3);
  assert(warm.fallback_slices > 0);
  assert(warm.path == essvi::CalibrationPath::fallback);
  assert(warm.objective <= cold.objective * 1.05 + 1e-12);
  essvi::WarmOptions different_legacy_tolerance = options;
  different_legacy_tolerance.loss_tol = 1e6;
  const auto same_gate = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, different_legacy_tolerance);
  assert(same_gate.converged);
  sameParameters(warm.surface, same_gate.surface);
  assert(warm.quality_checks == same_gate.quality_checks);
  assert(warm.slices_reused == same_gate.slices_reused);
  assert(warm.slices_refit == same_gate.slices_refit);
  assert(warm.fallback_slices == same_gate.fallback_slices);
  checkDenseGrid(warm.surface);
}

void perQuoteGateFallsBackToColdSearch() {
  constexpr std::uint64_t seed = 2;
  const auto previous_quotes = essvi::bench::realisticChain(3, 100, seed);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current = previous_quotes;
  essvi::bench::Noise noise(seed + 1);
  {
      // Keep the pre-B7 synthetic fast move as a stress fixture.
      // Production scenarios use the measured sizes in realistic_warm.hpp.
      const double common_level = essvi::bench::signedAmplitude(0.01, noise);
      const double common_tilt = essvi::bench::signedAmplitude(0.002, noise);
      const double quiet_level = essvi::bench::signedAmplitude(0.0005, noise);
      const double front_level = essvi::bench::signedAmplitude(0.004, noise);
      const double back_level = front_level > 0.0 ? 0.0005 : -0.0005;
      (void)quiet_level;
      (void)back_level;
      for (std::size_t expiry_index = 0; expiry_index < 3; ++expiry_index) {
        const double idiosyncratic = noise.centered() * 0.0002;
        const std::size_t begin = expiry_index * 100;
        const std::size_t end = std::min(begin + 100, current.size());
        for (std::size_t i = begin; i < end; ++i) {
          auto& quote = current[i];
          const double k = std::log(quote.strike / quote.forward);
          const double tilt = common_tilt * std::clamp(2.0 * k, -1.0, 1.0);
          const double change = common_level + idiosyncratic + tilt +
                                noise.centered() * 0.0001;
          quote.bid_iv = std::max(0.001, quote.bid_iv + change);
          quote.ask_iv = std::max(quote.bid_iv, quote.ask_iv + change);
          quote.mid_iv = std::max(0.001, quote.mid_iv + change);
        }
      }
    }
  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.loss_tol = 0.0;  // Deprecated; this fallback comes from the quote cap.
  const auto warm = essvi::calibrateWarm(
      current, &previous, previous_quotes, options);
  const auto cold = essvi::calibrate(current);
  assert(warm.converged && cold.converged);
  assert(warm.path == essvi::CalibrationPath::fallback);
  assert(warm.fallback_slices > 0);
  assert(warm.quality_checks > 0);
  assert(warm.quality_loss_scale > 0.0);
  assert(warm.quality_warm_loss > 0.0);
  assert(std::isfinite(warm.quality_reference_loss));
  assert(warm.loss_tolerance == options.loss_tol);
  assert(warm.local_solver_slices >= 3);

  const auto repeated = essvi::calibrateWarm(
      current, &previous, previous_quotes, options);
  assert(repeated.converged);
  sameParameters(warm.surface, repeated.surface);
  assert(warm.objective <= cold.objective * 1.05 + 1e-12);
  checkDenseGrid(warm.surface);
}

void sequentialCalendarRepairRetriesRejectedCandidates() {
  const auto previous_quotes = essvi::bench::realisticChain(4, 100, 1);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  essvi::bench::Noise noise(102);
  essvi::bench::evolveWarmScenario(
      current_quotes, 4, 100, essvi::bench::WarmScenario::fast, 1, noise);
  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.loss_tol = 0.0;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  const auto repeated = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  assert(warm.converged && repeated.converged);
  assert(warm.local_solver_slices > 4);
  assert(warm.slices_refit + warm.slices_reused == 4);
  sameParameters(warm.surface, repeated.surface);
  checkDenseGrid(warm.surface);
}

void failedJacobiPrefitFallsBackWithoutRetry() {
  const auto previous_quotes = essvi::bench::realisticChain(3, 100, 1);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  const double first_expiry = previous.surface.slices[0].expiry;
  const double failing_expiry = previous.surface.slices[1].expiry;
  for (auto& quote : current_quotes) {
    if (quote.expiry == first_expiry) {
      quote.bid_iv += 0.01;
      quote.ask_iv += 0.01;
      quote.mid_iv += 0.01;
    } else if (quote.expiry == failing_expiry) {
      quote.bid_iv = 0.49;
      quote.ask_iv = 0.50;
      quote.mid_iv = std::nan("");
    }
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.loss_tol = 0.0;
  options.quote_cap = 1.0;  // Keep the unrelated unchanged slice reusable.
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  assert(!warm.converged);
  assert(warm.error == "no ATM quote");
  assert(warm.local_solver_slices == 2);
  assert(warm.slices_refit == 1);
  assert(warm.slices_reused == 0);
}

void localWarmRefitHonorsBudgetAndQuality() {
  const auto previous_quotes = essvi::bench::realisticChain(3, 100, 1);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  const auto& last_slice = previous.surface.slices.back();
  const essvi::Slice target_slice{last_slice.expiry, last_slice.theta + 0.003,
                                  last_slice.psi, last_slice.rho};
  auto target_surface = previous.surface;
  target_surface.slices.back() = target_slice;
  assert(essvi::validate(target_surface).empty());
  for (auto& quote : current_quotes) {
    if (quote.expiry != target_slice.expiry) continue;
    const double k = std::log(quote.strike / quote.forward);
    const double mid = std::sqrt(
        essvi::sliceTotalVariance(target_slice, k) / quote.expiry);
    const double half_spread = 0.003 * (1.0 + 2.0 * std::abs(k));
    quote.bid_iv = std::max(0.001, mid - half_spread);
    quote.ask_iv = mid + half_spread;
    quote.mid_iv = mid;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 0.5;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  const auto cold = essvi::calibrate(current_quotes);
  assert(warm.converged && cold.converged);
  assert(warm.slices_refit == 1);
  assert(warm.slices_reused == 2);
  assert(warm.local_solver_slices == 1);
  assert(warm.local_objective_evaluations <= 15);
  assert(warm.local_mm_bound_calls <= 11);
  assert(warm.objective <= cold.objective * 1.05 + 1e-12);
  checkDenseGrid(warm.surface);
}
}  // namespace

int main() {
  warmUpdatesReuseAndRefitSafely();
  changedQuotesReuseWhenPreviousSurfaceStillFits();
  perQuoteCapBoundaryAndWidthFloor();
  perQuoteGateChecksSingleOutlierAgainstColdReference();
  perQuoteGateFallsBackToColdSearch();
  sequentialCalendarRepairRetriesRejectedCandidates();
  failedJacobiPrefitFallsBackWithoutRetry();
  localWarmRefitHonorsBudgetAndQuality();
  return 0;
}
