#pragma once

#include <cmath>
#include <cstdint>
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

// Martini--Mingone necessary-and-sufficient bound on psi^2 (arXiv:2106.02418 / 2204.00312).
double mmPsi2Bound(double theta, double abs_rho, double safety = 1e-10);

namespace detail {
// Legacy finite-grid envelope for candidate generation only. It can overestimate
// the true infimum; feasibility checks must use mmPsi2Bound/butterflySafe.
double mmPsi2BoundSearchHint(double theta, double abs_rho, double safety = 1e-10);

class MmBoundCallScope {
 public:
  explicit MmBoundCallScope(std::uint64_t* counter) noexcept;
  ~MmBoundCallScope();
  MmBoundCallScope(const MmBoundCallScope&) = delete;
  MmBoundCallScope& operator=(const MmBoundCallScope&) = delete;

 private:
  std::uint64_t* previous_;
};
std::uint64_t* currentMmBoundCounter() noexcept;
}  // namespace detail

#ifdef ESSVI_TESTING
double mmPsi2BoundReference(double theta, double abs_rho, double safety = 1e-10);
#endif

#ifdef ESSVI_PROFILE_MM_BOUND
struct MmBoundProfile {
  std::uint64_t calls{};
  std::uint64_t nanoseconds{};
};
void resetMmBoundProfile() noexcept;
MmBoundProfile mmBoundProfile() noexcept;
#endif

// Butterfly: Roger-Lee psi*(1+|rho|)<4 plus the MM infimum (replaces GJ's psi^2 (1+|rho|)<4 theta).
bool butterflySafe(const Slice& slice, double safety = 1e-10);

// Hendriks-Martini / Corbetta adjacent-slice calendar sufficient test.
bool calendarSafe(const Slice& shorter, const Slice& longer,
                  double safety = 1e-10);

std::string validate(const Surface& surface);

} // namespace essvi
