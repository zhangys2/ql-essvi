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
  options.cold_check_interval = 10;
  auto previous_surface = cold.surface;
  for (std::size_t update = 1; update <= 100; ++update) {
    auto current_quotes = previous_quotes;
    for (auto& quote : current_quotes) {
      quote.bid_iv += 1e-7;
      quote.ask_iv += 1e-7;
    }
    options.update_number = update;
    const auto warm = essvi::calibrateWarm(current_quotes, previous_surface,
                                           previous_quotes, options);
    assert(warm.converged);
    assert(warm.path == essvi::CalibrationPath::warm);
    assert(warm.slices_reused == 40);
    assert(warm.slices_refit == 0);
    assert(warm.objective <= cold.objective + 1e-12);
    assert(warm.objective_evaluations >= static_cast<std::uint64_t>(warm.iterations));
    assert(std::isfinite(warm.runtime_seconds));
    assert(warm.mm_bound_calls > 0);
    checkDenseGrid(warm.surface);
    previous_surface = warm.surface;
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
  options.cold_check_interval = 0;
  options.update_number = 101;
  const auto warm_refit = essvi::calibrateWarm(changed_quotes, previous_surface,
                                               previous_quotes, options);
  const auto cold_refit = essvi::calibrate(changed_quotes);
  assert(warm_refit.converged && cold_refit.converged);
  assert(warm_refit.slices_refit > 0);
  assert(warm_refit.slices_reused + warm_refit.slices_refit == 40);
  assert(warm_refit.objective <= cold_refit.objective * (1.0 + 1e-3) + 1e-12);
  assert(warm_refit.objective <= cold_refit.objective * 1.05 + 1e-12);
  checkDenseGrid(warm_refit.surface);

  const auto repeated_a = essvi::calibrateWarm(changed_quotes, previous_surface,
                                               previous_quotes, options);
  const auto repeated_b = essvi::calibrateWarm(changed_quotes, previous_surface,
                                               previous_quotes, options);
  assert(repeated_a.converged && repeated_b.converged);
  sameParameters(repeated_a.surface, repeated_b.surface);

  const auto failed = essvi::calibrateWarm({}, previous_surface,
                                            previous_quotes, options);
  assert(!failed.converged);
  assert(!failed.error.empty());
  assert(failed.surface.slices.empty());

  auto no_atm = std::vector<essvi::Quote>{
      {"bad-atm", 0.5, 100.0, 1.0, 100.0, 0.2, 0.21, NAN, true}};
  const auto infeasible = essvi::calibrateWarm(no_atm, previous_surface,
                                                previous_quotes, options);
  assert(!infeasible.converged);
  assert(!infeasible.error.empty());
  assert(infeasible.surface.slices.empty());
}

void changedQuotesReuseWhenPreviousSurfaceStillFits() {
  const auto previous_quotes = essvi::bench::syntheticChain(3, 100);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  auto current = previous_quotes;
  for (auto& quote : current) {
    if (quote.expiry != previous.surface.slices.back().expiry) continue;
    quote.bid_iv -= 1e-5;
    quote.ask_iv += 1e-5;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.cold_check_interval = 0;
  const auto warm = essvi::calibrateWarm(
      current, previous.surface, previous_quotes, options);
  assert(warm.converged);
  assert(warm.slices_reused == 3);
  assert(warm.slices_refit == 0);
  assert(warm.quality_checks == 0);
  checkDenseGrid(warm.surface);
}

void qualityGateUsesBandWidthScale() {
  const auto previous_quotes = essvi::bench::realisticChain(3, 100, 7);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  const double changed_expiry = previous.surface.slices.front().expiry;
  for (auto& quote : current_quotes) {
    if (quote.expiry != changed_expiry) continue;
    const double k = std::log(quote.strike / quote.forward);
    const double shift = 1.05 * 0.003 * (1.0 + 2.0 * std::abs(k));
    quote.bid_iv += shift;
    quote.ask_iv += shift;
    quote.mid_iv += shift;
  }

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.cold_check_interval = 1;
  options.update_number = 1;
  const auto warm = essvi::calibrateWarm(
      current_quotes, previous.surface, previous_quotes, options);
  const auto cold = essvi::calibrate(current_quotes);
  assert(warm.converged && cold.converged);
  assert(warm.quality_checks > 0);
  assert(warm.quality_loss_scale > 0.0);
  assert(warm.quality_warm_loss <= warm.quality_reference_loss +
                                       options.loss_tol *
                                           warm.quality_loss_scale);
  assert(warm.fallback_slices == 0);
  assert(warm.path == essvi::CalibrationPath::warm);
  assert(warm.objective <= cold.objective +
                              options.loss_tol * warm.quality_loss_scale);
  checkDenseGrid(warm.surface);
}

void qualityGateFallsBackToColdSearch() {
  constexpr std::uint64_t seed = 2;
  const auto previous_quotes = essvi::bench::realisticChain(3, 100, seed);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current = previous_quotes;
  essvi::bench::Noise noise(seed + 1);
  essvi::bench::evolveWarmScenario(
      current, 3, 100, essvi::bench::WarmScenario::fast, 1, noise);
  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.loss_tol = 0.05;
  options.cold_check_interval = 0;
  options.update_number = 1;
  const auto warm = essvi::calibrateWarm(
      current, previous.surface, previous_quotes, options);
  const auto cold = essvi::calibrate(current);
  assert(warm.converged && cold.converged);
  assert(warm.path == essvi::CalibrationPath::fallback);
  assert(warm.fallback_slices > 0);
  assert(warm.quality_checks > 0);
  assert(warm.quality_loss_scale > 0.0);
  assert(warm.quality_warm_loss > warm.quality_reference_loss +
                                     options.loss_tol *
                                         warm.quality_loss_scale);
  assert(warm.loss_tolerance == options.loss_tol);
  assert(warm.local_solver_slices >= 3);

  const auto repeated = essvi::calibrateWarm(
      current, previous.surface, previous_quotes, options);
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
  options.cold_check_interval = 0;
  const auto warm = essvi::calibrateWarm(
      current_quotes, previous.surface, previous_quotes, options);
  const auto repeated = essvi::calibrateWarm(
      current_quotes, previous.surface, previous_quotes, options);
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
  options.cold_check_interval = 0;
  const auto warm = essvi::calibrateWarm(
      current_quotes, previous.surface, previous_quotes, options);
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
  options.cold_check_interval = 0;
  const auto warm = essvi::calibrateWarm(
      current_quotes, previous.surface, previous_quotes, options);
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
  qualityGateUsesBandWidthScale();
  qualityGateFallsBackToColdSearch();
  sequentialCalendarRepairRetriesRejectedCandidates();
  failedJacobiPrefitFallsBackWithoutRetry();
  localWarmRefitHonorsBudgetAndQuality();
  return 0;
}
