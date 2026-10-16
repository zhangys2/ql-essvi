#include "essvi_surface.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <sstream>

#ifdef ESSVI_PROFILE_MM_BOUND
#include <chrono>
#endif

namespace essvi {
namespace {
constexpr std::size_t kMmGridSize = 257;
constexpr std::size_t kRhoGridSize = 100;
constexpr double kBoundRelativeSlack = 5e-13;

struct BoundPoint {
  double l{};
  double g{};
  double h{};
  double g2{};
};

struct RhoWorkspace {
  double abs_rho{};
  double s{};
  std::array<BoundPoint, kMmGridSize> points{};
};

struct WorkspaceCache {
  std::array<RhoWorkspace, kRhoGridSize> workspaces{};
  std::array<std::once_flag, kRhoGridSize> initialized{};
};

struct BoundEstimate {
  double safe{};
  double grid_hint{};
};

thread_local std::uint64_t* activeMmBoundCounter{};

void fillWorkspace(RhoWorkspace& workspace, double abs_rho) {
  workspace.abs_rho = abs_rho;
  workspace.s = std::sqrt(std::max(0.0, 1.0 - abs_rho * abs_rho));
  const double l2 = 1.0 / std::tan(std::acos(-abs_rho) / 3.0);
  const double lo = l2 + 1e-8;
  const double hi = std::max(50.0, lo + 1.0);
  for (std::size_t i = 0; i < kMmGridSize; ++i) {
    const double u = static_cast<double>(i) / 256.0;
    const double l = lo * std::pow(hi / lo, u);
    const double root = std::sqrt(l * l + 1.0);
    const double n = workspace.s + abs_rho * l + root;
    const double np = abs_rho + l / root;
    const double npp = std::pow(l * l + 1.0, -1.5);
    const double g = np / 4.0;
    const double h = 1.0 - (l - abs_rho / workspace.s) * np / (2.0 * n);
    const double g2 = npp - np * np / (2.0 * n);
    workspace.points[i] = {l, g, h, g2};
  }
}

WorkspaceCache& workspaceCache() {
  static WorkspaceCache cache;
  return cache;
}

const RhoWorkspace& workspaceFor(double abs_rho) {
  const long grid_index = std::lround(abs_rho * 100.0);
  if (grid_index >= 0 && grid_index < static_cast<long>(kRhoGridSize)) {
    const double grid_rho = static_cast<double>(grid_index) / 100.0;
    if (std::abs(abs_rho - grid_rho) <= 1e-12) {
      thread_local long cached_grid_index = -1;
      thread_local const RhoWorkspace* cached_grid_workspace = nullptr;
      if (cached_grid_index == grid_index && cached_grid_workspace)
        return *cached_grid_workspace;
      auto& cache = workspaceCache();
      auto& workspace = cache.workspaces[static_cast<std::size_t>(grid_index)];
      std::call_once(cache.initialized[static_cast<std::size_t>(grid_index)], [&] {
        fillWorkspace(workspace, grid_rho);
      });
      cached_grid_index = grid_index;
      cached_grid_workspace = &workspace;
      return workspace;
    }
  }

  thread_local RhoWorkspace arbitrary_workspace;
  thread_local double cached_rho = std::numeric_limits<double>::quiet_NaN();
  if (cached_rho != abs_rho) {
    fillWorkspace(arbitrary_workspace, abs_rho);
    cached_rho = abs_rho;
  }
  return arbitrary_workspace;
}

double gridRatio(double theta, const RhoWorkspace& workspace,
                 const BoundPoint& point) {
  const double denominator =
      theta * workspace.s * point.g * point.g - point.g2;
  if (!(denominator > 0.0)) return HUGE_VAL;
  return 4.0 * theta * workspace.s * point.h * point.h / denominator;
}

double ratioAt(double theta, const RhoWorkspace& workspace, double l) {
  const double l2 = l * l + 1.0;
  const double root = std::sqrt(l2);
  const double n = workspace.s + workspace.abs_rho * l + root;
  const double np = workspace.abs_rho + l / root;
  const double npp = 1.0 / (l2 * root);
  const double g = np / 4.0;
  const double h = 1.0 - (l - workspace.abs_rho / workspace.s) * np / (2.0 * n);
  const double g2 = npp - np * np / (2.0 * n);
  const double denominator = theta * workspace.s * g * g - g2;
  if (!(denominator > 0.0) || !(h * h >= 0.0)) return HUGE_VAL;
  return 4.0 * theta * workspace.s * h * h / denominator;
}

double refineMinimum(double theta, const RhoWorkspace& workspace, std::size_t i,
                     double coarse_value) {
  double a = workspace.points[i - 1].l;
  double b = workspace.points[i + 1].l;
  constexpr double golden = 0.381966011250105;
  double x1 = a + golden * (b - a);
  double x2 = b - golden * (b - a);
  double f1 = ratioAt(theta, workspace, x1);
  double f2 = ratioAt(theta, workspace, x2);
  double best = std::min(coarse_value, std::min(f1, f2));
  for (int step = 0; step < 36; ++step) {
    if (f1 < f2) {
      b = x2;
      x2 = x1;
      f2 = f1;
      x1 = a + golden * (b - a);
      f1 = ratioAt(theta, workspace, x1);
    } else {
      a = x1;
      x1 = x2;
      f1 = f2;
      x2 = b - golden * (b - a);
      f2 = ratioAt(theta, workspace, x2);
    }
    best = std::min(best, std::min(f1, f2));
  }
  return best;
}

BoundEstimate refinedBound(double theta, const RhoWorkspace& workspace) {
  double previous = gridRatio(theta, workspace, workspace.points[0]);
  double current = gridRatio(theta, workspace, workspace.points[1]);
  double best = std::min(previous, current);
  double grid_best = best;
  for (std::size_t i = 1; i + 1 < kMmGridSize; ++i) {
    const double next = gridRatio(theta, workspace, workspace.points[i + 1]);
    grid_best = std::min(grid_best, next);
    if (std::isfinite(current)) {
      best = std::min(best, current);
      if (current <= previous && current <= next)
        best = std::min(best, refineMinimum(theta, workspace, i, current));
    }
    previous = current;
    current = next;
  }
  best = std::min(best, current);
  if (!std::isfinite(grid_best)) grid_best = 0.0;
  if (!std::isfinite(best)) return {0.0, grid_best};
  return {best * (1.0 - kBoundRelativeSlack), grid_best};
}

#ifdef ESSVI_PROFILE_MM_BOUND
std::atomic<std::uint64_t> mmBoundProfileCalls{0};
std::atomic<std::uint64_t> mmBoundNanoseconds{0};
struct MmBoundTimer {
  std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
  ~MmBoundTimer() {
    mmBoundProfileCalls.fetch_add(1, std::memory_order_relaxed);
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - start);
    mmBoundNanoseconds.fetch_add(static_cast<std::uint64_t>(elapsed.count()),
                                 std::memory_order_relaxed);
  }
};
#endif

BoundEstimate boundEstimate(double theta, double abs_rho, double safety) {
#ifdef ESSVI_PROFILE_MM_BOUND
  [[maybe_unused]] const MmBoundTimer profile_timer;
#endif
  if (activeMmBoundCounter) ++*activeMmBoundCounter;
  if (!(theta > safety) || !(abs_rho >= 0.0) ||
      !(abs_rho < 1.0 - safety)) return {};
  const auto& workspace = workspaceFor(abs_rho);
  if (!(workspace.s > safety)) return {};

  struct LastBound {
    double theta{std::numeric_limits<double>::quiet_NaN()};
    double abs_rho{std::numeric_limits<double>::quiet_NaN()};
    BoundEstimate estimate{};
  };
  thread_local LastBound last;
  if (theta == last.theta && abs_rho == last.abs_rho) return last.estimate;
  const auto estimate = refinedBound(theta, workspace);
  last = {theta, abs_rho, estimate};
  return estimate;
}

}  // namespace

namespace detail {
MmBoundCallScope::MmBoundCallScope(std::uint64_t* counter) noexcept
    : previous_(activeMmBoundCounter) {
  activeMmBoundCounter = counter;
}

MmBoundCallScope::~MmBoundCallScope() {
  activeMmBoundCounter = previous_;
}

std::uint64_t* currentMmBoundCounter() noexcept {
  return activeMmBoundCounter;
}
}  // namespace detail

#ifdef ESSVI_PROFILE_MM_BOUND
void resetMmBoundProfile() noexcept {
  mmBoundProfileCalls.store(0, std::memory_order_relaxed);
  mmBoundNanoseconds.store(0, std::memory_order_relaxed);
}

MmBoundProfile mmBoundProfile() noexcept {
  return {mmBoundProfileCalls.load(std::memory_order_relaxed),
          mmBoundNanoseconds.load(std::memory_order_relaxed)};
}
#endif

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
  return boundEstimate(theta, abs_rho, safety).safe;
}

namespace detail {
double mmPsi2BoundSearchHint(double theta, double abs_rho, double safety) {
#ifdef ESSVI_PROFILE_MM_BOUND
  [[maybe_unused]] const MmBoundTimer profile_timer;
#endif
  if (activeMmBoundCounter) ++*activeMmBoundCounter;
  if (!(theta > safety) || !(abs_rho >= 0.0) || !(abs_rho < 1.0 - safety))
    return 0.0;
  const auto& workspace = workspaceFor(abs_rho);
  if (!(workspace.s > safety)) return 0.0;
  double best = HUGE_VAL;
  for (const auto& point : workspace.points)
    best = std::min(best, gridRatio(theta, workspace, point));
  return std::isfinite(best) ? best : 0.0;
}
}  // namespace detail

#ifdef ESSVI_TESTING
double mmPsi2BoundReference(double theta, double abs_rho, double safety) {
  if (!(theta > safety) || !(abs_rho >= 0.0) || !(abs_rho < 1.0 - safety)) return 0.0;
  const double s = std::sqrt(std::max(0.0, 1.0 - abs_rho * abs_rho));
  if (!(s > safety)) return 0.0;
  const double l2 = 1.0 / std::tan(std::acos(-abs_rho) / 3.0);
  auto ratio = [&](double l) {
    const double n = s + abs_rho * l + std::sqrt(l * l + 1.0);
    const double np = abs_rho + l / std::sqrt(l * l + 1.0);
    const double npp = std::pow(l * l + 1.0, -1.5);
    const double g = np / 4.0;
    const double h = 1.0 - (l - abs_rho / s) * np / (2.0 * n);
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
#endif

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
