#include "essvi_ql.hpp"

#undef NDEBUG  // Release builds define NDEBUG, which would compile these checks away.
#include <cassert>
#include <cmath>

int main() {
  using QuantLib::Date;
  using QuantLib::November;
  using QuantLib::Real;
  using QuantLib::Time;

  const essvi::Surface surface{{{0.5, 0.04, 0.20, -0.4}, {1.0, 0.07, 0.30, -0.35}}};
  assert(essvi::validate(surface).empty());
  const Real F = 100.0;
  const Date ref(11, November, 2024);
  essvi::EssviBlackVarianceSurface ts(ref, surface, {F, F});

  const Time t = 0.5;
  const Real k = 0.10;
  const Real strike = F * std::exp(k);
  const Real w = surface.totalVariance(t, k);
  assert(std::abs(ts.blackVariance(t, strike) - w) < 1e-14);
  assert(std::abs(ts.blackVol(t, strike) - std::sqrt(w / t)) < 1e-14);
  assert(std::abs(ts.blackVariance(t, F) - surface.slices.front().theta) < 1e-14);
  assert(std::abs(ts.atmLevel(t) - F) < 1e-14);

  essvi::EssviSmileSection section(t, F, surface.slices.front());
  assert(std::abs(section.variance(strike) - essvi::sliceTotalVariance(surface.slices.front(), k)) <
         1e-14);
  assert(std::abs(section.volatility(strike) - std::sqrt(section.variance(strike) / t)) < 1e-14);
  assert(std::abs(section.atmLevel() - F) < 1e-14);

  const auto smile = ts.smileSection(t);
  assert(std::abs(smile->variance(strike) - ts.blackVariance(t, strike)) < 1e-14);
  assert(std::abs(smile->volatility(strike) - ts.blackVol(t, strike)) < 1e-14);

  const Time mid = 0.75;
  const Real mid_k = -0.05;
  const Real mid_strike = F * std::exp(mid_k);
  assert(std::abs(ts.blackVariance(mid, mid_strike) - surface.totalVariance(mid, mid_k)) < 1e-14);

  const Date expiry = ref + 183;
  const Real date_var = ts.blackVariance(expiry, F, true);
  const Time date_t = ts.timeFromReference(expiry);
  assert(std::abs(date_var - surface.totalVariance(date_t, 0.0)) < 1e-14);
  return 0;
}
