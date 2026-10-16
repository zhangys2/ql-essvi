#include "calibrator.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef ESSVI_MAX_CALIBRATION_THREADS
#define ESSVI_MAX_CALIBRATION_THREADS 4
#endif

namespace essvi {
namespace {
constexpr double kEps = 1e-8;
constexpr std::size_t kMaxCalibrationThreads = ESSVI_MAX_CALIBRATION_THREADS;
using Clock = std::chrono::steady_clock;

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> r;
  std::stringstream x(s);
  std::string v;
  while (std::getline(x, v, ',')) r.push_back(v);
  return r;
}
double val(const std::string& s) { return s.empty() ? NAN : std::stod(s); }

double secondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

class RhoWorkerPool {
 public:
  explicit RhoWorkerPool(bool enabled) : workers_started_(!enabled) {}
  RhoWorkerPool(const RhoWorkerPool&) = delete;
  RhoWorkerPool& operator=(const RhoWorkerPool&) = delete;

  ~RhoWorkerPool() { stopWorkers(); }

  template <class Function>
  void parallelFor(std::size_t count, Function&& function) {
    if (count == 0) return;
    if (count == 1 || kMaxCalibrationThreads <= 1) {
      function(0, count);
      return;
    }
    ensureWorkers();
    if (workers_.empty()) {
      function(0, count);
      return;
    }

    using Callable = std::remove_reference_t<Function>;
    auto* parent_counter = detail::currentMmBoundCounter();
    task_context_ = std::addressof(function);
    task_function_ = [](const void* context, std::size_t begin, std::size_t end) {
      (*static_cast<const Callable*>(context))(begin, end);
    };
    const std::size_t thread_count = std::min(workers_.size() + 1, count);
    const auto [main_begin, main_end] = rangeFor(count, thread_count, 0);
    {
      std::lock_guard lock(mutex_);
      task_count_ = count;
      active_thread_count_ = thread_count;
      pending_workers_ = thread_count - 1;
      std::fill(worker_bound_calls_.begin(), worker_bound_calls_.end(), 0);
      ++generation_;
    }
    work_available_.notify_all();

    std::uint64_t main_calls = 0;
    {
      detail::MmBoundCallScope call_scope(&main_calls);
      task_function_(task_context_, main_begin, main_end);
    }
    {
      std::unique_lock lock(mutex_);
      work_complete_.wait(lock, [&] { return pending_workers_ == 0; });
      task_function_ = nullptr;
      task_context_ = nullptr;
    }
    if (parent_counter) {
      *parent_counter += main_calls;
      for (std::size_t worker = 0; worker + 1 < thread_count; ++worker)
        *parent_counter += worker_bound_calls_[worker];
    }
  }

 private:
  using TaskFunction = void (*)(const void*, std::size_t, std::size_t);

  static std::pair<std::size_t, std::size_t> rangeFor(
      std::size_t count, std::size_t thread_count, std::size_t worker) {
    const std::size_t base = count / thread_count;
    const std::size_t remainder = count % thread_count;
    const std::size_t begin = worker * base + std::min(worker, remainder);
    const std::size_t length = base + (worker < remainder ? 1 : 0);
    return {begin, begin + length};
  }

  void ensureWorkers() {
    if (workers_started_) return;
    workers_started_ = true;
    workers_.reserve(kMaxCalibrationThreads - 1);
    worker_bound_calls_.resize(kMaxCalibrationThreads - 1);
    for (std::size_t worker = 1; worker < kMaxCalibrationThreads; ++worker) {
      try {
        workers_.emplace_back(&RhoWorkerPool::workerLoop, this, worker);
      } catch (const std::system_error&) {
        break;
      }
    }
  }

  void workerLoop(std::size_t worker) {
    std::size_t observed_generation = 0;
    for (;;) {
      std::size_t begin = 0;
      std::size_t end = 0;
      {
        std::unique_lock lock(mutex_);
        work_available_.wait(lock, [&] {
          return stopping_ || generation_ != observed_generation;
        });
        if (stopping_) return;
        observed_generation = generation_;
        if (worker >= active_thread_count_) continue;
        const auto range = rangeFor(task_count_, active_thread_count_, worker);
        begin = range.first;
        end = range.second;
      }

      std::uint64_t calls = 0;
      {
        detail::MmBoundCallScope call_scope(&calls);
        task_function_(task_context_, begin, end);
      }
      {
        std::lock_guard lock(mutex_);
        worker_bound_calls_[worker - 1] = calls;
        --pending_workers_;
        if (pending_workers_ == 0) work_complete_.notify_one();
      }
    }
  }

  void stopWorkers() {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
      ++generation_;
    }
    work_available_.notify_all();
    for (auto& worker : workers_)
      if (worker.joinable()) worker.join();
  }

  std::vector<std::thread> workers_;
  std::vector<std::uint64_t> worker_bound_calls_;
  std::mutex mutex_;
  std::condition_variable work_available_;
  std::condition_variable work_complete_;
  std::size_t generation_{};
  std::size_t task_count_{};
  std::size_t active_thread_count_{1};
  std::size_t pending_workers_{};
  bool workers_started_{};
  bool stopping_{};
  const void* task_context_{};
  TaskFunction task_function_{};
};

struct QuoteGroups {
  std::vector<double> expiries;
  std::vector<std::vector<const Quote*>> slices;
};

QuoteGroups groupQuotes(const std::vector<Quote>& quotes) {
  QuoteGroups groups;
  for (const auto& q : quotes)
    if (q.included && std::isfinite(q.expiry)) groups.expiries.push_back(q.expiry);
  std::sort(groups.expiries.begin(), groups.expiries.end());
  groups.expiries.erase(std::unique(groups.expiries.begin(), groups.expiries.end()),
                        groups.expiries.end());
  groups.slices.resize(groups.expiries.size());
  for (const auto& q : quotes) {
    if (!q.included || !std::isfinite(q.expiry)) continue;
    const auto it = std::lower_bound(groups.expiries.begin(), groups.expiries.end(), q.expiry);
    if (it != groups.expiries.end() && *it == q.expiry)
      groups.slices[static_cast<std::size_t>(it - groups.expiries.begin())].push_back(&q);
  }
  return groups;
}

const std::vector<const Quote*>* quoteGroupAt(const QuoteGroups& groups, double expiry) {
  const auto it = std::lower_bound(groups.expiries.begin(), groups.expiries.end(), expiry);
  if (it == groups.expiries.end() || *it != expiry) return nullptr;
  return &groups.slices[static_cast<std::size_t>(it - groups.expiries.begin())];
}

const Slice* surfaceSliceAt(const Surface& surface, double expiry) {
  for (const auto& slice : surface.slices)
    if (slice.expiry == expiry) return &slice;
  return nullptr;
}

bool sameValue(double a, double b) {
  return a == b || (std::isnan(a) && std::isnan(b));
}

bool movementBelowTolerance(double current, double previous, double tolerance) {
  if (sameValue(current, previous)) return true;
  return std::isfinite(current) && std::isfinite(previous) &&
         std::abs(current - previous) < tolerance;
}

bool quoteGroupReusable(const std::vector<const Quote*>& current,
                        const std::vector<const Quote*>& previous,
                        double quote_tol) {
  if (current.size() != previous.size()) return false;
  for (std::size_t i = 0; i < current.size(); ++i) {
    const auto& a = *current[i];
    const auto& b = *previous[i];
    if (a.id != b.id || !sameValue(a.expiry, b.expiry) ||
        !sameValue(a.forward, b.forward) || !sameValue(a.strike, b.strike) ||
        !sameValue(a.mid_iv, b.mid_iv) ||
        !movementBelowTolerance(a.bid_iv, b.bid_iv, quote_tol) ||
        !movementBelowTolerance(a.ask_iv, b.ask_iv, quote_tol))
      return false;
  }
  return true;
}

struct PreparedQuote {
  double k{};
  double bid_variance{};
  double ask_variance{};
  double mid_iv{};
  bool has_band{};
  bool has_mid{};
};

std::vector<PreparedQuote> prepareQuotes(const std::vector<const Quote*>& quotes) {
  std::vector<PreparedQuote> prepared;
  prepared.reserve(quotes.size());
  for (const auto* quote : quotes) {
    const auto& q = *quote;
    const bool has_band = q.included && std::isfinite(q.bid_iv) &&
                          std::isfinite(q.ask_iv);
    const bool has_mid = q.included && std::isfinite(q.mid_iv);
    const double k = has_band || has_mid ? std::log(q.strike / q.forward) : 0.0;
    prepared.push_back({k, q.expiry * q.bid_iv * q.bid_iv,
                        q.expiry * q.ask_iv * q.ask_iv, q.mid_iv,
                        has_band, has_mid});
  }
  return prepared;
}

double bandLossScale(const std::vector<PreparedQuote>& quotes) {
  // Keep zero-width bands represented in the legacy scale diagnostic.
  double scale = 0.0;
  for (const auto& quote : quotes) {
    if (!quote.has_band) continue;
    const double width =
        std::max(quote.ask_variance - quote.bid_variance, 1e-6);
    scale += width * width;
  }
  return scale;
}
double bandLoss(const Slice& s, const std::vector<PreparedQuote>& quotes) {
  double z = 0;
  for (const auto& q : quotes) {
    if (!q.has_band) continue;
    const double w = sliceTotalVariance(s, q.k);
    const double d = w - std::clamp(w, q.bid_variance, q.ask_variance);
    z += d * d;
  }
  return z;
}

double quoteExcessRatio(const Slice& slice, const PreparedQuote& quote) {
  const double variance = sliceTotalVariance(slice, quote.k);
  const double excess = std::max(
      0.0, std::max(quote.bid_variance - variance,
                    variance - quote.ask_variance));
  const double width =
      std::max(quote.ask_variance - quote.bid_variance, 1e-6);
  return excess / width;
}

bool passesQuoteCap(const Slice& slice,
                    const std::vector<PreparedQuote>& quotes, double cap) {
  for (const auto& quote : quotes) {
    if (!quote.has_band) continue;
    const double ratio = quoteExcessRatio(slice, quote);
    if (!std::isfinite(ratio) || ratio > cap) return false;
  }
  return true;
}

bool passesAdaptiveQuoteCap(const Slice& candidate, const Slice& cold,
                            const std::vector<PreparedQuote>& quotes,
                            double cap) {
  for (const auto& quote : quotes) {
    if (!quote.has_band) continue;
    const double candidate_ratio = quoteExcessRatio(candidate, quote);
    const double cold_ratio = quoteExcessRatio(cold, quote);
    if (!std::isfinite(candidate_ratio) || !std::isfinite(cold_ratio) ||
        candidate_ratio > std::max(cap, cold_ratio + 0.01))
      return false;
  }
  return true;
}

class BoundMemo {
 public:
  explicit BoundMemo(bool search_hint) : search_hint_(search_hint) {}

  double get(double theta, double abs_rho) {
    if (theta == theta_ && abs_rho == abs_rho_) return value_;
    value_ = search_hint_
                 ? detail::mmPsi2BoundSearchHint(theta, abs_rho)
                 : mmPsi2Bound(theta, abs_rho, kEps);
    theta_ = theta;
    abs_rho_ = abs_rho;
    return value_;
  }

 private:
  bool search_hint_;
  double theta_{NAN};
  double abs_rho_{NAN};
  double value_{};
};

struct CalendarBounds {
  bool present{};
  bool expiry_increases{};
  bool previous_psi_valid{};
  double theta_lower{};
  double psi_lower{};
  double ratio_lower{};
  double previous_psi_over_theta{};
};

CalendarBounds calendarBounds(const Slice* previous, double expiry, double rho) {
  if (!previous) return {};
  const double ratio =
      std::max((1 + previous->rho) / (1 + rho),
               (1 - previous->rho) / (1 - rho));
  return {true,
          expiry > previous->expiry,
          previous->psi > kEps,
          previous->theta + kEps,
          previous->psi + kEps,
          previous->psi * ratio + kEps,
          previous->psi / previous->theta};
}

bool calendarSafe(const Slice& s, const CalendarBounds& bounds) {
  if (!bounds.present) return true;
  if (!(bounds.expiry_increases && s.theta > bounds.theta_lower &&
        s.psi > bounds.psi_lower && bounds.previous_psi_valid && s.psi > kEps))
    return false;
  const double upper = bounds.previous_psi_over_theta * s.theta;
  return s.psi > bounds.ratio_lower && s.psi < upper - kEps;
}

bool pairSafe(const Slice& s, BoundMemo& bound,
              const CalendarBounds& calendar) {
  const double ar = std::abs(s.rho);
  if (!(s.theta > kEps && s.psi > kEps && ar < 1.0 - kEps &&
        s.psi * (1.0 + ar) < 4.0 - kEps)) return false;
  if (!(s.psi * s.psi <= bound.get(s.theta, ar) - kEps)) return false;
  return calendarSafe(s, calendar);
}

struct Atm {
  double theta_star{NAN};
  double k_star{0};
};

Atm nearestAtm(const std::vector<PreparedQuote>& quotes, double t) {
  Atm a;
  double best = INFINITY;
  for (const auto& q : quotes) {
    if (!q.has_mid) continue;
    if (std::abs(q.k) < best) {
      best = std::abs(q.k);
      a.k_star = q.k;
      a.theta_star = t * q.mid_iv * q.mid_iv;
    }
  }
  return a;
}

Slice anchored(double t, const Atm& atm, double rho, double psi,
               const Slice* prev, BoundMemo& bound) {
  double theta = atm.theta_star - rho * psi * atm.k_star;
  if (prev) theta = std::max(theta, prev->theta + kEps);
  if (prev && prev->psi > kEps)
    theta = std::max(theta, psi * prev->theta / prev->psi + kEps);
  theta = std::max(theta, kEps);
  for (int i = 0; i < 48; ++i) {
    if (psi * psi <= bound.get(theta, std::abs(rho)) - 2 * kEps) break;
    theta *= 1.08;
  }
  return {t, theta, psi, rho};
}

double psiUpper(double theta, double rho, BoundMemo& bound) {
  const double ar = std::abs(rho);
  if (!(ar < 1.0 - kEps) || !(theta > kEps)) return 0;
  const double rl = 4.0 / (1.0 + ar) - kEps;
  const double mm = std::sqrt(std::max(0.0, bound.get(theta, ar))) - kEps;
  return std::min(rl, mm);
}

double psiLower(const Slice* prev, double rho) {
  double lo = 2 * kEps;
  if (!prev) return lo;
  const double ratio =
      std::max((1 + prev->rho) / (1 + rho), (1 - prev->rho) / (1 - rho));
  return std::max(lo, std::max(prev->psi + kEps, prev->psi * ratio + kEps));
}

struct RhoCandidate {
  Slice slice{};
  double loss{INFINITY};
  int evaluations{};
};

CalibrationResult fitSlice(const std::vector<PreparedQuote>& qs, double t,
                           const Slice* prev, RhoWorkerPool& rho_workers,
                           const Slice* warm_start = nullptr,
                           const Slice* restricted_center = nullptr) {
  CalibrationResult out;
  const auto atm = nearestAtm(qs, t);
  if (!std::isfinite(atm.theta_star) || atm.theta_star <= 0) {
    out.error = "no ATM quote";
    return out;
  }

  int evals = 0;
  Slice best{};
  double bestz = INFINITY;
  auto consider = [&](const Slice& slice, BoundMemo& safe_bound,
                      const CalendarBounds& calendar) {
    if (!pairSafe(slice, safe_bound, calendar)) return;
    ++evals;
    const double loss = bandLoss(slice, qs);
    if (loss < bestz) {
      bestz = loss;
      best = slice;
    }
  };

  const double theta0 =
      prev ? std::max(atm.theta_star, prev->theta + kEps) : atm.theta_star;
  const auto searchRhoWindow = [&](double rlo, double rhi, double rstep,
                                   int psi_steps) {
    std::vector<double> rho_values;
    for (double rho = rlo; rho <= rhi + 1e-12; rho += rstep) {
      if (std::abs(rho) < 0.98) rho_values.push_back(rho);
    }
    if (rho_values.empty()) return;

    std::vector<RhoCandidate> results(rho_values.size());
    const auto evaluateRho = [&](double rho) {
      RhoCandidate result;
      BoundMemo candidate_bound(true);
      BoundMemo safe_bound(false);
      const auto calendar = calendarBounds(prev, t, rho);
      const double lo = psiLower(prev, rho);
      const double hi = psiUpper(theta0, rho, candidate_bound);
      if (!(hi > lo)) return result;

      const auto considerRho = [&](const Slice& slice) {
        if (!pairSafe(slice, safe_bound, calendar)) return;
        ++result.evaluations;
        const double loss = bandLoss(slice, qs);
        if (loss < result.loss) {
          result.loss = loss;
          result.slice = slice;
        }
      };
      considerRho(anchored(t, atm, rho, lo, prev, candidate_bound));
      considerRho(anchored(t, atm, rho, hi, prev, candidate_bound));

      double a = lo;
      double b = hi;
      constexpr double golden = 0.381966011250105;
      double x1 = a + golden * (b - a);
      double x2 = b - golden * (b - a);
      const auto evaluatePsi = [&](double psi) {
        const auto slice = anchored(t, atm, rho, psi, prev, candidate_bound);
        if (!pairSafe(slice, safe_bound, calendar)) return 1e300;
        ++result.evaluations;
        return bandLoss(slice, qs);
      };
      double f1 = evaluatePsi(x1);
      double f2 = evaluatePsi(x2);
      for (int step = 0; step < psi_steps; ++step) {
        if (f1 < f2) {
          b = x2;
          x2 = x1;
          f2 = f1;
          x1 = a + golden * (b - a);
          f1 = evaluatePsi(x1);
        } else {
          a = x1;
          x1 = x2;
          f1 = f2;
          x2 = b - golden * (b - a);
          f2 = evaluatePsi(x2);
        }
      }
      considerRho(anchored(t, atm, rho, f1 < f2 ? x1 : x2, prev,
                           candidate_bound));
      return result;
    };

    const auto evaluateRange = [&](std::size_t begin, std::size_t end) {
      for (std::size_t i = begin; i < end; ++i)
        results[i] = evaluateRho(rho_values[i]);
    };
    rho_workers.parallelFor(rho_values.size(), evaluateRange);

    // Fixed rho-order reduction preserves the serial search's strict-less tie break.
    for (const auto& result : results) {
      evals += result.evaluations;
      if (result.loss < bestz) {
        bestz = result.loss;
        best = result.slice;
      }
    }
  };

  if (warm_start) {
    BoundMemo safe_bound(false);
    const auto lossAndNormal = [&](const Slice& slice, double normal[3][3],
                                   double gradient[3]) {
      for (int row = 0; row < 3; ++row) {
        gradient[row] = 0.0;
        for (int column = 0; column < 3; ++column)
          normal[row][column] = 0.0;
      }

      // Relative coordinates keep the three normal-equation columns balanced.
      const double parameter_scale[3] = {
          std::max(slice.theta * 0.1, 1e-3), 0.1,
          std::max(slice.psi * 0.1, 1e-3)};
      double loss = 0.0;
      for (const auto& quote : qs) {
        if (!quote.has_band) continue;
        const double theta = slice.theta;
        const double rho = slice.rho;
        const double psi = slice.psi;
        const double k = quote.k;
        const double a = psi * k + rho * theta;
        const double d = std::sqrt(std::max(
            1e-30, a * a + theta * theta * (1.0 - rho * rho)));
        const double jacobian[3] = {
            0.5 * (1.0 + (a * rho + theta * (1.0 - rho * rho)) / d),
            0.5 * (psi * k + (a * theta - theta * theta * rho) / d),
            0.5 * (rho * k + a * k / d)};
        const double scaled_jacobian[3] = {
            jacobian[0] * parameter_scale[0],
            jacobian[1] * parameter_scale[1],
            jacobian[2] * parameter_scale[2]};
        const double variance = sliceTotalVariance(slice, k);
        const double residual =
            variance - std::clamp(variance, quote.bid_variance, quote.ask_variance);
        loss += residual * residual;
        if (residual == 0.0) continue;
        for (int row = 0; row < 3; ++row) {
          gradient[row] += scaled_jacobian[row] * residual;
          for (int column = 0; column < 3; ++column)
            normal[row][column] +=
                scaled_jacobian[row] * scaled_jacobian[column];
        }
      }
      return loss;
    };

    Slice current = *warm_start;
    current.expiry = t;
    double normal[3][3];
    double gradient[3];
    double current_loss = lossAndNormal(current, normal, gradient);
    ++evals;

    // Use the cold path's ATM anchor only as a start; band loss stays the objective.
    Slice atm_seed = current;
    atm_seed.theta = atm.theta_star - atm_seed.rho * atm_seed.psi * atm.k_star;
    if (prev) {
      atm_seed.theta = std::max(atm_seed.theta, prev->theta + kEps);
      if (prev->psi > kEps)
        atm_seed.theta = std::max(
            atm_seed.theta, atm_seed.psi * prev->theta / prev->psi + kEps);
    }
    atm_seed.theta = std::max(atm_seed.theta, kEps);
    double atm_normal[3][3];
    double atm_gradient[3];
    const double atm_loss = lossAndNormal(atm_seed, atm_normal, atm_gradient);
    ++evals;
    bool current_is_safe = false;
    if (atm_loss <= current_loss &&
        pairSafe(atm_seed, safe_bound, calendarBounds(prev, t, atm_seed.rho))) {
      current = atm_seed;
      current_loss = atm_loss;
      for (int row = 0; row < 3; ++row) {
        gradient[row] = atm_gradient[row];
        for (int column = 0; column < 3; ++column)
          normal[row][column] = atm_normal[row][column];
      }
      current_is_safe = true;
    } else {
      current_is_safe =
          pairSafe(current, safe_bound, calendarBounds(prev, t, current.rho));
    }
    const double theta_min = prev ? prev->theta + kEps : kEps;

    for (int iteration = 0; iteration < 9 && evals < 15; ++iteration) {
      double matrix[3][4]{};
      for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column)
          matrix[row][column] = normal[row][column];
        matrix[row][row] += 1e-6 * std::max(normal[row][row], 1e-12);
        matrix[row][3] = -gradient[row];
      }

      bool solvable = true;
      for (int column = 0; column < 3; ++column) {
        int pivot = column;
        for (int row = column + 1; row < 3; ++row) {
          if (std::abs(matrix[row][column]) >
              std::abs(matrix[pivot][column]))
            pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1e-24) {
          solvable = false;
          break;
        }
        for (int index = column; index < 4; ++index)
          std::swap(matrix[column][index], matrix[pivot][index]);
        const double diagonal = matrix[column][column];
        for (int index = column; index < 4; ++index)
          matrix[column][index] /= diagonal;
        for (int row = 0; row < 3; ++row) {
          if (row == column) continue;
          const double factor = matrix[row][column];
          for (int index = column; index < 4; ++index)
            matrix[row][index] -= factor * matrix[column][index];
        }
      }
      if (!solvable || !(theta_min < 5.0)) break;

      const double parameter_scale[3] = {
          std::max(current.theta * 0.1, 1e-3), 0.1,
          std::max(current.psi * 0.1, 1e-3)};
      Slice selected = current;
      double selected_loss = current_loss;
      double selected_normal[3][3];
      double selected_gradient[3];
      bool found_improvement = false;
      for (double alpha : {1.0, 0.5, 0.25, 0.125, 0.0625}) {
        if (evals >= 15) break;
        Slice trial = current;
        trial.theta = std::clamp(
            current.theta + alpha * parameter_scale[0] * matrix[0][3],
            theta_min, 5.0);
        trial.rho = std::clamp(
            current.rho + alpha * parameter_scale[1] * matrix[1][3],
            -0.979999, 0.979999);
        const double psi_min = psiLower(prev, trial.rho);
        const double psi_max =
            4.0 / (1.0 + std::abs(trial.rho)) - 2.0 * kEps;
        if (!(psi_max > psi_min)) continue;
        trial.psi = std::clamp(
            current.psi + alpha * parameter_scale[2] * matrix[2][3],
            psi_min, psi_max);
        const auto calendar = calendarBounds(prev, t, trial.rho);
        if (!(trial.theta > theta_min - kEps &&
              trial.psi * (1.0 + std::abs(trial.rho)) < 4.0 - kEps &&
              calendarSafe(trial, calendar)))
          continue;

        double trial_normal[3][3];
        double trial_gradient[3];
        const double trial_loss =
            lossAndNormal(trial, trial_normal, trial_gradient);
        ++evals;
        if (trial_loss < current_loss) {
          selected = trial;
          selected_loss = trial_loss;
          for (int row = 0; row < 3; ++row) {
            selected_gradient[row] = trial_gradient[row];
            for (int column = 0; column < 3; ++column)
              selected_normal[row][column] = trial_normal[row][column];
          }
          found_improvement = true;
          break;
        }
      }
      if (!found_improvement) break;

      const auto calendar = calendarBounds(prev, t, selected.rho);
      if (!pairSafe(selected, safe_bound, calendar)) {
        const double mm_bound =
            safe_bound.get(selected.theta, std::abs(selected.rho));
        const double safe_psi = std::sqrt(std::max(0.0, mm_bound - kEps));
        selected.psi = std::min(selected.psi, safe_psi);
        if (!(selected.psi > psiLower(prev, selected.rho))) break;
        if (evals >= 15) break;
        selected_loss = lossAndNormal(selected, selected_normal,
                                      selected_gradient);
        ++evals;
        if (!(selected_loss < current_loss) ||
            !pairSafe(selected, safe_bound, calendar))
          break;
      }

      current = selected;
      current_loss = selected_loss;
      current_is_safe = true;
      for (int row = 0; row < 3; ++row) {
        gradient[row] = selected_gradient[row];
        for (int column = 0; column < 3; ++column)
          normal[row][column] = selected_normal[row][column];
      }
    }

    if (current_is_safe && current_loss < bestz) {
      best = current;
      bestz = current_loss;
    }
  } else if (restricted_center) {
    // Keep the cold fallback near the prior slice; psi still spans its feasible interval.
    const double rho_lower =
        std::max(-0.95, restricted_center->rho - 0.15);
    const double rho_upper =
        std::min(0.95, restricted_center->rho + 0.15);
    searchRhoWindow(rho_lower, rho_upper, 0.05, 22);
    if (bestz < INFINITY) {
      searchRhoWindow(std::max(rho_lower, best.rho - 0.06),
                      std::min(rho_upper, best.rho + 0.06), 0.01, 18);
    }
  } else {
    searchRhoWindow(-0.95, 0.95, 0.05, 22);
    if (bestz < INFINITY) {
      searchRhoWindow(std::max(-0.95, best.rho - 0.06),
                      std::min(0.95, best.rho + 0.06), 0.01, 18);
    }
  }

  if (!warm_start && bestz < INFINITY) {
    const double tlo = prev ? prev->theta + kEps : kEps;
    BoundMemo safe_bound(false);
    const auto calendar = calendarBounds(prev, t, best.rho);
    for (double multiplier : {0.90, 0.95, 1.00, 1.05, 1.10, 1.20}) {
      Slice slice = best;
      slice.theta = std::clamp(best.theta * multiplier, tlo, 5.0);
      consider(slice, safe_bound, calendar);
    }
  }

  out.iterations = evals;
  out.objective_evaluations = static_cast<std::uint64_t>(evals);
  out.objective = bestz;
  if (!(bestz < INFINITY)) {
    out.error = "empty feasible set at t=" + std::to_string(t);
    return out;
  }
  out.surface.slices.push_back(best);
  Surface check{out.surface.slices};
  if (prev) check.slices.insert(check.slices.begin(), *prev);
  out.error = validate(check);
  out.converged = out.error.empty();
  return out;
}
void finishResult(CalibrationResult& result, Clock::time_point started,
                  std::uint64_t bound_calls) {
  result.objective_evaluations = static_cast<std::uint64_t>(result.iterations);
  result.mm_bound_calls = bound_calls;
  result.runtime_seconds = secondsSince(started);
}
}  // namespace

std::vector<Quote> readQuotes(const std::string& path) {
  std::ifstream f(path);
  std::string line;
  std::getline(f, line);
  auto h = split(line);
  std::unordered_map<std::string, size_t> ix;
  for (size_t i = 0; i < h.size(); ++i) ix[h[i]] = i;
  std::vector<Quote> out;
  while (std::getline(f, line)) {
    auto v = split(line);
    if (v.size() < h.size()) continue;
    try {
      out.push_back({v[ix["quote_id"]], val(v[ix["time_to_expiry"]]), val(v[ix["forward"]]),
                     val(v[ix["discount_factor"]]), val(v[ix["strike"]]), val(v[ix["bid_iv"]]),
                     val(v[ix["ask_iv"]]), val(v[ix["mid_iv"]]), v[ix["included"]] == "true"});
    } catch (...) {
    }
  }
  return out;
}

CalibrationResult calibrate(const std::vector<Quote>& quotes) {
  CalibrationResult out;
  out.path = CalibrationPath::cold;
  const auto started = Clock::now();
  std::uint64_t bound_calls = 0;
  detail::MmBoundCallScope bound_call_scope(&bound_calls);
  const auto grouping_started = Clock::now();
  const auto groups = groupQuotes(quotes);
  out.timings.quote_grouping_seconds = secondsSince(grouping_started);
  if (groups.expiries.empty()) {
    out.error = "no included quotes";
    finishResult(out, started, bound_calls);
    return out;
  }
  out.reuse_ages.reserve(groups.expiries.size());

  // Avoid thread startup for one-expiry requests; multi-slice chains reuse this pool.
  RhoWorkerPool rho_workers(groups.expiries.size() > 1);
  const auto fitting_started = Clock::now();
  for (std::size_t i = 0; i < groups.expiries.size(); ++i) {
    const double t = groups.expiries[i];
    const auto prepared = prepareQuotes(groups.slices[i]);
    const Slice* prev = out.surface.slices.empty() ? nullptr : &out.surface.slices.back();
    auto part = fitSlice(prepared, t, prev, rho_workers);
    out.iterations += part.iterations;
    out.objective += part.objective < INFINITY ? part.objective : 0;
    if (!part.converged) {
      out.timings.slice_fitting_seconds = secondsSince(fitting_started);
      out.error = part.error.empty() ? "slice fit failed" : part.error;
      finishResult(out, started, bound_calls);
      return out;
    }
    out.surface.slices.push_back(part.surface.slices.front());
    out.reuse_ages.push_back(0);
    ++out.slices_refit;
  }
  out.timings.slice_fitting_seconds = secondsSince(fitting_started);
  const auto validation_started = Clock::now();
  out.error = validate(out.surface);
  out.timings.validation_seconds = secondsSince(validation_started);
  out.converged = out.error.empty();
  finishResult(out, started, bound_calls);
  return out;
}

CalibrationResult calibrateWarm(const std::vector<Quote>& current_quotes,
                                const CalibrationResult* previous_result,
                                const std::vector<Quote>& previous_quotes,
                                const WarmOptions& options) {
  CalibrationResult out;
  out.path = CalibrationPath::warm;
  out.loss_tolerance = options.loss_tol;
  const auto started = Clock::now();
  std::uint64_t bound_calls = 0;
  detail::MmBoundCallScope bound_call_scope(&bound_calls);
  const auto finish = [&] { finishResult(out, started, bound_calls); };
  if (!std::isfinite(options.quote_tol) || options.quote_tol < 0.0 ||
      !std::isfinite(options.quote_cap) || options.quote_cap < 0.0) {
    out.error = "invalid warm calibration options";
    finish();
    return out;
  }

  const bool has_previous = previous_result && previous_result->converged &&
                            !previous_result->surface.slices.empty();
  const Surface empty_previous;
  const Surface& previous_surface =
      has_previous ? previous_result->surface : empty_previous;
  const auto grouping_started = Clock::now();
  const auto current_groups = groupQuotes(current_quotes);
  QuoteGroups previous_groups;
  if (has_previous) previous_groups = groupQuotes(previous_quotes);
  out.timings.quote_grouping_seconds = secondsSince(grouping_started);
  if (current_groups.expiries.empty()) {
    out.error = "no included quotes";
    finish();
    return out;
  }
  out.reuse_ages.reserve(current_groups.expiries.size());

  RhoWorkerPool rho_workers(current_groups.expiries.size() > 1);
  bool used_warm_path = false;
  bool used_fallback = false;
  const auto fitting_started = Clock::now();
  {
      struct WarmCandidate {
        std::vector<PreparedQuote> prepared;
        const Slice* warm_start{};
        const Slice* old_previous{};
        double loss_scale{};
        Slice candidate{};
        double candidate_loss{INFINITY};
        bool have_candidate{};
        bool candidate_reused{};
        bool candidate_from_warm{};
        bool local_attempted{};
        CalibrationResult local;
        std::uint64_t local_bound_calls{};
        CalibrationResult full_cold_reference;
        bool full_cold_reference_attempted{};
      };
      std::vector<WarmCandidate> candidates(current_groups.expiries.size());
      const auto previousReuseAge = [&](double expiry) {
        if (!has_previous) return std::size_t{0};
        for (std::size_t i = 0; i < previous_surface.slices.size(); ++i) {
          if (previous_surface.slices[i].expiry != expiry) continue;
          return i < previous_result->reuse_ages.size()
                     ? previous_result->reuse_ages[i]
                     : std::size_t{0};
        }
        return std::size_t{0};
      };
      const auto previousSliceBefore = [&](double expiry) -> const Slice* {
        const Slice* previous = nullptr;
        for (const auto& slice : previous_surface.slices) {
          if (slice.expiry < expiry &&
              (!previous || slice.expiry > previous->expiry))
            previous = &slice;
        }
        return previous;
      };
      // One full-cold fit per slice and current snapshot serves all gate decisions.
      const auto fullColdReference = [&](WarmCandidate& plan, double expiry,
                                         const Slice* previous)
          -> const CalibrationResult& {
        if (!plan.full_cold_reference_attempted) {
          plan.full_cold_reference_attempted = true;
          plan.full_cold_reference =
              fitSlice(plan.prepared, expiry, previous, rho_workers);
          out.iterations += plan.full_cold_reference.iterations;
        }
        return plan.full_cold_reference;
      };
  
      for (std::size_t i = 0; i < current_groups.expiries.size(); ++i) {
        const double expiry = current_groups.expiries[i];
        auto& plan = candidates[i];
        plan.prepared = prepareQuotes(current_groups.slices[i]);
        plan.loss_scale = bandLossScale(plan.prepared);
        plan.warm_start = surfaceSliceAt(previous_surface, expiry);
        plan.old_previous = previousSliceBefore(expiry);
        if (!plan.warm_start) continue;
  
        const auto* old_quote_group = quoteGroupAt(previous_groups, expiry);
        const bool quotes_unchanged =
            old_quote_group &&
            quoteGroupReusable(current_groups.slices[i], *old_quote_group,
                               options.quote_tol);
        const std::size_t reuse_age = previousReuseAge(expiry);
        if (quotes_unchanged && reuse_age < 12) {
          BoundMemo safe_bound(false);
          const auto calendar = calendarBounds(plan.old_previous, expiry,
                                               plan.warm_start->rho);
          Slice reused = *plan.warm_start;
          reused.expiry = expiry;
          if (pairSafe(reused, safe_bound, calendar)) {
            const double loss = bandLoss(reused, plan.prepared);
            if (std::isfinite(loss) &&
                passesQuoteCap(reused, plan.prepared, options.quote_cap)) {
              plan.candidate = reused;
              plan.candidate_loss = loss;
              plan.have_candidate = true;
              plan.candidate_reused = true;
              plan.candidate_from_warm = true;
            }
          }
        }
  
        if (!plan.have_candidate && reuse_age < 12) {
          BoundMemo safe_bound(false);
          Slice prior = *plan.warm_start;
          prior.expiry = expiry;
          const auto calendar = calendarBounds(plan.old_previous, expiry,
                                               prior.rho);
          if (pairSafe(prior, safe_bound, calendar) &&
              passesQuoteCap(prior, plan.prepared, options.quote_cap)) {
            plan.candidate = prior;
            plan.candidate_loss = bandLoss(prior, plan.prepared);
            plan.have_candidate = std::isfinite(plan.candidate_loss);
            plan.candidate_reused = plan.have_candidate;
            plan.candidate_from_warm = plan.have_candidate;
          }
        }
      }
  
      // Jacobi prefit: changed slices see only their previous-snapshot predecessor.
      {
          std::size_t local_fit_count = 0;
          for (const auto& plan : candidates)
            local_fit_count += plan.warm_start && !plan.have_candidate;
          std::vector<std::size_t> local_indices;
          if (local_fit_count != 0) {
            local_indices.reserve(local_fit_count);
            for (std::size_t i = 0; i < candidates.size(); ++i) {
              const auto& plan = candidates[i];
              if (plan.warm_start && !plan.have_candidate)
                local_indices.push_back(i);
            }
            rho_workers.parallelFor(
                local_indices.size(), [&](std::size_t begin, std::size_t end) {
                  for (std::size_t task = begin; task < end; ++task) {
                    const std::size_t i = local_indices[task];
                    auto& plan = candidates[i];
                    plan.local_attempted = true;
                    {
                      detail::MmBoundCallScope local_bound_scope(
                          &plan.local_bound_calls);
                      plan.local = fitSlice(plan.prepared, current_groups.expiries[i],
                                            plan.old_previous, rho_workers,
                                            plan.warm_start);
                    }
                    if (plan.local.converged) {
                      plan.candidate = plan.local.surface.slices.front();
                      plan.candidate_loss = plan.local.objective;
                      plan.have_candidate = true;
                      plan.candidate_from_warm = true;
                    }
                  }
                });
          }
        }
      for (const auto& plan : candidates) {
        if (!plan.local_attempted) continue;
        ++out.local_solver_slices;
        out.local_objective_evaluations += plan.local.objective_evaluations;
        out.local_mm_bound_calls += plan.local_bound_calls;
        bound_calls += plan.local_bound_calls;
        out.iterations += plan.local.iterations;
      }
  
      for (std::size_t i = 0; i < current_groups.expiries.size(); ++i) {
        const double expiry = current_groups.expiries[i];
        auto& plan = candidates[i];
        const Slice* previous_slice =
            out.surface.slices.empty() ? nullptr : &out.surface.slices.back();
        const Slice* warm_start = plan.warm_start;
        Slice candidate = plan.candidate;
        double candidate_loss = plan.candidate_loss;
        bool have_candidate = plan.have_candidate;
        bool candidate_reused = plan.candidate_reused;
        bool candidate_from_warm = plan.candidate_from_warm;
  
        bool calendar_repair_needed =
            have_candidate &&
            !calendarSafe(candidate,
                          calendarBounds(previous_slice, expiry, candidate.rho));
                
  
        // Repair only candidates made infeasible by the updated predecessor.
        
        if (calendar_repair_needed && warm_start) {
          ++out.local_solver_slices;
          std::uint64_t local_bound_calls = 0;
          CalibrationResult local;
          {
            detail::MmBoundCallScope local_bound_scope(&local_bound_calls);
            local = fitSlice(plan.prepared, expiry, previous_slice, rho_workers,
                             warm_start);
          }
          out.local_objective_evaluations += local.objective_evaluations;
          out.local_mm_bound_calls += local_bound_calls;
          bound_calls += local_bound_calls;
          out.iterations += local.iterations;
          if (local.converged) {
            candidate = local.surface.slices.front();
            candidate_loss = local.objective;
            have_candidate = true;
            candidate_reused = false;
            candidate_from_warm = true;
          } else {
            have_candidate = false;
          }
        }
  
        const bool candidate_fails_cap =
            candidate_from_warm && have_candidate &&
            !passesQuoteCap(candidate, plan.prepared, options.quote_cap);
        const bool needs_restricted = warm_start &&
                                     (!have_candidate || candidate_fails_cap);
        if (needs_restricted) {
          const auto restricted = fitSlice(plan.prepared, expiry, previous_slice,
                                           rho_workers, nullptr, warm_start);
          out.iterations += restricted.iterations;
          if (restricted.converged) {
            const Slice restricted_slice = restricted.surface.slices.front();
            if (passesQuoteCap(restricted_slice, plan.prepared,
                               options.quote_cap)) {
              candidate = restricted_slice;
              candidate_loss = restricted.objective;
              have_candidate = true;
              candidate_reused = false;
              candidate_from_warm = false;
              used_fallback = true;
              ++out.fallback_slices;
            } else {
              ++out.quality_checks;
              out.quality_warm_loss += bandLoss(restricted_slice, plan.prepared);
              out.quality_loss_scale += plan.loss_scale;
              const auto& cold =
                  fullColdReference(plan, expiry, previous_slice);
              if (!cold.converged) {
                out.timings.slice_fitting_seconds =
                    secondsSince(fitting_started);
                out.error = "cold quality reference failed at t=" +
                            std::to_string(expiry);
                out.path = used_fallback ? CalibrationPath::fallback
                                         : CalibrationPath::warm;
                finish();
                return out;
              }
              const Slice& cold_slice = cold.surface.slices.front();
              out.quality_reference_loss +=
                  bandLoss(cold_slice, plan.prepared);
              if (passesAdaptiveQuoteCap(restricted_slice, cold_slice,
                                         plan.prepared, options.quote_cap)) {
                candidate = restricted_slice;
                candidate_loss = restricted.objective;
              } else {
                candidate = cold_slice;
                candidate_loss = cold.objective;
              }
              have_candidate = true;
              candidate_reused = false;
              candidate_from_warm = false;
              used_fallback = true;
              ++out.fallback_slices;
            }
          } else {
            const auto& cold =
                fullColdReference(plan, expiry, previous_slice);
            if (!cold.converged) {
              out.timings.slice_fitting_seconds = secondsSince(fitting_started);
              out.error = cold.error.empty() ? "slice fit failed" : cold.error;
              out.path = CalibrationPath::fallback;
              finish();
              return out;
            }
            candidate = cold.surface.slices.front();
            candidate_loss = cold.objective;
            have_candidate = true;
            candidate_reused = false;
            candidate_from_warm = false;
            used_fallback = true;
            ++out.fallback_slices;
          }
        } else if (!have_candidate) {
          const auto& cold = fullColdReference(plan, expiry, previous_slice);
          if (!cold.converged) {
            out.timings.slice_fitting_seconds = secondsSince(fitting_started);
            out.error = cold.error.empty() ? "slice fit failed" : cold.error;
            out.path = CalibrationPath::cold;
            finish();
            return out;
          }
          candidate = cold.surface.slices.front();
          candidate_loss = cold.objective;
          have_candidate = true;
          candidate_reused = false;
          candidate_from_warm = false;
        }
  
        if (!have_candidate || !std::isfinite(candidate_loss)) {
          out.timings.slice_fitting_seconds = secondsSince(fitting_started);
          out.error = "no finite feasible candidate at t=" +
                      std::to_string(expiry);
          out.path = used_fallback ? CalibrationPath::fallback
                                   : CalibrationPath::warm;
          finish();
          return out;
        }
  
        out.surface.slices.push_back(candidate);
        out.reuse_ages.push_back(candidate_reused
                                     ? previousReuseAge(expiry) + 1
                                     : 0);
        out.objective += candidate_loss;
        if (candidate_reused)
          ++out.slices_reused;
        else
          ++out.slices_refit;
        used_warm_path = used_warm_path || candidate_from_warm;
      }
    }
  out.timings.slice_fitting_seconds = secondsSince(fitting_started);

  const auto validation_started = Clock::now();
  out.error = validate(out.surface);
  out.timings.validation_seconds = secondsSince(validation_started);
  out.converged = out.error.empty();
  if (!out.converged) {
    out.surface.slices.clear();
    out.reuse_ages.clear();
  }
  out.path = used_fallback ? CalibrationPath::fallback
                           : (used_warm_path ? CalibrationPath::warm
                                             : CalibrationPath::cold);
  finish();
  return out;
}
void writeParameters(const CalibrationResult& r, const std::string& p) {
  std::ofstream f(p);
  f << std::fixed << std::setprecision(17);
  f << "time_to_expiry,theta,psi,rho,phi,butterfly_safe,calendar_safe\n";
  for (size_t i = 0; i < r.surface.slices.size(); ++i) {
    const auto& s = r.surface.slices[i];
    f << s.expiry << ',' << s.theta << ',' << s.psi << ',' << s.rho << ',' << s.psi / s.theta
      << ',' << (butterflySafe(s) ? "true" : "false") << ','
      << (!i || calendarSafe(r.surface.slices[i - 1], s) ? "true" : "false") << '\n';
  }
}
void writeDiagnostics(const CalibrationResult& r, const std::string& p) {
  std::ofstream f(p);
  f << std::fixed << std::setprecision(17);
  f << "model,converged,objective,iterations,runtime_seconds,error\n";
  f << "eSSVI," << (r.converged ? "true" : "false") << ',' << r.objective << ',' << r.iterations
    << ',' << r.runtime_seconds << ',' << r.error << '\n';
}
}  // namespace essvi
