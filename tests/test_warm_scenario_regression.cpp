#include "calibrator.hpp"
#include "quality_metrics.hpp"
#include "realistic_warm.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <iostream>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::size_t kExpiryCount = 40;
constexpr std::size_t kQuotesPerExpiry = 100;
constexpr std::size_t kUpdates = 50;
constexpr std::uint64_t kSeed = 1;
constexpr std::size_t kCheckCount = 15;

enum CheckIndex : std::size_t {
  unpaired_update,
  cap_share_delta,
  maximum_ratio,
  rms_ratio,
  summed_loss_ratio,
  rms_iv_delta,
  warm_failures,
  invalid_surfaces,
  dense_grid_violations,
  degraded_contract,
  cold_failures,
  warm_p99,
  fallback_updates,
  quiet_median_non_regression,
  quiet_p99_non_regression
};

struct Violation {
  bool present{};
  std::size_t update{};
  const char* metric{};
  double observed{};
  double hard_limit{};
};

using Violations = std::array<Violation, kCheckCount>;

void checkLimit(Violations& violations, CheckIndex check, std::size_t update,
                const char* metric, double value, double hard_limit) {
  if (value <= hard_limit) return;
  auto& violation = violations[static_cast<std::size_t>(check)];
  if (!violation.present)
    violation = {true, update, metric, value, hard_limit};
}

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

std::size_t denseGridViolationCount(const essvi::Surface& surface) {
  std::size_t violations = 0;
  std::array<double, 101> previous_variance{};
  bool has_previous = false;
  for (const auto& slice : surface.slices) {
    if (!essvi::butterflySafe(slice)) ++violations;
    std::array<double, 101> variance{};
    std::array<double, 101> call{};
    std::array<double, 101> strike{};
    for (std::size_t j = 0; j < variance.size(); ++j) {
      const double k = -0.5 + 0.01 * static_cast<double>(j);
      strike[j] = 100.0 * std::exp(k);
      variance[j] = essvi::sliceTotalVariance(slice, k);
      if (!(std::isfinite(variance[j]) && variance[j] > 0.0)) {
        ++violations;
        continue;
      }
      if (has_previous && variance[j] < previous_variance[j] - 1e-8)
        ++violations;
      call[j] = normalizedCall(100.0, strike[j], variance[j]);
    }
    for (std::size_t j = 1; j < 100; ++j) {
      const double left_slope = (call[j] - call[j - 1]) /
                                (strike[j] - strike[j - 1]);
      const double right_slope = (call[j + 1] - call[j]) /
                                 (strike[j + 1] - strike[j]);
      if (!(std::isfinite(left_slope) && std::isfinite(right_slope) &&
            right_slope >= left_slope - 1e-8))
        ++violations;
    }
    previous_variance = variance;
    has_previous = true;
  }
  return violations;
}

double lossRatio(double warm_loss, double cold_loss) {
  if (cold_loss == 0.0)
    return warm_loss == 0.0 ? 1.0 : std::numeric_limits<double>::infinity();
  return warm_loss / cold_loss;
}

double percentile(const std::vector<std::pair<double, std::size_t>>& samples,
                  double p, std::size_t& update) {
  auto ordered = samples;
  std::sort(ordered.begin(), ordered.end());
  const double index = p * static_cast<double>(ordered.size() - 1);
  const auto low = static_cast<std::size_t>(index);
  const auto high = std::min(low + 1, ordered.size() - 1);
  const double fraction = index - static_cast<double>(low);
  update = ordered[high].second;
  return ordered[low].first + fraction * (ordered[high].first - ordered[low].first);
}

bool runScenario(essvi::bench::WarmScenario scenario) {
  const char* name = essvi::bench::warmScenarioName(scenario);
  Violations violations{};
  std::vector<std::pair<double, std::size_t>> timed_updates;
  timed_updates.reserve(kUpdates);
  std::size_t fallback_update_count = 0;
  essvi::bench::BenchmarkQuality paired_quality{};

  auto previous_quotes = essvi::bench::realisticChain(
      kExpiryCount, kQuotesPerExpiry, kSeed);
  auto previous_result = essvi::calibrate(previous_quotes);
  if (!previous_result.converged) {
    checkLimit(violations, cold_failures, 0, "initial cold calibration failure",
               1.0, 0.0);
    std::cerr << "scenario=" << name << " seed=" << kSeed
              << " update=0 metric=\"initial cold calibration failure\""
              << " observed=1 hard_limit=0\n";
    std::cout << "scenario=" << name << " seed=" << kSeed
              << " updates=0 hard_limit_status=FAIL\n";
    return false;
  }

  essvi::bench::Noise noise(kSeed + 1);
  essvi::WarmOptions options;
  options.quote_cap = 0.1;
  for (std::size_t update = 1; update <= kUpdates; ++update) {
    auto current_quotes = previous_quotes;
    essvi::bench::evolveWarmScenario(
        current_quotes, kExpiryCount, kQuotesPerExpiry, scenario, update, noise);

    const auto start = Clock::now();
    const auto warm = essvi::calibrateWarm(
        current_quotes, &previous_result, previous_quotes, options);
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    timed_updates.emplace_back(elapsed * 1000.0, update);
    if (warm.path == essvi::CalibrationPath::fallback ||
        warm.path == essvi::CalibrationPath::degraded)
      ++fallback_update_count;

    checkLimit(violations, warm_failures, update, "warm failures",
               warm.converged ? 0.0 : 1.0, 0.0);
    if (warm.converged &&
        (warm.surface.slices.empty() || !essvi::validate(warm.surface).empty()))
      checkLimit(violations, invalid_surfaces, update, "invalid warm surface",
                 1.0, 0.0);
    if (warm.converged && !warm.surface.slices.empty())
      checkLimit(violations, dense_grid_violations, update,
                 "dense-grid butterfly/calendar violations",
                 static_cast<double>(denseGridViolationCount(warm.surface)), 0.0);

    const bool within_cold_budget =
        warm.full_cold_searches <= 8 + warm.forced_full_cold_searches;
    if (!within_cold_budget)
      checkLimit(violations, degraded_contract, update,
                 "full-cold search budget", 1.0, 0.0);
    if (warm.path == essvi::CalibrationPath::degraded) {
      const auto aged_slices = static_cast<std::size_t>(std::count_if(
          warm.degraded_ages.begin(), warm.degraded_ages.end(),
          [](std::size_t age) { return age != 0; }));
      const bool state_matches =
          warm.converged && warm.degraded_ages.size() == warm.surface.slices.size() &&
          warm.degraded_slices == aged_slices && warm.degraded_slices > 0 &&
          within_cold_budget;
      checkLimit(violations, degraded_contract, update,
                 "degraded result state/count/budget contract",
                 state_matches ? 0.0 : 1.0, 0.0);
    }

    const auto cold = essvi::calibrate(current_quotes);
    checkLimit(violations, cold_failures, update, "cold reference failure",
               cold.converged ? 0.0 : 1.0, 0.0);
    if (warm.converged && cold.converged &&
        !warm.surface.slices.empty() && !cold.surface.slices.empty()) {
      const auto warm_quality = essvi::bench::measureExcessMetrics(
          current_quotes, warm.surface, 0.1);
      const auto cold_quality = essvi::bench::measureExcessMetrics(
          current_quotes, cold.surface, 0.1);
      const auto expected_quotes =
          essvi::bench::countValidTwoSidedBands(current_quotes);
      if (!essvi::bench::mergePairedQuoteMetrics(
              paired_quality, warm_quality, cold_quality, expected_quotes))
        checkLimit(violations, unpaired_update, update,
                   "unpaired or unscored update", 1.0, 0.0);
    } else {
      checkLimit(violations, unpaired_update, update,
                 "unpaired or unscored update", 1.0, 0.0);
    }

    if (warm.converged)
      previous_result = warm;
    else if (cold.converged)
      previous_result = cold;
    else
      previous_result = {};
    previous_quotes = std::move(current_quotes);
  }

  const double unpaired_updates =
      static_cast<double>(kUpdates - paired_quality.paired_updates);
  checkLimit(violations, unpaired_update, kUpdates,
             "unpaired or unscored updates", unpaired_updates, 0.0);
  // Addendum B quality statistics aggregate the paired quote observations
  // across the complete scenario, matching the benchmark's report semantics.
  if (essvi::bench::hasPairedQuoteObservations(paired_quality.warm,
                                               paired_quality.cold)) {
    const auto& warm = paired_quality.warm;
    const auto& cold = paired_quality.cold;
    checkLimit(violations, cap_share_delta, kUpdates,
               "share above cap warm minus cold (percentage points)",
               100.0 * (warm.share_above_cap() - cold.share_above_cap()), 2.0);
    checkLimit(violations, maximum_ratio, kUpdates,
               "maximum excess ratio, warm", warm.maximum_ratio,
               std::max(0.2, cold.maximum_ratio + 0.05));
    checkLimit(violations, rms_ratio, kUpdates, "RMS excess ratio, warm",
               warm.rms_ratio(), 0.05);
    checkLimit(violations, summed_loss_ratio, kUpdates,
               "summed band loss warm / cold",
               lossRatio(warm.summed_band_loss, cold.summed_band_loss), 1.20);
    checkLimit(violations, rms_iv_delta, kUpdates,
               "RMS excess IV points warm minus cold",
               warm.rms_excess_vol_points() - cold.rms_excess_vol_points(),
               0.05);
  } else {
    const double unavailable = std::numeric_limits<double>::quiet_NaN();
    checkLimit(violations, cap_share_delta, kUpdates,
               "share above cap warm minus cold (percentage points)",
               unavailable, 2.0);
    checkLimit(violations, maximum_ratio, kUpdates,
               "maximum excess ratio, warm", unavailable, 0.2);
    checkLimit(violations, rms_ratio, kUpdates, "RMS excess ratio, warm",
               unavailable, 0.05);
    checkLimit(violations, summed_loss_ratio, kUpdates,
               "summed band loss warm / cold", unavailable, 1.20);
    checkLimit(violations, rms_iv_delta, kUpdates,
               "RMS excess IV points warm minus cold", unavailable, 0.05);
  }

  const double fallback_limit = scenario == essvi::bench::WarmScenario::fast
                                    ? 25.0
                                    : 5.0;
  checkLimit(violations, fallback_updates, kUpdates,
             "fallback updates (percent)",
             100.0 * static_cast<double>(fallback_update_count) /
                 static_cast<double>(kUpdates),
             fallback_limit);
  std::size_t p99_update = 0;
  const double p99_ms = percentile(timed_updates, 0.99, p99_update);
  const double p99_limit = scenario == essvi::bench::WarmScenario::fast
                               ? 20.0
                               : 10.0;
  checkLimit(violations, warm_p99, p99_update, "warm p99 (milliseconds)",
             p99_ms, p99_limit);
  if (scenario == essvi::bench::WarmScenario::quiet) {
    std::size_t median_update = 0;
    const double median_ms = percentile(timed_updates, 0.50, median_update);
    checkLimit(violations, quiet_median_non_regression, median_update,
               "quiet median non-regression vs 49fed2b (milliseconds)",
               median_ms, 0.47);
    checkLimit(violations, quiet_p99_non_regression, p99_update,
               "quiet p99 non-regression vs 49fed2b (milliseconds)",
               p99_ms, 7.6);
  }

  bool passed = true;
  for (const auto& violation : violations) {
    if (!violation.present) continue;
    passed = false;
    std::cerr << std::setprecision(17)
              << "scenario=" << name << " seed=" << kSeed
              << " update=" << violation.update
              << " metric=\"" << violation.metric << "\" observed="
              << violation.observed << " hard_limit=" << violation.hard_limit
              << '\n';
  }
  std::cout << "scenario=" << name << " seed=" << kSeed
            << " updates=" << kUpdates
            << " fallback_updates=" << fallback_update_count
            << " hard_limit_status=" << (passed ? "PASS" : "FAIL") << '\n';
  return passed;
}

}  // namespace

int main() {
  const std::array scenarios{
      essvi::bench::WarmScenario::quiet,
      essvi::bench::WarmScenario::normal,
      essvi::bench::WarmScenario::fast,
      essvi::bench::WarmScenario::front};
  bool passed = true;
  for (const auto scenario : scenarios) passed = runScenario(scenario) && passed;
  return passed ? 0 : 1;
}
