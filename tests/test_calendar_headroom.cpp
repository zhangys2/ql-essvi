#include "calibrator.hpp"
#include "realistic_warm.hpp"

#undef NDEBUG
#include <algorithm>
#include <array>
#include <cmath>
#include <cassert>
#include <string>

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
  for (const auto& slice : surface.slices) {
    assert(essvi::butterflySafe(slice));
    std::array<double, 101> variance{};
    std::array<double, 101> call{};
    std::array<double, 101> strike{};
    for (int j = 0; j <= 100; ++j) {
      const double k = -0.5 + 0.01 * j;
      const auto index = static_cast<std::size_t>(j);
      strike[index] = 100.0 * std::exp(k);
      variance[index] = essvi::sliceTotalVariance(slice, k);
      assert(variance[index] > 0.0);
      if (has_previous)
        assert(variance[index] >= previous_variance[index] - 1e-8);
      call[index] = normalizedCall(100.0, strike[index], variance[index]);
    }
    for (int j = 1; j < 100; ++j) {
      const auto index = static_cast<std::size_t>(j);
      const double left_slope =
          (call[index] - call[index - 1]) / (strike[index] - strike[index - 1]);
      const double right_slope =
          (call[index + 1] - call[index]) / (strike[index + 1] - strike[index]);
      assert(right_slope >= left_slope - 1e-8);
    }
    previous_variance = variance;
    has_previous = true;
  }
}

void deribitFixturePassesDenseGrid() {
  const auto quotes = essvi::readQuotes(
      std::string(ESSVI_SOURCE_DIR) +
      "/tests/fixtures/deribit_btc_20261018T1200_quotes.csv");
  const auto result = essvi::calibrate(quotes);
  assert(result.converged);
  checkDenseGrid(result.surface);
}

void fastScenarioStaysCalibrable() {
  constexpr std::size_t expiry_count = 40;
  constexpr std::size_t quotes_per_expiry = 100;
  auto previous_quotes = essvi::bench::realisticChain(
      expiry_count, quotes_per_expiry, 1);
  auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  essvi::bench::Noise noise(2);
  essvi::WarmOptions options;
  options.quote_cap = 0.1;
  for (std::size_t update = 1; update <= 32; ++update) {
    auto current_quotes = previous_quotes;
    essvi::bench::evolveWarmScenario(
        current_quotes, expiry_count, quotes_per_expiry,
        essvi::bench::WarmScenario::fast, update, noise);
    const auto warm = essvi::calibrateWarm(
        current_quotes, &previous, previous_quotes, options);
    const auto cold = essvi::calibrate(current_quotes);
    assert(warm.converged);
    assert(cold.converged);
    if (update == 18 || update == 27 || update == 31) {
      assert(essvi::validate(warm.surface).empty());
      bool strict_only_pair = false;
      for (std::size_t i = 1; i < warm.surface.slices.size(); ++i) {
        const auto& shorter = warm.surface.slices[i - 1];
        const auto& longer = warm.surface.slices[i];
        const double wing_ratio =
            std::max((1.0 + shorter.rho) / (1.0 + longer.rho),
                     (1.0 - shorter.rho) / (1.0 - longer.rho));
        const double strict_floor = shorter.psi * wing_ratio + 1e-8;
        const double headroom_floor =
            shorter.psi * wing_ratio * 1.005 + 1e-8;
        strict_only_pair = strict_only_pair ||
                           (longer.psi > strict_floor &&
                            longer.psi <= headroom_floor);
      }
      assert(strict_only_pair);
    }
    previous = warm;
    previous_quotes = std::move(current_quotes);
  }
}
}  // namespace

int main() {
  deribitFixturePassesDenseGrid();
  fastScenarioStaysCalibrable();
}
