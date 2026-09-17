#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace essvi {

struct Slice {
  double expiry{};
  double theta{}; // forward ATM total variance
  double psi{};   // theta * phi
  double rho{};
};

struct Surface {
  std::vector<Slice> slices;

  // Corbetta interpolation of (theta, psi, rho*psi) onto expiry, clamped to the
  // first and last listed slices.
  Slice sliceAt(double expiry) const;
  // Evaluates total variance at expiry and forward log-moneyness k.
  double totalVariance(double expiry, double k) const;
  double impliedVolatility(double expiry, double k) const;
};

double sliceTotalVariance(const Slice& slice, double k);

// Fast, explicit sufficient butterfly constraints from Gatheral-Jacquier.
bool butterflySafe(const Slice& slice, double safety = 1e-10);

// Hendriks-Martini / Corbetta adjacent-slice calendar sufficient test.
bool calendarSafe(const Slice& shorter, const Slice& longer,
                  double safety = 1e-10);

std::string validate(const Surface& surface);

} // namespace essvi
