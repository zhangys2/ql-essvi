#include "essvi_surface.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace essvi {

double sliceTotalVariance(const Slice& s, double k) {
  const double x = s.psi * k + s.theta * s.rho;
  return 0.5 * (s.theta + s.rho * s.psi * k +
                std::sqrt(x * x + s.theta * s.theta * (1.0 - s.rho * s.rho)));
}

Slice Surface::sliceAt(double expiry) const {
  if (slices.empty()) return {};
  if (slices.size() == 1 || expiry <= slices.front().expiry) return slices.front();
  if (expiry >= slices.back().expiry) return slices.back();
  auto hi = std::lower_bound(slices.begin(), slices.end(), expiry,
                             [](const Slice& s, double t) { return s.expiry < t; });
  const auto& b = *hi;
  const auto& a = *(hi - 1);
  const double u = (expiry - a.expiry) / (b.expiry - a.expiry);
  // Corbetta et al.: interpolate (theta, psi, rho*psi), not rho directly.
  const double psi = a.psi + u * (b.psi - a.psi);
  return {expiry, a.theta + u * (b.theta - a.theta), psi,
          ((1 - u) * a.rho * a.psi + u * b.rho * b.psi) / psi};
}

double Surface::totalVariance(double expiry, double k) const {
  if (slices.empty()) return NAN;
  return sliceTotalVariance(sliceAt(expiry), k);
}

double Surface::impliedVolatility(double expiry, double k) const {
  if (!(expiry > 0)) return NAN;
  const double w = totalVariance(expiry, k);
  return w >= 0 ? std::sqrt(w / expiry) : NAN;
}

double mmPsi2Bound(double theta, double abs_rho, double safety) {
  const double ar = abs_rho;
  if (!(theta > safety) || !(ar >= 0.0) || !(ar < 1.0 - safety)) return 0.0;
  const double s = std::sqrt(std::max(0.0, 1.0 - ar * ar));
  if (!(s > safety)) return 0.0;
  const double l2 = 1.0 / std::tan(std::acos(-ar) / 3.0);
  auto ratio = [&](double l) {
    const double n = s + ar * l + std::sqrt(l * l + 1.0);
    const double np = ar + l / std::sqrt(l * l + 1.0);
    const double npp = std::pow(l * l + 1.0, -1.5);
    const double g = np / 4.0;
    const double h = 1.0 - (l - ar / s) * np / (2.0 * n);
    const double g2 = npp - np * np / (2.0 * n);
    const double den = theta * s * g * g - g2;
    if (!(den > 0.0) || !(h * h >= 0.0)) return HUGE_VAL;
    return 4.0 * theta * s * h * h / den;
  };
  double best = HUGE_VAL;
  const double lo = l2 + 1e-8;
  const double hi = std::max(50.0, lo + 1.0);
  for (int i = 0; i <= 256; ++i) {
    const double u = static_cast<double>(i) / 256.0;
    const double l = lo * std::pow(hi / lo, u);
    best = std::min(best, ratio(l));
  }
  return std::isfinite(best) ? best : 0.0;
}

bool butterflySafe(const Slice& s, double safety) {
  const double ar = std::abs(s.rho);
  return s.theta > safety && s.psi > safety && ar < 1.0 - safety &&
         s.psi * (1.0 + ar) < 4.0 - safety &&
         s.psi * s.psi <= mmPsi2Bound(s.theta, ar, safety) - safety;
}

bool calendarSafe(const Slice& a, const Slice& b, double safety) {
  if (!(b.expiry > a.expiry && b.theta > a.theta + safety &&
        b.psi > a.psi + safety && a.psi > safety && b.psi > safety)) return false;
  const double lower = a.psi * std::max((1+a.rho)/(1+b.rho),
                                         (1-a.rho)/(1-b.rho));
  const double upper = a.psi / a.theta * b.theta;
  return b.psi > lower + safety && b.psi < upper - safety;
}

std::string validate(const Surface& surface) {
  if (surface.slices.empty()) return "surface has no slices";
  for (size_t i = 0; i < surface.slices.size(); ++i) {
    if (!butterflySafe(surface.slices[i])) {
      return "butterfly constraint failed at slice " + std::to_string(i);
    }
    if (i && !calendarSafe(surface.slices[i-1], surface.slices[i])) {
      return "calendar constraint failed between slices " + std::to_string(i-1) +
             " and " + std::to_string(i);
    }
  }
  return {};
}

} // namespace essvi
