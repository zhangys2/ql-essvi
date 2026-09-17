#include "calibrator.hpp"
#include "essvi_surface.hpp"

#undef NDEBUG  // Release builds define NDEBUG, which would compile these checks away.
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

namespace {

std::vector<essvi::Quote> quotesOn(const essvi::Surface& surface, double bid_off, double ask_off) {
  std::vector<essvi::Quote> quotes;
  for (const auto& s : surface.slices) {
    for (int i = -6; i <= 6; ++i) {
      const double k = 0.05 * i, strike = 100.0 * std::exp(k);
      const double iv = surface.impliedVolatility(s.expiry, k);
      quotes.push_back({"q" + std::to_string(quotes.size()), s.expiry, 100.0, 1.0, strike,
                        iv + bid_off, iv + ask_off, iv + 0.5 * (bid_off + ask_off), true});
    }
  }
  return quotes;
}

bool fittedInsideBand(const essvi::Surface& fitted, const essvi::Quote& q, double tol) {
  const double iv = fitted.impliedVolatility(q.expiry, std::log(q.strike / q.forward));
  return iv >= q.bid_iv - tol && iv <= q.ask_iv + tol;
}

// Three eSSVI slices with non-monotone rho. D13's sigmoid(a+b log T) is monotone in T,
// so a global-family calibrator cannot recover this surface.
essvi::Surface perSliceSurface() {
  return {{{0.25, 0.010, 0.04, -0.50},
           {0.50, 0.020, 0.075, -0.10},
           {1.00, 0.040, 0.14, -0.20}}};
}

void calibrationRecoversFreeRhoPerSlice() {
  const auto truth = perSliceSurface();
  assert(essvi::validate(truth).empty());
  const auto quotes = quotesOn(truth, -0.0005, 0.0005);
  const auto result = essvi::calibrate(quotes);
  assert(result.converged);
  assert(result.surface.slices.size() == 3);
  assert(essvi::validate(result.surface).empty());
  for (const auto& q : quotes) assert(fittedInsideBand(result.surface, q, 1e-5));
  const auto& s = result.surface.slices;
  assert(s[1].rho > s[0].rho);
  assert(s[1].rho > s[2].rho);
  assert(s[0].rho < -0.25);
  assert(s[1].rho > -0.25);
}

void calibrationKeepsFitInsideEveryBidAskBand() {
  // Each strike is quoted twice: a tight band and a wide band skewed upward. The known
  // surface lies inside both, but the average of the mids lies outside the tight band.
  const auto truth = perSliceSurface();
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
  for (const auto& q : quotes) assert(fittedInsideBand(result.surface, q, 1e-5));
}

void calibrationReportsErrorWhenNoIncludedQuotes() {
  std::vector<essvi::Quote> quotes{
      {"q0", 0.5, 100.0, 1.0, 100.0, 0.2, 0.21, 0.205, false}};
  const auto result = essvi::calibrate(quotes);
  assert(!result.converged);
  assert(!result.error.empty());
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
  assert(std::abs(surface.sliceAt(0.5).theta - 0.04) < 1e-12);
  assert(surface.sliceAt(0.75).theta > 0.04);
  assert(surface.totalVariance(0.75, 0.0) > 0.04);
  essvi::Slice rising_phi{1.0, 0.05, 0.30, -0.4};
  assert(!essvi::calendarSafe({0.5, 0.04, 0.20, -0.4}, rising_phi));

  calibrationRecoversFreeRhoPerSlice();
  calibrationKeepsFitInsideEveryBidAskBand();
  calibrationReportsErrorWhenNoIncludedQuotes();
  return 0;
}
