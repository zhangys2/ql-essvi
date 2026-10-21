#include "calibrator.hpp"
#include "realistic_warm.hpp"

#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <string>

namespace {
void adjacentSlicesKeepHeadroom() {
  const auto quotes = essvi::readQuotes(
      std::string(ESSVI_SOURCE_DIR) +
      "/tests/fixtures/deribit_btc_20261018T1200_quotes.csv");
  const auto result = essvi::calibrate(quotes);
  assert(result.converged);
  assert(essvi::validate(result.surface).empty());

  const auto& slices = result.surface.slices;
  assert(slices.size() > 1);
  constexpr double headroom = 0.005;
  for (std::size_t i = 1; i < slices.size(); ++i) {
    const auto& shorter = slices[i - 1];
    const auto& longer = slices[i];
    const double wing_ratio =
        std::max((1.0 + shorter.rho) / (1.0 + longer.rho),
                 (1.0 - shorter.rho) / (1.0 - longer.rho));
    assert(longer.psi >= shorter.psi * wing_ratio * (1.0 + headroom) * (1.0 - 1e-9));
  }
}

// Seed 1, 40 x 100: updates 18, 27 and 31 once left no room for the headroom
// (adjacent total variances within 0.6% at the long end) and failed with "empty
// feasible set".
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
    previous = warm;
    previous_quotes = std::move(current_quotes);
  }
}
}  // namespace

int main() {
  adjacentSlicesKeepHeadroom();
  fastScenarioStaysCalibrable();
}
