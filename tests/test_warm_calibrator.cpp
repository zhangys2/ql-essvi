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

void checkPosteriorCovariances(const essvi::CalibrationResult& result);

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
  assert(at_boundary.quality_maximum_excess_ratio <= options.quote_cap + 1e-8);

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
  assert(floor_boundary.quality_maximum_excess_ratio <= options.quote_cap + 1e-8);
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
  auto covariance_prior = previous;
  const std::array<double, 6> prior_covariance{0.4, 0.0, 0.0, 0.6, 0.0, 0.8};
  for (auto& covariance : covariance_prior.posterior_covariances)
    covariance = prior_covariance;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &covariance_prior, previous_quotes, options);
  assert(warm.converged);
  assert(warm.quality_checks > 0);
  assert(warm.slices_refit > 0);
  assert(warm.slices_reused + warm.slices_refit == 3);
  assert(warm.fallback_slices > 0);
  assert(warm.fallback_reason_counts.loss_above_gate > 0);
  assert(warm.path == essvi::CalibrationPath::fallback);
  checkPosteriorCovariances(warm);
  essvi::WarmOptions different_legacy_tolerance = options;
  different_legacy_tolerance.loss_tol = 1e6;
  const auto same_gate = essvi::calibrateWarm(
      current_quotes, &covariance_prior, previous_quotes,
      different_legacy_tolerance);
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
  assert(warm.fallback_reason_counts.infeasible_local_result > 0);
  assert(warm.slices_refit == 1);
  assert(warm.slices_reused == 0);
}

void rejectedInfeasiblePriorIsCountedAndRefit() {
  const auto previous_quotes = essvi::bench::realisticChain(1, 100, 3);
  auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  previous.surface.slices.front().psi = 4.0;
  assert(!essvi::validate(previous.surface).empty());

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  const auto warm = essvi::calibrateWarm(
      previous_quotes, &previous, previous_quotes, options);
  assert(warm.converged);
  assert(essvi::validate(warm.surface).empty());
  assert(warm.fallback_reason_counts.bound_check_failed > 0);
}

void posteriorModeKeepsStrictCalendarBoundaryFeasible() {
  const auto previous_quotes = essvi::bench::realisticChain(2, 100, 4);
  auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto& shorter = previous.surface.slices[0];
  auto& longer = previous.surface.slices[1];
  const double wing_ratio =
      std::max((1.0 + shorter.rho) / (1.0 + longer.rho),
               (1.0 - shorter.rho) / (1.0 - longer.rho));
  const double strict_floor = shorter.psi * wing_ratio + 1e-8;
  const double headroom_floor = shorter.psi * wing_ratio * 1.005 + 1e-8;
  assert(longer.psi > headroom_floor);
  longer.psi = 0.5 * (strict_floor + headroom_floor);
  assert(essvi::validate(previous.surface).empty());

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 100.0;
  const auto warm = essvi::calibrateWarm(
      previous_quotes, &previous, previous_quotes, options);
  assert(warm.converged);
  assert(warm.surface.slices.size() == previous.surface.slices.size());
  assert(warm.surface.slices[1].psi > strict_floor);
}

void failedCalendarRepairKeepsPosteriorFeasible() {
  const auto previous_quotes = essvi::bench::realisticChain(3, 100, 1);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  const auto& old_front = previous.surface.slices[0];
  const auto& old_next = previous.surface.slices[1];
  const double wing_ratio =
      std::max((1.0 + old_front.rho) / (1.0 + old_next.rho),
               (1.0 - old_front.rho) / (1.0 - old_next.rho));
  const double strict_limit = old_next.psi / wing_ratio;
  const double headroom_limit = old_next.psi / (wing_ratio * 1.005);
  const essvi::Slice new_front{old_front.expiry, old_front.theta,
                               0.5 * (strict_limit + headroom_limit),
                               old_front.rho};
  auto target_surface = previous.surface;
  target_surface.slices.front() = new_front;
  assert(essvi::validate(target_surface).empty());
  for (auto& quote : current_quotes) {
    if (quote.expiry == old_front.expiry) {
      const double k = std::log(quote.strike / quote.forward);
      const double iv = std::sqrt(
          essvi::sliceTotalVariance(new_front, k) / quote.expiry);
      quote.bid_iv = std::max(0.001, iv - 1e-5);
      quote.ask_iv = iv + 1e-5;
      quote.mid_iv = iv;
    } else if (quote.expiry == old_next.expiry) {
      quote.mid_iv = std::nan("");
    }
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 1.0;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  assert(warm.converged);
  assert(essvi::validate(warm.surface).empty());
  assert(warm.fallback_reason_counts.calendar_repair_failed == 1);
  assert(warm.surface.slices.size() == previous.surface.slices.size());
  assert(warm.surface.slices[1].theta == old_next.theta);
  assert(warm.surface.slices[1].psi == old_next.psi);
  assert(warm.surface.slices[1].rho == old_next.rho);
  assert(essvi::calendarSafe(warm.surface.slices[0],
                            warm.surface.slices[1]));
  checkDenseGrid(warm.surface);
}
void localSolverUsesThreeBoundedStarts() {
  const auto previous_quotes = essvi::bench::realisticChain(1, 100, 7);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  auto target = previous.surface.slices.front();
  target.rho = std::clamp(target.rho + 0.03, -0.95, 0.95);
  std::size_t quote_index = 0;
  for (auto& quote : current_quotes) {
    const double k = std::log(quote.strike / quote.forward);
    const double iv = std::sqrt(
        essvi::sliceTotalVariance(target, k) / quote.expiry);
    const double noise = quote_index++ % 2 == 0 ? 5e-5 : -5e-5;
    quote.bid_iv = std::max(0.001, iv + noise - 2e-5);
    quote.ask_iv = iv + noise + 2e-5;
    quote.mid_iv = iv + noise;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 5.0;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  const auto repeated = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  assert(warm.converged && repeated.converged);
  assert(warm.local_solver_slices == 1);
  assert(warm.local_solver_starts == 3);
  assert(warm.local_objective_evaluations > 0);
  assert(warm.local_objective_evaluations <= 25);
  assert(warm.local_objective_evaluations ==
         repeated.local_objective_evaluations);
  assert(warm.local_solver_starts == repeated.local_solver_starts);
  sameParameters(warm.surface, repeated.surface);
  checkDenseGrid(warm.surface);
}

void posteriorModePreservesQuoteCapBoundary() {
  const auto previous_quotes = essvi::bench::realisticChain(1, 100, 37);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  const auto& prior = previous.surface.slices.front();
  const essvi::Slice target{prior.expiry, prior.theta + 0.002, prior.psi,
                            prior.rho};
  assert(essvi::validate(essvi::Surface{{target}}).empty());

  constexpr double quote_cap = 0.1137;
  constexpr double cap_boundary = 0.1;
  // Ten shifted narrow bands compete with ninety prior-centered broad bands;
  // the raw-loss minimum misses this per-quote cap by less than one basis point.
  constexpr double narrow_half_width_fraction =
      (1.0 - 0.3) / (1.0 + 2.0 * cap_boundary);
  constexpr double broad_half_width_fraction = 0.28;
  auto current_quotes = previous_quotes;
  for (std::size_t i = 0; i < current_quotes.size(); ++i) {
    auto& quote = current_quotes[i];
    const double k = std::log(quote.strike / quote.forward);
    const bool narrow = i == 49 || (i % 10 == 0 && i / 10 < 9);
    const double center_variance = essvi::sliceTotalVariance(
        narrow ? target : prior, k);
    const double target_variance = essvi::sliceTotalVariance(target, k);
    const double prior_variance = essvi::sliceTotalVariance(prior, k);
    const double delta = std::abs(target_variance - prior_variance);
    const double half_width_fraction =
        narrow ? narrow_half_width_fraction : broad_half_width_fraction;
    const double center_iv = std::sqrt(center_variance / quote.expiry);
    const double half_spread =
        2.0 * delta * half_width_fraction /
        (4.0 * quote.expiry * center_iv);
    quote.bid_iv = center_iv - half_spread;
    quote.ask_iv = center_iv + half_spread;
    quote.mid_iv = center_iv;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = quote_cap;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  auto relaxed_options = options;
  relaxed_options.quote_cap = 0.114;
  const auto relaxed = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, relaxed_options);
  assert(warm.converged && relaxed.converged);
  assert(warm.local_solver_slices == 1 && relaxed.local_solver_slices == 1);
  assert(warm.fallback_slices == 0 && relaxed.fallback_slices == 0);
  assert(warm.quality_maximum_excess_ratio <= quote_cap + 1e-10);
  assert(relaxed.quality_maximum_excess_ratio <=
         relaxed_options.quote_cap + 1e-10);
  checkDenseGrid(warm.surface);
  checkDenseGrid(relaxed.surface);
}

void localSolverTraversesFallbackRhoRangeForNormalTilt() {
  constexpr std::size_t expiry_count = 40;
  constexpr std::size_t quotes_per_expiry = 100;
  const auto previous_quotes = essvi::bench::realisticChain(
      expiry_count, quotes_per_expiry, 1);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  essvi::bench::Noise noise(2);
  essvi::bench::evolveWarmScenario(
      current_quotes, expiry_count, quotes_per_expiry,
      essvi::bench::WarmScenario::normal, 1, noise);
  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  assert(warm.converged);
  assert(warm.restricted_cold_searches < 3 * expiry_count / 4);
  assert(warm.quality_maximum_excess_ratio <= options.quote_cap);
  assert(warm.quality_rms_excess_ratio <= 0.02);
  checkDenseGrid(warm.surface);
}

void localWarmRefitHonorsBudgetAndQuality() {
  auto previous_quotes = essvi::bench::realisticChain(3, 100, 1);
  // Wider symmetric bands keep the quality cap achievable after a local repair.
  for (auto& quote : previous_quotes) {
    const double k = std::log(quote.strike / quote.forward);
    const double half_spread = 0.006 * (1.0 + 2.0 * std::abs(k));
    quote.bid_iv = std::max(0.001, quote.mid_iv - half_spread);
    quote.ask_iv = quote.mid_iv + half_spread;
  }
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
    const double half_spread = 0.006 * (1.0 + 2.0 * std::abs(k));
    quote.bid_iv = std::max(0.001, mid - half_spread);
    quote.ask_iv = mid + half_spread;
    quote.mid_iv = mid;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 0.5;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  assert(warm.converged);
  double maximum_weighted_excess_ratio = 0.0;
  for (const auto& quote : current_quotes) {
    if (!quote.included) continue;
    const auto slice = std::find_if(
        warm.surface.slices.begin(), warm.surface.slices.end(),
        [&](const essvi::Slice& candidate) {
          return candidate.expiry == quote.expiry;
        });
    assert(slice != warm.surface.slices.end());
    const double variance = essvi::sliceTotalVariance(
        *slice, std::log(quote.strike / quote.forward));
    const double bid_variance = quote.expiry * quote.bid_iv * quote.bid_iv;
    const double ask_variance = quote.expiry * quote.ask_iv * quote.ask_iv;
    const double excess = std::max(
        0.0, std::max(bid_variance - variance, variance - ask_variance));
    const double width = std::max(ask_variance - bid_variance, 1e-6);
    maximum_weighted_excess_ratio =
        std::max(maximum_weighted_excess_ratio, quote.weight * excess / width);
  }
  assert(maximum_weighted_excess_ratio <= options.quote_cap + 1e-12);
  assert(warm.surface.slices.size() >= 2);
  const auto& shorter = warm.surface.slices[warm.surface.slices.size() - 2];
  const auto& longer = warm.surface.slices.back();
  const double wing_ratio =
      std::max((1.0 + shorter.rho) / (1.0 + longer.rho),
               (1.0 - shorter.rho) / (1.0 - longer.rho));
  assert(longer.psi >=
         shorter.psi * wing_ratio * 1.005 * (1.0 - 1e-9));
  assert(warm.slices_refit > 0 &&
         warm.slices_refit + warm.slices_reused ==
             previous.surface.slices.size());
  assert(warm.local_solver_slices == 1);
  assert(warm.local_solver_starts == 1);
  assert(warm.local_objective_evaluations == 2);
  assert(warm.local_objective_evaluations <= 25);
  assert(std::abs(warm.surface.slices.back().rho -
                  previous.surface.slices.back().rho) <= 0.05 + 1e-12);
  assert(warm.local_mm_bound_calls <= 11);
  checkDenseGrid(warm.surface);
}

void fastScenarioPreservesQuoteSpreadsAtIvFloor() {
  constexpr std::size_t expiry_count = 40;
  constexpr std::size_t quotes_per_expiry = 100;
  auto quotes = essvi::bench::realisticChain(
      expiry_count, quotes_per_expiry, 1);
  essvi::bench::Noise noise(2);
  for (std::size_t update = 1; update <= 200; ++update) {
    essvi::bench::evolveWarmScenario(
        quotes, expiry_count, quotes_per_expiry,
        essvi::bench::WarmScenario::fast, update, noise);
    for (const auto& quote : quotes) assert(quote.ask_iv > quote.bid_iv);
  }
}

void highFallbackShortcutUsesFastScenarioMoveScale() {
  constexpr std::size_t expiry_count = 10;
  constexpr std::size_t quotes_per_expiry = 100;
  const auto previous_quotes = essvi::bench::realisticChain(
      expiry_count, quotes_per_expiry, 11);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 0.0;
  const auto run = [&](essvi::bench::WarmScenario scenario) {
    auto current = previous_quotes;
    essvi::bench::Noise noise(12);
    essvi::bench::evolveWarmScenario(current, expiry_count,
                                     quotes_per_expiry, scenario, 1, noise);
    auto prior = previous;
    prior.fallback_slices = 6;
    return essvi::calibrateWarm(current, &prior, previous_quotes, options);
  };

  const auto normal = run(essvi::bench::WarmScenario::normal);
  const auto fast = run(essvi::bench::WarmScenario::fast);
  assert(normal.converged);
  assert(normal.local_solver_slices > 0);
  assert(fast.converged);
  assert(fast.local_solver_slices == 0);
  assert(fast.restricted_cold_searches > 0);
  checkDenseGrid(fast.surface);
}

void stagedFallbackBudgetReturnsValidDegradedResults() {
  const auto previous_quotes = essvi::bench::realisticChain(10, 100, 11);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current = previous_quotes;
  for (std::size_t i = 0; i < current.size(); ++i) {
    auto& quote = current[i];
    const double k = std::log(quote.strike / quote.forward);
    const double direction = i % 2 == 0 ? 1.0 : -1.0;
    quote.mid_iv = std::max(0.001, quote.mid_iv + direction * 0.01 *
                                                (1.0 + std::abs(k)));
    quote.bid_iv = quote.mid_iv;
    quote.ask_iv = quote.mid_iv;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 0.0;

  auto prior_result = previous;
  auto prior_quotes = previous_quotes;
  auto result = essvi::calibrateWarm(current, &prior_result, prior_quotes, options);
  assert(result.converged);
  assert(result.path == essvi::CalibrationPath::degraded);
  assert(result.full_cold_searches == 8);
  assert(result.forced_full_cold_searches == 0);
  assert(result.fallback_reason_counts.budget_exhausted > 0);
  assert(result.degraded_slices > 0);
  assert(result.degraded_ages.size() == result.surface.slices.size());
  assert(static_cast<std::size_t>(std::count_if(
             result.degraded_ages.begin(), result.degraded_ages.end(),
             [](std::size_t age) { return age != 0; })) ==
         result.degraded_slices);
  assert(result.full_cold_searches <=
         8 + result.forced_full_cold_searches);
  assert(result.quality_quote_count == current.size());
  assert(std::isfinite(result.quality_rms_excess_ratio));
  assert(std::isfinite(result.worst_slice_rms_excess_ratio));
  checkDenseGrid(result.surface);

  for (int update = 2; update <= 3; ++update) {
    prior_result = result;
    prior_quotes = current;
    result = essvi::calibrateWarm(current, &prior_result, prior_quotes, options);
    assert(result.converged);
    assert(result.path == essvi::CalibrationPath::degraded);
    assert(result.degraded_slices > 0);
    assert(static_cast<std::size_t>(std::count_if(
               result.degraded_ages.begin(), result.degraded_ages.end(),
               [](std::size_t age) { return age != 0; })) ==
           result.degraded_slices);
    assert(result.full_cold_searches <=
           8 + result.forced_full_cold_searches);
    checkDenseGrid(result.surface);
  }

  prior_result = result;
  prior_quotes = current;
  result = essvi::calibrateWarm(current, &prior_result, prior_quotes, options);
  assert(result.converged);
  assert(result.forced_full_cold_searches > 0);
  assert(result.full_cold_searches > 8);
  assert(result.path == essvi::CalibrationPath::fallback ||
         result.path == essvi::CalibrationPath::degraded);
  checkDenseGrid(result.surface);

  auto with_new_expiry = current;
  auto new_quote = with_new_expiry.front();
  new_quote.expiry = previous.surface.slices.back().expiry + 0.05;
  new_quote.id = "new-expiry";
  new_quote.strike = new_quote.forward;
  new_quote.bid_iv = new_quote.mid_iv;
  new_quote.ask_iv = new_quote.mid_iv;
  with_new_expiry.push_back(new_quote);
  const auto incomplete = essvi::calibrateWarm(
      with_new_expiry, &previous, previous_quotes, options);
  assert(!incomplete.converged);
  assert(!incomplete.error.empty());
  assert(incomplete.full_cold_searches == 8);
  assert(incomplete.path != essvi::CalibrationPath::degraded);
  assert(incomplete.fallback_reason_counts.budget_exhausted > 0);

  const auto reset_quotes = essvi::bench::syntheticChain(1, 100, 23);
  auto reset_prior = essvi::calibrate(reset_quotes);
  assert(reset_prior.converged);
  reset_prior.degraded_ages = {3};
  const auto reset = essvi::calibrateWarm(
      reset_quotes, &reset_prior, reset_quotes);
  assert(reset.converged);
  assert(reset.forced_full_cold_searches == 1);
  assert(reset.degraded_ages.size() == 1);
  assert(reset.degraded_ages.front() == 0);
}

using TestMatrix = std::array<std::array<double, 3>, 3>;
using TestCovariance = std::array<double, 6>;

TestMatrix invertTestMatrix(TestMatrix matrix) {
  TestMatrix inverse{{{1.0, 0.0, 0.0},
                      {0.0, 1.0, 0.0},
                      {0.0, 0.0, 1.0}}};
  for (int pivot = 0; pivot < 3; ++pivot) {
    const double divisor = matrix[pivot][pivot];
    assert(std::isfinite(divisor) && divisor != 0.0);
    for (int column = 0; column < 3; ++column) {
      matrix[pivot][column] /= divisor;
      inverse[pivot][column] /= divisor;
    }
    for (int row = 0; row < 3; ++row) {
      if (row == pivot) continue;
      const double factor = matrix[row][pivot];
      for (int column = 0; column < 3; ++column) {
        matrix[row][column] -= factor * matrix[pivot][column];
        inverse[row][column] -= factor * inverse[pivot][column];
      }
    }
  }
  return inverse;
}

TestCovariance independentlyRefreshedCovariance(
    TestCovariance predicted, const essvi::Slice& fitted,
    const std::vector<essvi::Quote>& quotes) {
  const std::array<double, 3> state{
      std::log(fitted.theta), std::atanh(fitted.rho), std::log(fitted.psi)};
  double loss_scale = 0.0;
  for (const auto& quote : quotes) {
    const double width = std::max(
        quote.expiry * (quote.ask_iv * quote.ask_iv - quote.bid_iv * quote.bid_iv),
        1e-6);
    loss_scale += quote.weight * quote.weight * width * width;
  }

  TestMatrix information{};
  for (const auto& quote : quotes) {
    const double k = std::log(quote.strike / quote.forward);
    const double variance = essvi::sliceTotalVariance(fitted, k);
    const double bid = quote.expiry * quote.bid_iv * quote.bid_iv;
    const double ask = quote.expiry * quote.ask_iv * quote.ask_iv;
    if (variance >= bid && variance <= ask) continue;
    std::array<double, 3> jacobian{};
    for (int coordinate = 0; coordinate < 3; ++coordinate) {
      auto upper = state;
      auto lower = state;
      upper[coordinate] += 1e-6;
      lower[coordinate] -= 1e-6;
      const auto sliceAt = [&](const std::array<double, 3>& x) {
        return essvi::Slice{fitted.expiry, std::exp(x[0]), std::exp(x[2]),
                            std::tanh(x[1])};
      };
      jacobian[coordinate] =
          (essvi::sliceTotalVariance(sliceAt(upper), k) -
           essvi::sliceTotalVariance(sliceAt(lower), k)) /
          2e-6;
    }
    const double scale = quote.weight * quote.weight / loss_scale;
    for (int row = 0; row < 3; ++row)
      for (int column = 0; column < 3; ++column)
        information[row][column] +=
            scale * jacobian[row] * jacobian[column];
  }

  TestMatrix covariance{{{predicted[0], predicted[1], predicted[2]},
                         {predicted[1], predicted[3], predicted[4]},
                         {predicted[2], predicted[4], predicted[5]}}};
  TestMatrix precision = invertTestMatrix(covariance);
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 3; ++column)
      precision[row][column] += information[row][column];
  TestMatrix refreshed = invertTestMatrix(precision);
  for (int i = 0; i < 3; ++i)
    refreshed[i][i] = std::max(refreshed[i][i], 1e-8);
  return {refreshed[0][0], refreshed[0][1], refreshed[0][2],
          refreshed[1][1], refreshed[1][2], refreshed[2][2]};
}

void assertCovarianceNear(const TestCovariance& actual,
                          const TestCovariance& expected) {
  for (std::size_t i = 0; i < actual.size(); ++i)
    assert(std::abs(actual[i] - expected[i]) <=
           1e-10 * (1.0 + std::abs(expected[i])));
}

void checkPosteriorCovariances(const essvi::CalibrationResult& result) {
  assert(result.posterior_covariances.size() == result.surface.slices.size());
  for (const auto& covariance : result.posterior_covariances) {
    const double a = covariance[0];
    const double b = covariance[1];
    const double c = covariance[2];
    const double d = covariance[3];
    const double e = covariance[4];
    const double f = covariance[5];
    assert(std::isfinite(a) && std::isfinite(b) && std::isfinite(c));
    assert(std::isfinite(d) && std::isfinite(e) && std::isfinite(f));
    assert(a >= 1e-8 && d >= 1e-8 && f >= 1e-8);
    assert(a * d - b * b > 0.0);
    assert(a * (d * f - e * e) - b * (b * f - c * e) +
               c * (b * e - c * d) > 0.0);
  }
}

double rawBandLossForSingleExpiry(
    const std::vector<essvi::Quote>& quotes, const essvi::Surface& surface) {
  const auto& slice = surface.slices.front();
  double loss = 0.0;
  for (const auto& quote : quotes) {
    const double k = std::log(quote.strike / quote.forward);
    const double variance = essvi::sliceTotalVariance(slice, k);
    const double bid = quote.expiry * quote.bid_iv * quote.bid_iv;
    const double ask = quote.expiry * quote.ask_iv * quote.ask_iv;
    const double residual = variance - std::clamp(variance, bid, ask);
    loss += quote.weight * quote.weight * residual * residual;
  }
  return loss;
}

void constrainedPosteriorModeUsesGaussianPriorAndIgnoresPreviousQuotes() {
  const auto previous_quotes = essvi::bench::realisticChain(1, 100, 1);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  const auto prior_slice = previous.surface.slices.front();
  const essvi::Slice target{prior_slice.expiry, prior_slice.theta * 1.15,
                            prior_slice.psi, prior_slice.rho};
  auto current_quotes = previous_quotes;
  for (auto& quote : current_quotes) {
    const double k = std::log(quote.strike / quote.forward);
    const double iv =
        std::sqrt(essvi::sliceTotalVariance(target, k) / quote.expiry);
    const double half_spread = 0.01 * (1.0 + 2.0 * std::abs(k));
    quote.bid_iv = std::max(0.001, iv - half_spread);
    quote.ask_iv = iv + half_spread;
    quote.mid_iv = iv;
  }
  auto changed_previous_quotes = previous_quotes;
  changed_previous_quotes.front().bid_iv += 0.02;

  auto covariance_prior = previous;
  covariance_prior.posterior_covariances.front() =
      {1e-5, 0.0, 0.0, 1e-5, 0.0, 1e-5};
  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 1e6;
  const auto concentrated = essvi::calibrateWarm(
      current_quotes, &covariance_prior, previous_quotes, options);
  covariance_prior.posterior_covariances.front() =
      {100.0, 0.0, 0.0, 100.0, 0.0, 100.0};
  const auto diffuse = essvi::calibrateWarm(
      current_quotes, &covariance_prior, previous_quotes, options);
  const auto changed_history = essvi::calibrateWarm(
      current_quotes, &covariance_prior, changed_previous_quotes, options);
  assert(concentrated.converged && diffuse.converged &&
         changed_history.converged);
  const double concentrated_move = std::abs(std::log(
      concentrated.surface.slices.front().theta / prior_slice.theta));
  const double diffuse_move = std::abs(std::log(
      diffuse.surface.slices.front().theta / prior_slice.theta));
  assert(diffuse_move > concentrated_move + 1e-4);
  auto scalar_quote = previous_quotes.front();
  scalar_quote.strike = scalar_quote.forward;
  const double scalar_bid_variance = prior_slice.theta * 1.125;
  const double scalar_ask_variance = prior_slice.theta * 1.375;
  scalar_quote.bid_iv =
      std::sqrt(scalar_bid_variance / scalar_quote.expiry);
  scalar_quote.ask_iv =
      std::sqrt(scalar_ask_variance / scalar_quote.expiry);
  scalar_quote.mid_iv =
      0.5 * (scalar_quote.bid_iv + scalar_quote.ask_iv);
  auto scalar_prior = previous;
  scalar_prior.posterior_covariances.front() =
      {1e-5, 0.0, 0.0, 1e-5, 0.0, 1e-5};
  const auto scalar_mode = essvi::calibrateWarm(
      {scalar_quote}, &scalar_prior, previous_quotes, options);
  assert(scalar_mode.converged);
  const double scalar_width = scalar_ask_variance - scalar_bid_variance;
  const double predicted_theta_variance = 1e-5 + 1e-2;
  const auto theta_score_derivative = [&](double theta) {
    return (theta - scalar_bid_variance) / (scalar_width * scalar_width) +
           std::log(theta / prior_slice.theta) /
               (predicted_theta_variance * theta);
  };
  double lower = prior_slice.theta, upper = scalar_bid_variance;
  for (int step = 0; step < 80; ++step) {
    const double midpoint = 0.5 * (lower + upper);
    if (theta_score_derivative(midpoint) < 0.0)
      lower = midpoint;
    else
      upper = midpoint;
  }
  const double scalar_mode_theta = scalar_mode.surface.slices.front().theta;
  assert(std::abs(std::log(scalar_mode_theta / (0.5 * (lower + upper)))) <
         2e-3);
  assert(std::abs(diffuse.objective -
                  rawBandLossForSingleExpiry(current_quotes, diffuse.surface)) <=
         1e-12 * (1.0 + diffuse.objective));
  sameParameters(diffuse.surface, changed_history.surface);
  assert(diffuse.objective == changed_history.objective);
  checkDenseGrid(diffuse.surface);

  auto fallback_majority_prior = previous;
  fallback_majority_prior.fallback_slices = 1;
  auto shifted_history_mids = previous_quotes;
  for (auto& quote : shifted_history_mids) quote.mid_iv += 0.2;
  auto history_options = options;
  history_options.quote_cap = 0.1;
  const auto original_history_result = essvi::calibrateWarm(
      current_quotes, &fallback_majority_prior, previous_quotes,
      history_options);
  const auto shifted_history_result = essvi::calibrateWarm(
      current_quotes, &fallback_majority_prior, shifted_history_mids,
      history_options);
  assert(original_history_result.converged && shifted_history_result.converged);
  assert(original_history_result.local_solver_slices > 0);
  assert(original_history_result.local_solver_slices ==
         shifted_history_result.local_solver_slices);
  sameParameters(original_history_result.surface,
                 shifted_history_result.surface);
  assert(original_history_result.objective == shifted_history_result.objective);
}

void posteriorModeEscapesLocalWarmRhoWindow() {
  const auto previous_quotes = essvi::bench::realisticChain(1, 100, 4);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  const auto prior_slice = previous.surface.slices.front();
  const double rho_direction = prior_slice.rho < 0.0 ? 1.0 : -1.0;
  const double target_rho = std::clamp(
      prior_slice.rho + 0.45 * rho_direction, -0.9, 0.9);
  assert(std::abs(target_rho - prior_slice.rho) > 0.3);
  const essvi::Slice target{prior_slice.expiry, prior_slice.theta,
                            prior_slice.psi, target_rho};
  assert(essvi::butterflySafe(target));

  auto current_quotes = previous_quotes;
  for (auto& quote : current_quotes) {
    const double k = std::log(quote.strike / quote.forward);
    const double iv = std::sqrt(
        essvi::sliceTotalVariance(target, k) / quote.expiry);
    const double half_spread = 5e-4 * (1.0 + std::abs(k));
    quote.bid_iv = std::max(0.001, iv - half_spread);
    quote.ask_iv = iv + half_spread;
    quote.mid_iv = iv;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 1e6;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);
  assert(warm.converged && warm.fallback_slices == 0);
  checkDenseGrid(warm.surface);

  const auto& fitted = warm.surface.slices.front();
  assert(std::abs(fitted.rho - prior_slice.rho) > 0.2);
  assert(std::abs(fitted.rho - target_rho) < 0.1);
}

void multivariatePosteriorMatchesIndependentScore() {
  const auto previous_quotes = essvi::bench::realisticChain(1, 100, 4);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  const auto prior_slice = previous.surface.slices.front();
  const essvi::Slice target{prior_slice.expiry, prior_slice.theta * 1.08,
                            prior_slice.psi * 0.96,
                            std::clamp(prior_slice.rho + 0.025, -0.9, 0.9)};
  assert(essvi::butterflySafe(target));

  auto prior = previous;
  prior.posterior_covariances.front() =
      {0.2, 0.3, -0.1, 1.0, 0.2, 1.0};
  std::vector<essvi::Quote> current_quotes;
  current_quotes.reserve(5);
  const std::array<double, 5> log_moneyness{-0.35, -0.16, 0.0, 0.18, 0.34};
  for (std::size_t index = 0; index < log_moneyness.size(); ++index) {
    auto quote = previous_quotes.front();
    quote.id = "posterior-score-" + std::to_string(index);
    quote.strike = quote.forward * std::exp(log_moneyness[index]);
    const double target_iv = std::sqrt(
        essvi::sliceTotalVariance(target, log_moneyness[index]) /
        quote.expiry);
    const double half_spread = 1e-3 * (1.0 + std::abs(log_moneyness[index]));
    const double quote_shift =
        index == 0 ? 0.004
                   : (index + 1 == log_moneyness.size() ? -0.004 : 0.0);
    quote.bid_iv = target_iv + quote_shift - half_spread;
    quote.ask_iv = target_iv + quote_shift + half_spread;
    quote.mid_iv = target_iv + quote_shift;
    current_quotes.push_back(quote);
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 1e6;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &prior, previous_quotes, options);
  assert(warm.converged);
  assert(warm.fallback_slices == 0);
  checkDenseGrid(warm.surface);

  std::array<double, 3> prior_mean{
      std::log(prior_slice.theta), std::atanh(prior_slice.rho),
      std::log(prior_slice.psi)};
  double covariance[3][3] = {{0.21, 0.3, -0.1},
                             {0.3, 6.0, 0.2},
                             {-0.1, 0.2, 6.0}};
  double lower[3][3]{};
  lower[0][0] = std::sqrt(covariance[0][0]);
  lower[1][0] = covariance[1][0] / lower[0][0];
  lower[2][0] = covariance[2][0] / lower[0][0];
  lower[1][1] =
      std::sqrt(covariance[1][1] - lower[1][0] * lower[1][0]);
  lower[2][1] = (covariance[2][1] - lower[2][0] * lower[1][0]) /
                lower[1][1];
  lower[2][2] = std::sqrt(covariance[2][2] -
                          lower[2][0] * lower[2][0] -
                          lower[2][1] * lower[2][1]);

  const auto independent_score = [&](const std::array<double, 3>& state) {
    const essvi::Slice slice{prior_slice.expiry, std::exp(state[0]),
                             std::exp(state[2]), std::tanh(state[1])};
    double loss = 0.0;
    double loss_scale = 0.0;
    for (const auto& quote : current_quotes) {
      const double k = std::log(quote.strike / quote.forward);
      const double variance = essvi::sliceTotalVariance(slice, k);
      const double bid = quote.expiry * quote.bid_iv * quote.bid_iv;
      const double ask = quote.expiry * quote.ask_iv * quote.ask_iv;
      const double residual = variance - std::clamp(variance, bid, ask);
      const double width = std::max(ask - bid, 1e-6);
      loss += quote.weight * quote.weight * residual * residual;
      loss_scale += quote.weight * quote.weight * width * width;
    }
    const std::array<double, 3> delta{state[0] - prior_mean[0],
                                      state[1] - prior_mean[1],
                                      state[2] - prior_mean[2]};
    double whitened[3]{};
    for (int row = 0; row < 3; ++row) {
      double value = delta[row];
      for (int column = 0; column < row; ++column)
        value -= lower[row][column] * whitened[column];
      whitened[row] = value / lower[row][row];
    }
    double prior_quadratic = 0.0;
    for (const double value : whitened) prior_quadratic += value * value;
    return 0.5 * (loss / loss_scale + prior_quadratic);
  };

  const auto& fitted = warm.surface.slices.front();
  const std::array<double, 3> fitted_state{
      std::log(fitted.theta), std::atanh(fitted.rho), std::log(fitted.psi)};
  std::size_t active_quotes = 0;
  for (const auto& quote : current_quotes) {
    const double variance = essvi::sliceTotalVariance(
        fitted, std::log(quote.strike / quote.forward));
    const double bid = quote.expiry * quote.bid_iv * quote.bid_iv;
    const double ask = quote.expiry * quote.ask_iv * quote.ask_iv;
    active_quotes += variance < bid || variance > ask;
  }
  for (std::size_t coordinate = 0; coordinate < fitted_state.size();
       ++coordinate) {
    auto upper = fitted_state;
    auto lower_state = fitted_state;
    upper[coordinate] += 1e-5;
    lower_state[coordinate] -= 1e-5;
    const double score_gradient =
        (independent_score(upper) - independent_score(lower_state)) / 2e-5;
    assert(std::abs(score_gradient) < 1e-3);
  }
  assert(active_quotes >= 2);
}

void covarianceStateIsAlignedAndInvalidStateColdStarts() {
  const auto previous_quotes = essvi::bench::realisticChain(2, 100, 6);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  checkPosteriorCovariances(previous);
  const auto& cold = previous;
  auto wide_quotes = previous_quotes;
  for (auto& quote : wide_quotes) {
    const auto slice = std::find_if(
        previous.surface.slices.begin(), previous.surface.slices.end(),
        [&](const essvi::Slice& candidate) { return candidate.expiry == quote.expiry; });
    assert(slice != previous.surface.slices.end());
    const double k = std::log(quote.strike / quote.forward);
    const double fitted_iv = std::sqrt(
        essvi::sliceTotalVariance(*slice, k) / quote.expiry);
    quote.bid_iv = std::max(0.001, fitted_iv - 0.2);
    quote.ask_iv = fitted_iv + 0.2;
    quote.mid_iv = fitted_iv;
  }
  const auto wide_cold = essvi::calibrate(wide_quotes);
  assert(wide_cold.converged);
  const std::array<double, 6> cold_diagonal{0.25, 0.0, 0.0, 0.25, 0.0, 0.25};
  for (const auto& covariance : wide_cold.posterior_covariances)
    assert(covariance == cold_diagonal);

  const auto warm = essvi::calibrateWarm(wide_quotes, &previous,
                                          previous_quotes);
  assert(warm.converged);
  checkPosteriorCovariances(warm);
  const auto add_process_noise = [](std::array<double, 6> covariance) {
    covariance[0] += 0.01;
    covariance[3] += 5.0;
    covariance[5] += 5.0;
    return covariance;
  };
  assert(warm.posterior_covariances.front() ==
         add_process_noise(previous.posterior_covariances.front()));
  const auto second_warm = essvi::calibrateWarm(wide_quotes, &warm, wide_quotes);
  assert(second_warm.converged);
  assert(second_warm.posterior_covariances.front() ==
         add_process_noise(warm.posterior_covariances.front()));
  auto malformed = previous;
  for (auto& covariance : malformed.posterior_covariances)
    covariance[0] = -1.0;
  const auto invalid = essvi::calibrateWarm(previous_quotes, &malformed,
                                             previous_quotes);
  assert(invalid.converged);
  assert(invalid.path == essvi::CalibrationPath::cold);
  assert(invalid.slices_reused == 0);
  assert(invalid.slices_refit == invalid.surface.slices.size());
  assert(invalid.surface.slices.front().theta ==
         cold.surface.slices.front().theta);
  assert(invalid.surface.slices.front().psi ==
         cold.surface.slices.front().psi);
  assert(invalid.surface.slices.front().rho ==
         cold.surface.slices.front().rho);
  assert(invalid.posterior_covariances == cold.posterior_covariances);
  checkPosteriorCovariances(invalid);

  auto missing = previous;
  missing.posterior_covariances.clear();
  const auto absent = essvi::calibrateWarm(previous_quotes, &missing,
                                            previous_quotes);
  assert(absent.converged);
  assert(absent.path == essvi::CalibrationPath::cold);
  assert(absent.slices_reused == 0);
  assert(absent.slices_refit == absent.surface.slices.size());
  assert(absent.surface.slices.front().theta ==
         cold.surface.slices.front().theta);
  assert(absent.surface.slices.front().psi ==
         cold.surface.slices.front().psi);
  assert(absent.surface.slices.front().rho ==
         cold.surface.slices.front().rho);
  assert(absent.posterior_covariances == cold.posterior_covariances);
  checkPosteriorCovariances(absent);

  auto mismatched_expiry = previous;
  assert(mismatched_expiry.posterior_covariances.front() ==
         cold.posterior_covariances.front());
  mismatched_expiry.surface.slices.front().expiry *= 0.5;
  const auto unmatched = essvi::calibrateWarm(
      previous_quotes, &mismatched_expiry, previous_quotes);
  assert(unmatched.converged);
  assert(unmatched.surface.slices.front().expiry ==
         previous.surface.slices.front().expiry);
  assert(unmatched.surface.slices.front().theta ==
         cold.surface.slices.front().theta);
  assert(unmatched.surface.slices.front().psi ==
         cold.surface.slices.front().psi);
  assert(unmatched.surface.slices.front().rho ==
         cold.surface.slices.front().rho);
  assert(unmatched.posterior_covariances.front() ==
         cold.posterior_covariances.front());
  checkPosteriorCovariances(unmatched);

  auto large_covariance_prior = previous;
  const std::array<double, 6> large_finite_diagonal{
      1e110, 0.0, 0.0, 1e110, 0.0, 1e110};
  for (auto& covariance : large_covariance_prior.posterior_covariances)
    covariance = large_finite_diagonal;
  auto shifted_quotes = previous_quotes;
  for (auto& quote : shifted_quotes) {
    quote.bid_iv += 0.015;
    quote.ask_iv += 0.015;
    quote.mid_iv += 0.015;
  }
  essvi::WarmOptions large_covariance_options;
  large_covariance_options.quote_tol = 0.0;
  large_covariance_options.quote_cap = 1e6;
  const auto large_covariance_warm = essvi::calibrateWarm(
      shifted_quotes, &large_covariance_prior, previous_quotes,
      large_covariance_options);
  assert(large_covariance_warm.converged);
  assert(std::abs(std::log(large_covariance_warm.surface.slices.front().theta /
                           previous.surface.slices.front().theta)) > 1e-4);
  assert(large_covariance_warm.posterior_covariances.front()[0] > 1e100);
  checkPosteriorCovariances(large_covariance_warm);

  auto invalid_large_prior = large_covariance_prior;
  for (auto& covariance : invalid_large_prior.posterior_covariances)
    covariance[0] = -1.0;
  const auto invalid_large_warm = essvi::calibrateWarm(
      shifted_quotes, &invalid_large_prior, previous_quotes,
      large_covariance_options);
  const auto shifted_cold = essvi::calibrate(shifted_quotes);
  assert(invalid_large_warm.converged && shifted_cold.converged);
  assert(invalid_large_warm.path == essvi::CalibrationPath::cold);
  assert(invalid_large_warm.posterior_covariances ==
         shifted_cold.posterior_covariances);
  assert(invalid_large_warm.surface.slices.size() ==
         shifted_cold.surface.slices.size());
  for (std::size_t i = 0; i < shifted_cold.surface.slices.size(); ++i) {
    const auto& slice = shifted_cold.surface.slices[i];
    const auto& quotes = shifted_quotes;
    std::vector<essvi::Quote> expiry_quotes;
    std::copy_if(quotes.begin(), quotes.end(),
                 std::back_inserter(expiry_quotes), [&](const auto& quote) {
                   return quote.expiry == slice.expiry;
                 });
    if (i == 0) continue;
    const TestCovariance cold_prediction{0.25, 0.0, 0.0, 0.25, 0.0, 0.25};
    assertCovarianceNear(
        shifted_cold.posterior_covariances[i],
        independentlyRefreshedCovariance(cold_prediction, slice,
                                         expiry_quotes));
  }
}

void activeQuoteInformationRefreshesColdAndWarmCovariance() {
  const auto previous_quotes = essvi::bench::realisticChain(1, 100, 4);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto prior = previous;
  prior.posterior_covariances.front() =
      {0.4, 0.0, 0.0, 0.6, 0.0, 0.8};
  auto current_quotes = previous_quotes;
  for (std::size_t index = 0; index < current_quotes.size(); ++index) {
    auto& quote = current_quotes[index];
    const double k = std::log(quote.strike / quote.forward);
    const double fitted_iv = std::sqrt(
        essvi::sliceTotalVariance(previous.surface.slices.front(), k) /
        quote.expiry);
    const double direction = index % 2 == 0 ? 1.0 : -1.0;
    const double target_iv =
        fitted_iv + direction * 0.004 * (1.0 + std::abs(k));
    quote.bid_iv = target_iv;
    quote.ask_iv = target_iv;
    quote.mid_iv = target_iv;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 1e6;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &prior, previous_quotes, options);
  assert(warm.converged);
  assert(warm.fallback_slices == 0);
  checkPosteriorCovariances(warm);
  const TestCovariance predicted_warm{0.41, 0.0, 0.0, 5.6, 0.0, 5.8};
  assertCovarianceNear(
      warm.posterior_covariances.front(),
      independentlyRefreshedCovariance(predicted_warm,
                                       warm.surface.slices.front(),
                                       current_quotes));

  auto fallback_options = options;
  fallback_options.quote_cap = 0.1;
  const auto fallback = essvi::calibrateWarm(
      current_quotes, &prior, previous_quotes, fallback_options);
  assert(fallback.converged);
  assert(fallback.fallback_slices > 0);
  checkPosteriorCovariances(fallback);
  assertCovarianceNear(
      fallback.posterior_covariances.front(),
      independentlyRefreshedCovariance(predicted_warm,
                                       fallback.surface.slices.front(),
                                       current_quotes));

  const auto cold = essvi::calibrate(current_quotes);
  assert(cold.converged);
  checkPosteriorCovariances(cold);
  const TestCovariance cold_prediction{0.25, 0.0, 0.0, 0.25, 0.0, 0.25};
  assertCovarianceNear(
      cold.posterior_covariances.front(),
      independentlyRefreshedCovariance(cold_prediction,
                                       cold.surface.slices.front(),
                                       current_quotes));
  checkDenseGrid(warm.surface);
  checkDenseGrid(cold.surface);
}

void unusableMatchedCovarianceForcesColdBeyondOrdinaryBudget() {
  const auto quotes = essvi::bench::realisticChain(10, 100, 11);
  const auto calibrated = essvi::calibrate(quotes);
  assert(calibrated.converged);

  auto unusable = calibrated;
  for (auto& covariance : unusable.posterior_covariances)
    covariance[0] = -1.0;
  const auto result = essvi::calibrateWarm(quotes, &unusable, quotes);
  assert(result.converged);
  assert(result.full_cold_searches == 10);
  assert(result.forced_full_cold_searches == 10);
  assert(result.slices_refit == 10);
  assert(result.slices_reused == 0);
  checkPosteriorCovariances(result);
  checkDenseGrid(result.surface);
}

}  // namespace

int main() {
  {
    const auto warmFitIgnoresMidOutsideBand = []() {
      const auto previous_quotes = essvi::bench::realisticChain(1, 100, 1);
      const auto previous = essvi::calibrate(previous_quotes);
      assert(previous.converged);
    
      auto current_quotes = previous_quotes;
      const auto& old_slice = previous.surface.slices.front();
      const essvi::Slice target{old_slice.expiry, old_slice.theta + 0.003,
                                old_slice.psi, old_slice.rho};
      for (std::size_t i = 0; i < current_quotes.size(); ++i) {
        auto& quote = current_quotes[i];
        const double k = std::log(quote.strike / quote.forward);
        const double mid = std::sqrt(
            essvi::sliceTotalVariance(target, k) / quote.expiry);
        const double half_spread = i == 49 ? 0.1 : 0.003;
        quote.bid_iv = std::max(0.001, mid - half_spread);
        quote.ask_iv = mid + half_spread;
        quote.mid_iv = mid;
      }
      auto shifted_mid_quotes = current_quotes;
      shifted_mid_quotes[49].mid_iv += 0.08;
    
      essvi::WarmOptions options;
      options.quote_tol = 0.0;
      options.quote_cap = 0.1;
      const auto baseline = essvi::calibrateWarm(
          current_quotes, &previous, previous_quotes, options);
      const auto shifted_mid = essvi::calibrateWarm(
          shifted_mid_quotes, &previous, previous_quotes, options);
      assert(baseline.converged && shifted_mid.converged);
      assert(baseline.local_solver_slices == 1);
      assert(shifted_mid.local_solver_slices == 1);
      assert(baseline.local_objective_evaluations > 0);
      assert(shifted_mid.local_objective_evaluations > 0);
      sameParameters(baseline.surface, shifted_mid.surface);
      checkDenseGrid(baseline.surface);
      checkDenseGrid(shifted_mid.surface);
    };
    warmFitIgnoresMidOutsideBand();
    warmUpdatesReuseAndRefitSafely();
  }
  changedQuotesReuseWhenPreviousSurfaceStillFits();
  perQuoteCapBoundaryAndWidthFloor();
  perQuoteGateChecksSingleOutlierAgainstColdReference();
  perQuoteGateFallsBackToColdSearch();
  sequentialCalendarRepairRetriesRejectedCandidates();
  failedJacobiPrefitFallsBackWithoutRetry();
  rejectedInfeasiblePriorIsCountedAndRefit();
  posteriorModeKeepsStrictCalendarBoundaryFeasible();
  failedCalendarRepairKeepsPosteriorFeasible();
  localWarmRefitHonorsBudgetAndQuality();
  localSolverUsesThreeBoundedStarts();
  posteriorModePreservesQuoteCapBoundary();
  localSolverTraversesFallbackRhoRangeForNormalTilt();
  fastScenarioPreservesQuoteSpreadsAtIvFloor();
  highFallbackShortcutUsesFastScenarioMoveScale();
  constrainedPosteriorModeUsesGaussianPriorAndIgnoresPreviousQuotes();
  posteriorModeEscapesLocalWarmRhoWindow();
  multivariatePosteriorMatchesIndependentScore();
  covarianceStateIsAlignedAndInvalidStateColdStarts();
  activeQuoteInformationRefreshesColdAndWarmCovariance();
  unusableMatchedCovarianceForcesColdBeyondOrdinaryBudget();
  stagedFallbackBudgetReturnsValidDegradedResults();
  return 0;
}
