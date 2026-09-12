#include "essvi_surface.hpp"

#include <algorithm>
#include <sstream>

namespace essvi {

double sliceTotalVariance(const Slice& s, double k) {
  const double x = s.psi * k + s.theta * s.rho;
  return 0.5 * (s.theta + s.rho * s.psi * k +
                std::sqrt(x * x + s.theta * s.theta * (1.0 - s.rho * s.rho)));
}

double Surface::totalVariance(double expiry, double k) const {
  if (slices.empty()) return NAN;
  if (slices.size() == 1) return sliceTotalVariance(slices.front(), k);
  if (expiry <= slices.front().expiry) return sliceTotalVariance(slices.front(), k);
  if (expiry >= slices.back().expiry) return sliceTotalVariance(slices.back(), k);
  auto hi = std::lower_bound(slices.begin(), slices.end(), expiry,
                             [](const Slice& s, double t) { return s.expiry < t; });
  const auto& b = *hi;
  const auto& a = *(hi - 1);
  const double u = (expiry - a.expiry) / (b.expiry - a.expiry);
  // Corbetta et al.: interpolate (theta, psi, rho*psi), not rho directly.
  Slice x{expiry, a.theta + u * (b.theta - a.theta),
          a.psi + u * (b.psi - a.psi),
          ((1-u)*a.rho*a.psi + u*b.rho*b.psi) /
              (a.psi + u * (b.psi - a.psi))};
  return sliceTotalVariance(x, k);
}

double Surface::impliedVolatility(double expiry, double k) const {
  if (!(expiry > 0)) return NAN;
  const double w = totalVariance(expiry, k);
  return w >= 0 ? std::sqrt(w / expiry) : NAN;
}

bool butterflySafe(const Slice& s, double safety) {
  const double ar = std::abs(s.rho);
  return s.theta > safety && s.psi > safety && std::abs(s.rho) < 1.0 - safety &&
         s.psi * (1.0 + ar) < 4.0 - safety &&
         s.psi * s.psi * (1.0 + ar) < 4.0 * s.theta - safety;
}

bool calendarSafe(const Slice& a, const Slice& b, double safety) {
  if (!(b.expiry > a.expiry && b.theta > a.theta + safety &&
        b.psi > a.psi + safety && a.psi > safety && b.psi > safety)) return false;
  const double lower = a.psi * std::max((1+a.rho)/(1+b.rho),
                                         (1-a.rho)/(1-b.rho));
  return b.psi > lower + safety;
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
