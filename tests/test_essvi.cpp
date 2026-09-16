#include "calibrator.hpp"
#include "essvi_surface.hpp"

#undef NDEBUG  // Release builds define NDEBUG, which would compile these checks away.
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

namespace {

// A surface inside the calibrator's family: eta=1, lambda=0.3, rho=-0.98+1.96*sigmoid(-1).
essvi::Surface knownSurface() {
  const double rho = -0.98 + 1.96 / (1.0 + std::exp(1.0));
  const auto slice = [&](double t, double theta) {
    return essvi::Slice{t, theta, theta * std::pow(theta, -0.3), rho};
  };
  return {{slice(0.5, 0.02), slice(1.0, 0.04)}};
}

bool fittedInsideBand(const essvi::Surface& fitted, const essvi::Quote& q, double tol) {
  const double iv = fitted.impliedVolatility(q.expiry, std::log(q.strike / q.forward));
  return iv >= q.bid_iv - tol && iv <= q.ask_iv + tol;
}

void calibrationKeepsFitInsideEveryBidAskBand() {
  // Each strike is quoted twice: a tight band and a wide band skewed upward. The known
  // surface lies inside both, but the average of the mids lies outside the tight band.
  const auto truth = knownSurface();
  std::vector<essvi::Quote> quotes;
  for (const auto& s : truth.slices) {
    for (int i = -6; i <= 6; ++i) {
      const double k = 0.05 * i, strike = 100.0 * std::exp(k);
      const double iv = truth.impliedVolatility(s.expiry, k);
      const auto add = [&](double bid, double ask) {
        quotes.push_back({"q" + std::to_string(quotes.size()), s.expiry, 100.0, 1.0, strike,
                          bid, ask, 0.5 * (bid + ask), true});
      };
      add(iv - 0.0005, iv + 0.0005);
      add(iv - 0.0005, iv + 0.05);
    }
  }
  const auto result = essvi::calibrate(quotes);
  assert(result.converged);
  for (const auto& q : quotes) assert(fittedInsideBand(result.surface, q, 1e-6));
}

void calibrationReportsConvergedOnlyForAFinishedFit() {
  // The ATM quotes carry lopsided bands, so the ATM starting point overshoots the short expiry
  // and undershoots the long one. The solver runs out of evaluations before reaching the known
  // surface, which lies inside every band; that must be reported as a failure, not a fit.
  const auto rho = knownSurface().slices.front().rho;
  const auto slice = [&](double t, double theta) {
    return essvi::Slice{t, theta, theta * std::pow(theta, -0.3), rho};
  };
  const essvi::Surface truth{{slice(0.25, 0.0100), slice(0.26, 0.0104)}};
  std::vector<essvi::Quote> quotes;
  for (size_t j = 0; j < truth.slices.size(); ++j) {
    const auto& s = truth.slices[j];
    for (int i = -6; i <= 6; ++i) {
      const double k = 0.05 * i, iv = truth.impliedVolatility(s.expiry, k);
      double bid = iv - 0.0005, ask = iv + 0.0005;
      if (i == 0 && j == 0) ask = iv + 0.03;
      if (i == 0 && j == 1) bid = iv - 0.03;
      quotes.push_back({"q" + std::to_string(quotes.size()), s.expiry, 100.0, 1.0,
                        100.0 * std::exp(k), bid, ask, 0.5 * (bid + ask), true});
    }
  }
  const auto result = essvi::calibrate(quotes);
  if (result.converged) {
    for (const auto& q : quotes) assert(fittedInsideBand(result.surface, q, 1e-6));
  } else {
    assert(!result.error.empty());
  }
}

}  // namespace

int main() {
  essvi::Slice s{1.0, 0.04, 0.20, -0.4};
  assert(std::abs(essvi::sliceTotalVariance(s, 0.0) - s.theta) < 1e-12);
  assert(essvi::butterflySafe(s));
  assert(essvi::sliceTotalVariance(s, 1.0) > 0.0);

  essvi::Surface surface{{{0.5, 0.04, 0.20, -0.4},
                          {1.0, 0.07, 0.30, -0.35}}};
  assert(essvi::validate(surface).empty());
  assert(surface.totalVariance(0.75, 0.0) > 0.04);

  calibrationKeepsFitInsideEveryBidAskBand();
  calibrationReportsConvergedOnlyForAFinishedFit();
  return 0;
}
