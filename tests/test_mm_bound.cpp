#include "essvi_surface.hpp"

#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>

namespace {
double ratio(double theta, double abs_rho, double l) {
  const double s = std::sqrt(1.0 - abs_rho * abs_rho);
  const double root = std::sqrt(l * l + 1.0);
  const double n = s + abs_rho * l + root;
  const double np = abs_rho + l / root;
  const double npp = 1.0 / ((l * l + 1.0) * root);
  const double g = np / 4.0;
  const double h = 1.0 - (l - abs_rho / s) * np / (2.0 * n);
  const double g2 = npp - np * np / (2.0 * n);
  const double den = theta * s * g * g - g2;
  return den > 0.0 ? 4.0 * theta * s * h * h / den : HUGE_VAL;
}

double fineBound(double theta, double abs_rho) {
  const double l2 = 1.0 / std::tan(std::acos(-abs_rho) / 3.0);
  const double lo = l2 + 1e-8;
  const double hi = std::max(50.0, lo + 1.0);
  constexpr int points = 4096;
  double best = HUGE_VAL;
  int best_index = 0;
  for (int i = 0; i <= points; ++i) {
    const double u = static_cast<double>(i) / points;
    const double l = lo * std::exp(std::log(hi / lo) * u);
    const double value = ratio(theta, abs_rho, l);
    if (value < best) {
      best = value;
      best_index = i;
    }
  }

  double left = lo * std::exp(std::log(hi / lo) *
                               static_cast<double>(std::max(0, best_index - 1)) / points);
  double right = lo * std::exp(std::log(hi / lo) *
                                static_cast<double>(std::min(points, best_index + 1)) / points);
  constexpr double golden = 0.381966011250105;
  double a = std::log(left), b = std::log(right);
  double x1 = a + golden * (b - a), x2 = b - golden * (b - a);
  double f1 = ratio(theta, abs_rho, std::exp(x1));
  double f2 = ratio(theta, abs_rho, std::exp(x2));
  for (int i = 0; i < 48; ++i) {
    if (f1 < f2) {
      b = x2;
      x2 = x1;
      f2 = f1;
      x1 = a + golden * (b - a);
      f1 = ratio(theta, abs_rho, std::exp(x1));
    } else {
      a = x1;
      x1 = x2;
      f1 = f2;
      x2 = b - golden * (b - a);
      f2 = ratio(theta, abs_rho, std::exp(x2));
    }
  }
  return std::min(best, std::min(f1, f2));
}

void checkConservative(double theta, double rho, double truth) {
  const double fast = essvi::mmPsi2Bound(theta, rho);
  const double tol = 1e-12 * std::abs(truth);
  assert(std::isfinite(fast) && fast > 0.0);
  assert(fast <= truth + tol);
  assert(fast >= truth - 1e-9 * std::abs(truth));
}
}  // namespace

int main() {
  // This pair exposes the old 257-point grid's non-conservative discretization.
  constexpr double theta = 1.4902654986887248e-5;
  constexpr double rho = 0.2993688263167309;
  checkConservative(theta, rho, fineBound(theta, rho));

  std::mt19937_64 random(0x8f3f73b5cf1c9ad1ULL);
  std::uniform_real_distribution<double> log_theta(-6.0, std::log10(5.0));
  std::uniform_real_distribution<double> rho_distribution(0.0, 0.99);
  for (int i = 0; i < 1'000'000; ++i) {
    const double sample_theta = std::pow(10.0, log_theta(random));
    const double sample_rho = rho_distribution(random);
    const double fast = essvi::mmPsi2Bound(sample_theta, sample_rho);
    const double reference =
        essvi::mmPsi2BoundReference(sample_theta, sample_rho);
    const double tolerance = 1e-12 * std::abs(reference);
    const double hint =
        essvi::detail::mmPsi2BoundSearchHint(sample_theta, sample_rho);
    assert(std::isfinite(fast) && fast > 0.0);
    assert(fast <= reference + tolerance);
    assert(std::abs(hint - reference) <= 1e-14 * std::abs(reference));
  }

  // Dense scans plus local refinement validate randomly distributed true infima.
  for (int i = 0; i < 4096; ++i) {
    const double sample_theta = std::pow(10.0, log_theta(random));
    const double sample_rho = rho_distribution(random);
    checkConservative(sample_theta, sample_rho,
                      fineBound(sample_theta, sample_rho));
  }
  return 0;
}
