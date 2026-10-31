#include "calibrator.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
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
constexpr double kCalendarHeadroom = 0.005;
constexpr std::size_t kMaxCalibrationThreads = ESSVI_MAX_CALIBRATION_THREADS;
constexpr std::size_t kFullColdSearchBudget = 8;
constexpr std::size_t kDegradedAgeBeforeForcedCold = 3;
// WP-B7 pooled fast tilt p95 times the mean absolute loading of the uniform
// log-moneyness shape on [-0.5, 0.5]; see docs/perf-results.md, WP-B4.
constexpr double kFastScenarioMeanAbsoluteMove =
    0.5 * 0.20680703819458962;
using Clock = std::chrono::steady_clock;

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> r;
  std::stringstream x(s);
  std::string v;
  while (std::getline(x, v, ',')) r.push_back(v);
  if (!r.empty() && !r.back().empty() && r.back().back() == '\r')
    r.back().pop_back();
  if (!s.empty() && s.back() == ',') r.emplace_back();
  return r;
}

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
        !sameValue(a.mid_iv, b.mid_iv) || !sameValue(a.weight, b.weight) ||
        !movementBelowTolerance(a.bid_iv, b.bid_iv, quote_tol) ||
        !movementBelowTolerance(a.ask_iv, b.ask_iv, quote_tol))
      return false;
  }
  return true;
}

double meanAbsoluteMidIvMove(const QuoteGroups& current,
                             const QuoteGroups& previous) {
  double absolute_move_sum = 0.0;
  std::size_t matched_quotes = 0;
  for (std::size_t group = 0; group < current.expiries.size(); ++group) {
    const auto* previous_quotes =
        quoteGroupAt(previous, current.expiries[group]);
    if (!previous_quotes || previous_quotes->size() != current.slices[group].size())
      continue;
    for (std::size_t quote = 0; quote < current.slices[group].size(); ++quote) {
      const auto& now = *current.slices[group][quote];
      const auto& before = *(*previous_quotes)[quote];
      if (now.id != before.id || !std::isfinite(now.mid_iv) ||
          !std::isfinite(before.mid_iv))
        continue;
      absolute_move_sum += std::abs(now.mid_iv - before.mid_iv);
      ++matched_quotes;
    }
  }
  return matched_quotes ? absolute_move_sum / matched_quotes : 0.0;
}

struct PreparedQuote {
  double k{};
  double weight{1.0};
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
    prepared.push_back({k, q.weight, q.expiry * q.bid_iv * q.bid_iv,
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
    scale += quote.weight * quote.weight * width * width;
  }
  return scale;
}
double bandLoss(const Slice& s, const std::vector<PreparedQuote>& quotes) {
  double z = 0;
  for (const auto& q : quotes) {
    if (!q.has_band) continue;
    const double w = sliceTotalVariance(s, q.k);
    const double d = w - std::clamp(w, q.bid_variance, q.ask_variance);
    z += q.weight * q.weight * d * d;
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
    const double ratio = quote.weight * quoteExcessRatio(slice, quote);
    if (!std::isfinite(ratio) || ratio > cap) return false;
  }
  return true;
}

bool passesAdaptiveQuoteCap(const Slice& candidate, const Slice& cold,
                            const std::vector<PreparedQuote>& quotes,
                            double cap) {
  for (const auto& quote : quotes) {
    if (!quote.has_band) continue;
    const double candidate_ratio = quote.weight * quoteExcessRatio(candidate, quote);
    const double cold_ratio = quote.weight * quoteExcessRatio(cold, quote);
    if (!detail::passesAdaptiveWeightedQuoteCap(candidate_ratio, cold_ratio, quote.weight, cap)) return false;
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

CalendarBounds calendarBounds(const Slice* previous, double expiry, double rho,
                              double headroom = kCalendarHeadroom) {
  if (!previous) return {};
  const double ratio =
      std::max((1 + previous->rho) / (1 + rho),
               (1 - previous->rho) / (1 - rho));
  return {true,
          expiry > previous->expiry,
          previous->psi > kEps,
          previous->theta + kEps,
          previous->psi + kEps,
          previous->psi * ratio * (1.0 + headroom) + kEps,
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

bool boundSafe(const Slice& s, BoundMemo& bound) {
  const double ar = std::abs(s.rho);
  if (!(s.theta > kEps && s.psi > kEps && ar < 1.0 - kEps &&
        s.psi * (1.0 + ar) < 4.0 - kEps)) return false;
  return s.psi * s.psi <= bound.get(s.theta, ar) - kEps;
}

bool pairSafe(const Slice& s, BoundMemo& bound,
              const CalendarBounds& calendar) {
  return boundSafe(s, bound) && calendarSafe(s, calendar);
}

struct Atm {
  double theta_star{NAN};
  double k_star{0};
  const PreparedQuote* quote{nullptr};
};

Atm nearestAtm(const std::vector<PreparedQuote>& quotes, double t) {
  Atm a;
  double best = INFINITY;
  for (const auto& q : quotes) {
    if (!q.has_mid) continue;
    const double abs_k = std::abs(q.k);
    if (abs_k < best) {
      best = abs_k;
      a.k_star = q.k;
      a.quote = &q;
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

CalibrationResult fitSliceAtHeadroom(const std::vector<PreparedQuote>& qs, double t,
                           const Slice* prev, RhoWorkerPool& rho_workers,
                           const Slice* warm_start,
                           const Slice* restricted_center,
                           double quote_cap, double calendar_headroom) {
  CalibrationResult out;
  const auto atm = [&] {
    Atm result = nearestAtm(qs, t);
    const auto* quote = result.quote;
    if (warm_start && quote && quote->has_band && t > 0.0 &&
        quote->bid_variance <= quote->ask_variance) {
      const double anchor_iv =
          0.5 * (std::sqrt(quote->bid_variance / t) +
                 std::sqrt(quote->ask_variance / t));
      if (anchor_iv > 0.0 && std::isfinite(anchor_iv))
        result.theta_star = t * anchor_iv * anchor_iv;
    }
    return result;
  }();
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
      const auto calendar = calendarBounds(prev, t, rho, calendar_headroom);
      const double lo = std::max(psiLower(prev, rho), calendar.ratio_lower);
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

      const double parameter_scale[3] = {
          std::max(slice.theta * 0.1, 1e-3), 0.1,
          std::max(slice.psi * 0.1, 1e-3)};
      double loss = 0.0;
      double maximum_excess_ratio = 0.0;
      for (const auto& quote : qs) {
        if (!quote.has_band) continue;
        const double weight_squared = quote.weight * quote.weight;
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
        loss += weight_squared * residual * residual;
        const double quote_width =
            std::max(quote.ask_variance - quote.bid_variance, 1e-6);
        const double excess_ratio = quote.weight * std::abs(residual) / quote_width;
        maximum_excess_ratio =
            std::isfinite(excess_ratio)
                ? std::max(maximum_excess_ratio, excess_ratio)
                : INFINITY;
        if (residual == 0.0) continue;
        for (int row = 0; row < 3; ++row) {
          gradient[row] += weight_squared * scaled_jacobian[row] * residual;
          for (int column = 0; column < 3; ++column)
            normal[row][column] += weight_squared * scaled_jacobian[row] *
                                   scaled_jacobian[column];
        }
      }
      return std::pair<double, double>{loss, maximum_excess_ratio};
    };

    // Keep the best raw-loss candidate and the best gate-passing candidate separately.
    Slice best_quote_cap_candidate{};
    double best_quote_cap_loss = INFINITY;
    bool have_quote_cap_candidate = false;
    const auto considerLocalCandidate = [&](const Slice& candidate, double loss,
                                            double maximum_excess_ratio) {
      if (loss < bestz) {
        bestz = loss;
        best = candidate;
      }
      if (std::isfinite(maximum_excess_ratio) &&
          !(maximum_excess_ratio > quote_cap) && loss < best_quote_cap_loss) {
        best_quote_cap_loss = loss;
        best_quote_cap_candidate = candidate;
        have_quote_cap_candidate = true;
      }
    };

    Slice incumbent = *warm_start;
    incumbent.expiry = t;
    BoundMemo incumbent_bound(false);
    const auto incumbent_calendar =
        calendarBounds(prev, t, incumbent.rho, calendar_headroom);
    if (pairSafe(incumbent, incumbent_bound, incumbent_calendar)) {
      bestz = bandLoss(incumbent, qs);
      best = incumbent;
      if (passesQuoteCap(incumbent, qs, quote_cap)) {
        best_quote_cap_loss = bestz;
        best_quote_cap_candidate = incumbent;
        have_quote_cap_candidate = true;
      }
      ++evals;
    }

    constexpr std::array<double, 3> rho_offsets{0.0, -0.05, 0.05};
    constexpr std::array<double, 5> alphas{1.0, 0.5, 0.25, 0.125, 0.0625};
    constexpr int local_evaluation_budget = 25;
    // Starts remain within rho ±0.05; LM may traverse the staged ±0.15
    // fallback radius before resorting to a cold search.
    const double rho_lower = std::max(-0.979999, warm_start->rho - 0.15);
    const double rho_upper = std::min(0.979999, warm_start->rho + 0.15);
    const double theta_min = prev ? prev->theta + kEps : kEps;

    for (std::size_t start_index = 0; start_index < rho_offsets.size();
         ++start_index) {
      if (evals >= local_evaluation_budget) break;
      const double rho = std::clamp(warm_start->rho + rho_offsets[start_index],
                                    rho_lower, rho_upper);
      BoundMemo candidate_bound(true);
      const auto calendar = calendarBounds(prev, t, rho, calendar_headroom);
      const double psi_min =
          std::max(psiLower(prev, rho), calendar.ratio_lower);
      const double theta_seed =
          prev ? std::max(atm.theta_star, prev->theta + kEps)
               : atm.theta_star;
      const double psi_max = psiUpper(theta_seed, rho, candidate_bound);
      if (!(psi_max > psi_min)) {
        ++out.fallback_reason_counts.bound_check_failed;
        continue;
      }

      const double psi_fraction =
          start_index == 1 ? 0.25 : (start_index == 2 ? 0.75 : 0.0);
      const double psi_seed = start_index == 0
                                  ? warm_start->psi
                                  : psi_min + psi_fraction * (psi_max - psi_min);
      const double psi = std::clamp(psi_seed, psi_min, psi_max);
      Slice current = anchored(t, atm, rho, psi, prev, candidate_bound);
      if (!boundSafe(current, safe_bound)) {
        ++out.fallback_reason_counts.bound_check_failed;
        continue;
      }
      if (!calendarSafe(current, calendar)) continue;

      ++out.local_solver_starts;
      double normal[3][3];
      double gradient[3];
      const auto current_evaluation =
          lossAndNormal(current, normal, gradient);
      double current_loss = current_evaluation.first;
      considerLocalCandidate(current, current_loss, current_evaluation.second);
      ++evals;
      if (current_loss == 0.0) break;

      const int starts_remaining =
          static_cast<int>(rho_offsets.size() - start_index);
      const int start_evaluation_limit =
          evals + (local_evaluation_budget - evals) / starts_remaining;
      double damping = 1e-6;
      for (int iteration = 0;
           iteration < 12 && evals < start_evaluation_limit &&
           theta_min < 5.0 && current_loss > 0.0;
           ++iteration) {
        double matrix[3][4]{};
        for (int row = 0; row < 3; ++row) {
          for (int column = 0; column < 3; ++column)
            matrix[row][column] = normal[row][column];
          matrix[row][row] += damping * std::max(normal[row][row], 1e-12);
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
        if (!solvable) {
          damping *= 10.0;
          if (damping > 1e12) break;
          continue;
        }

        const double parameter_scale[3] = {
            std::max(current.theta * 0.1, 1e-3), 0.1,
            std::max(current.psi * 0.1, 1e-3)};
        Slice selected = current;
        double selected_loss = current_loss;
        double selected_normal[3][3];
        double selected_gradient[3];
        bool found_improvement = false;
        for (const double alpha : alphas) {
          if (evals >= start_evaluation_limit) break;
          Slice trial = current;
          trial.theta = std::clamp(
              current.theta + alpha * parameter_scale[0] * matrix[0][3],
              theta_min, 5.0);
          trial.rho = std::clamp(
              current.rho + alpha * parameter_scale[1] * matrix[1][3],
              rho_lower, rho_upper);
          const auto trial_calendar =
              calendarBounds(prev, t, trial.rho, calendar_headroom);
          const double trial_psi_min =
              std::max(psiLower(prev, trial.rho), trial_calendar.ratio_lower);
          const double trial_psi_max =
              psiUpper(trial.theta, trial.rho, candidate_bound);
          if (!(trial_psi_max > trial_psi_min)) {
            ++out.fallback_reason_counts.bound_check_failed;
            continue;
          }
          trial.psi = std::clamp(
              current.psi + alpha * parameter_scale[2] * matrix[2][3],
              trial_psi_min, trial_psi_max);
          if (!boundSafe(trial, safe_bound)) {
            ++out.fallback_reason_counts.bound_check_failed;
            continue;
          }
          if (!calendarSafe(trial, trial_calendar)) continue;

          double trial_normal[3][3];
          double trial_gradient[3];
          const auto trial_evaluation =
              lossAndNormal(trial, trial_normal, trial_gradient);
          const double trial_loss = trial_evaluation.first;
          considerLocalCandidate(trial, trial_loss, trial_evaluation.second);
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
        if (!found_improvement) {
          damping *= 10.0;
          if (damping > 1e12) break;
          continue;
        }

        current = selected;
        current_loss = selected_loss;
        for (int row = 0; row < 3; ++row) {
          gradient[row] = selected_gradient[row];
          for (int column = 0; column < 3; ++column)
            normal[row][column] = selected_normal[row][column];
        }
        damping = std::max(damping * 0.3, 1e-12);
        if (current_loss < bestz) {
          bestz = current_loss;
          best = current;
        }
      }
      if (bestz == 0.0) break;
    }
    if (have_quote_cap_candidate) {
      bestz = best_quote_cap_loss;
      best = best_quote_cap_candidate;
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
    const auto calendar = calendarBounds(prev, t, best.rho, calendar_headroom);
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
CalibrationResult fitSlice(const std::vector<PreparedQuote>& qs, double t,
                           const Slice* prev, RhoWorkerPool& rho_workers,
                           const Slice* warm_start = nullptr,
                           const Slice* restricted_center = nullptr,
                           double quote_cap = INFINITY,
                           double* used_calendar_headroom = nullptr) {
  if (used_calendar_headroom) *used_calendar_headroom = 0.0;
  auto strict = fitSliceAtHeadroom(qs, t, prev, rho_workers, warm_start,
                                   restricted_center, quote_cap, 0.0);
  if (!prev || !strict.converged) return strict;

  const Slice& strict_candidate = strict.surface.slices.front();
  if (calendarSafe(strict_candidate,
                   calendarBounds(prev, t, strict_candidate.rho))) {
    if (used_calendar_headroom)
      *used_calendar_headroom = kCalendarHeadroom;
    return strict;
  }

  auto with_headroom = fitSliceAtHeadroom(
      qs, t, prev, rho_workers, warm_start, restricted_center, quote_cap,
      kCalendarHeadroom);
  if (with_headroom.converged) {
    if (used_calendar_headroom)
      *used_calendar_headroom = kCalendarHeadroom;
    return with_headroom;
  }
  return strict;
}

bool hasInvalidIncludedWeight(const std::vector<Quote>& quotes) {
  for (const auto& quote : quotes)
    if (quote.included &&
        (!std::isfinite(quote.weight) || quote.weight <= 0.0))
      return true;
  return false;
}

void finishResult(CalibrationResult& result, Clock::time_point started,
                  std::uint64_t bound_calls) {
  result.objective_evaluations = static_cast<std::uint64_t>(result.iterations);
  result.mm_bound_calls = bound_calls;
  result.runtime_seconds = secondsSince(started);
}

using StateMatrix = std::array<std::array<double, 3>, 3>;
using StateCovariance = std::array<double, 6>;
constexpr double kColdStateVariance = 0.25;

StateMatrix unpackCovariance(const StateCovariance& packed) {
  StateMatrix matrix{};
  matrix[0][0] = packed[0];
  matrix[0][1] = matrix[1][0] = packed[1];
  matrix[0][2] = matrix[2][0] = packed[2];
  matrix[1][1] = packed[3];
  matrix[1][2] = matrix[2][1] = packed[4];
  matrix[2][2] = packed[5];
  return matrix;
}

bool cholesky(const StateMatrix& matrix, StateMatrix& lower) {
  lower = {};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column <= row; ++column) {
      double value = matrix[row][column];
      if (!std::isfinite(value)) return false;
      for (int index = 0; index < column; ++index)
        value -= lower[row][index] * lower[column][index];
      if (row == column) {
        if (!(value > 0.0) || !std::isfinite(value)) return false;
        lower[row][column] = std::sqrt(value);
      } else {
        lower[row][column] = value / lower[column][column];
        if (!std::isfinite(lower[row][column])) return false;
      }
    }
  }
  return true;
}

bool validCovariance(const StateCovariance& covariance) {
  StateMatrix lower{};
  return cholesky(unpackCovariance(covariance), lower);
}

StateCovariance coldCovariance() {
  return {kColdStateVariance, 0.0, 0.0, kColdStateVariance, 0.0,
          kColdStateVariance};
}

const StateCovariance* posteriorCovarianceAt(
    const CalibrationResult* result, double expiry) {
  if (!result || !result->converged ||
      result->posterior_covariances.size() != result->surface.slices.size())
    return nullptr;
  for (std::size_t index = 0; index < result->surface.slices.size(); ++index) {
    if (result->surface.slices[index].expiry != expiry) continue;
    const auto& covariance = result->posterior_covariances[index];
    return validCovariance(covariance) ? &covariance : nullptr;
  }
  return nullptr;
}
}  // namespace

std::vector<Quote> readQuotes(const std::string& path) {
  std::ifstream file(path);
  if (!file) throw std::runtime_error(path + ": cannot open quote CSV");
  std::string line;
  if (!std::getline(file, line)) {
    if (file.bad()) throw std::runtime_error(path + ": cannot read quote CSV");
    throw std::invalid_argument(path + ": missing quote CSV header");
  }
  const auto header = split(line);
  constexpr std::array<const char*, 9> required_columns{
      "quote_id", "time_to_expiry", "forward", "discount_factor", "strike",
      "bid_iv", "ask_iv", "mid_iv", "included"};
  std::array<std::size_t, 9> columns{};
  for (std::size_t i = 0; i < required_columns.size(); ++i) {
    const auto column = std::find(header.begin(), header.end(), required_columns[i]);
    if (column == header.end())
      throw std::invalid_argument(path + ": missing required column " +
                                  required_columns[i]);
    columns[i] = static_cast<std::size_t>(column - header.begin());
  }
  const auto weight_column = std::find(header.begin(), header.end(), "weight");
  const auto weight_index = static_cast<std::size_t>(weight_column - header.begin());
  std::vector<Quote> out;
  std::size_t line_number = 1;
  while (std::getline(file, line)) {
    ++line_number;
    const auto fields = split(line);
    const auto rowError = [&](const std::string& message) {
      return std::invalid_argument(path + ":" + std::to_string(line_number) +
                                   ": " + message);
    };
    if (fields.size() != header.size())
      throw rowError("expected " + std::to_string(header.size()) + " columns");
    const auto& included_text = fields[columns[8]];
    if (included_text != "true" && included_text != "false")
      throw rowError("invalid included flag");
    const bool included = included_text == "true";
    const auto readNumber = [&](std::size_t column) -> double {
      const auto& text = fields[columns[column]];
      if (text.empty() && !included) return NAN;
      std::size_t parsed = 0;
      double value;
      try {
        value = std::stod(text, &parsed);
      } catch (const std::logic_error&) {
        throw rowError(std::string("invalid ") + required_columns[column]);
      }
      if (parsed != text.size() || !std::isfinite(value))
        throw rowError(std::string("invalid ") + required_columns[column]);
      return value;
    };
    double weight = 1.0;
    if (weight_index < header.size() && !fields[weight_index].empty()) {
      const auto& text = fields[weight_index];
      try {
        std::size_t parsed = 0;
        weight = std::stod(text, &parsed);
        if (parsed != text.size()) weight = NAN;
      } catch (const std::logic_error&) {
        weight = NAN;
      }
    }
    out.push_back({fields[columns[0]], readNumber(1), readNumber(2),
                   readNumber(3), readNumber(4), readNumber(5), readNumber(6),
                   readNumber(7), included, weight});
  }
  if (file.bad()) throw std::runtime_error(path + ": cannot read quote CSV");
  return out;
}

CalibrationResult calibrate(const std::vector<Quote>& quotes) {
  CalibrationResult out;
  out.path = CalibrationPath::cold;
  const auto started = Clock::now();
  std::uint64_t bound_calls = 0;
  detail::MmBoundCallScope bound_call_scope(&bound_calls);
  if (hasInvalidIncludedWeight(quotes)) {
    out.error = "included quote weight must be finite and positive";
    finishResult(out, started, bound_calls);
    return out;
  }
  const auto grouping_started = Clock::now();
  const auto groups = groupQuotes(quotes);
  out.timings.quote_grouping_seconds = secondsSince(grouping_started);
  if (groups.expiries.empty()) {
    out.error = "no included quotes";
    finishResult(out, started, bound_calls);
    return out;
  }
  out.reuse_ages.reserve(groups.expiries.size());
  out.posterior_covariances.reserve(groups.expiries.size());

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
    out.posterior_covariances.push_back(coldCovariance());
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
  if (hasInvalidIncludedWeight(current_quotes)) {
    out.error = "included quote weight must be finite and positive";
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
  out.posterior_covariances.reserve(current_groups.expiries.size());
  out.degraded_ages.reserve(current_groups.expiries.size());

  RhoWorkerPool rho_workers(current_groups.expiries.size() > 1);
  bool used_warm_path = false;
  bool used_fallback = false;
  double quality_ratio_square_sum = 0.0;
  const auto fitting_started = Clock::now();
  {
      struct WarmCandidate {
        std::vector<PreparedQuote> prepared;
        const Slice* warm_start{};
        const Slice* old_previous{};
        double loss_scale{};
        Slice candidate{};
        double candidate_loss{INFINITY};
        double candidate_headroom{kCalendarHeadroom};
        double full_cold_headroom{kCalendarHeadroom};
        bool have_candidate{};
        bool candidate_reused{};
        bool candidate_from_warm{};
        bool local_attempted{};
        CalibrationResult local;
        std::uint64_t local_bound_calls{};
        CalibrationResult full_cold_reference;
        bool full_cold_reference_attempted{};
        bool full_cold_budget_exhausted{};
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
      const auto previousDegradedAge = [&](double expiry) {
        if (!has_previous) return std::size_t{0};
        for (std::size_t i = 0; i < previous_surface.slices.size(); ++i) {
          if (previous_surface.slices[i].expiry != expiry) continue;
          return i < previous_result->degraded_ages.size()
                     ? previous_result->degraded_ages[i]
                     : std::size_t{0};
        }
        return std::size_t{0};
      };
      const bool prior_fallback_majority =
          has_previous &&
          previous_result->fallback_slices * 2 > previous_surface.slices.size();
      const double mean_absolute_move =
          has_previous ? meanAbsoluteMidIvMove(current_groups, previous_groups)
                       : 0.0;
      const bool skip_local_for_large_move =
          prior_fallback_majority &&
          mean_absolute_move >= kFastScenarioMeanAbsoluteMove;
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
          const bool forced =
              previousDegradedAge(expiry) >= kDegradedAgeBeforeForcedCold;
          if (has_previous && !forced &&
              out.full_cold_searches >= kFullColdSearchBudget) {
            plan.full_cold_budget_exhausted = true;
            ++out.fallback_reason_counts.budget_exhausted;
          } else {
            if (has_previous) {
              ++out.full_cold_searches;
              if (forced) ++out.forced_full_cold_searches;
            }
            plan.full_cold_reference = fitSlice(
                plan.prepared, expiry, previous, rho_workers, nullptr, nullptr,
                INFINITY, &plan.full_cold_headroom);
            out.iterations += plan.full_cold_reference.iterations;
          }
        }
        return plan.full_cold_reference;
      };
  
      for (std::size_t i = 0; i < current_groups.expiries.size(); ++i) {
        const double expiry = current_groups.expiries[i];
        auto& plan = candidates[i];
        plan.prepared = prepareQuotes(current_groups.slices[i]);
        plan.loss_scale = bandLossScale(plan.prepared);
        plan.warm_start =
            posteriorCovarianceAt(previous_result, expiry)
                ? surfaceSliceAt(previous_surface, expiry)
                : nullptr;
        plan.old_previous = previousSliceBefore(expiry);
        if (!plan.warm_start ||
            previousDegradedAge(expiry) >= kDegradedAgeBeforeForcedCold)
          continue;
  
        const auto* old_quote_group = quoteGroupAt(previous_groups, expiry);
        const bool quotes_unchanged =
            old_quote_group &&
            quoteGroupReusable(current_groups.slices[i], *old_quote_group,
                               options.quote_tol);
        const std::size_t reuse_age = previousReuseAge(expiry);
        if (quotes_unchanged && reuse_age < 12) {
          BoundMemo safe_bound(false);
          const auto calendar = calendarBounds(plan.old_previous, expiry,
                                               plan.warm_start->rho, 0.0);
          Slice reused = *plan.warm_start;
          reused.expiry = expiry;
          if (pairSafe(reused, safe_bound, calendar)) {
            const double loss = bandLoss(reused, plan.prepared);
            if (std::isfinite(loss) &&
                passesQuoteCap(reused, plan.prepared, options.quote_cap)) {
              plan.candidate = reused;
              plan.candidate_loss = loss;
              plan.candidate_headroom =
                  calendarSafe(reused, calendarBounds(
                                           plan.old_previous, expiry, reused.rho))
                      ? kCalendarHeadroom
                      : 0.0;
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
                                               prior.rho, 0.0);
          const bool prior_bound_safe = boundSafe(prior, safe_bound);
          if (!prior_bound_safe) {
            ++out.fallback_reason_counts.bound_check_failed;
          } else if (calendarSafe(prior, calendar)) {
            if (passesQuoteCap(prior, plan.prepared, options.quote_cap)) {
              plan.candidate = prior;
              plan.candidate_loss = bandLoss(prior, plan.prepared);
              plan.have_candidate = std::isfinite(plan.candidate_loss);
              plan.candidate_reused = plan.have_candidate;
              plan.candidate_from_warm = plan.have_candidate;
              if (plan.have_candidate)
                plan.candidate_headroom =
                    calendarSafe(prior, calendarBounds(plan.old_previous,
                                                       expiry, prior.rho))
                        ? kCalendarHeadroom
                        : 0.0;
            } else {
              ++out.fallback_reason_counts.loss_above_gate;
            }
          }
        }
      }
  
      // Jacobi prefit: changed slices see only their previous-snapshot predecessor.
      {
          std::size_t local_fit_count = 0;
          for (std::size_t i = 0; i < candidates.size(); ++i) {
            const auto& plan = candidates[i];
            local_fit_count += plan.warm_start && !plan.have_candidate &&
                               previousDegradedAge(current_groups.expiries[i]) <
                                   kDegradedAgeBeforeForcedCold &&
                               !skip_local_for_large_move;
          }
          std::vector<std::size_t> local_indices;
          if (local_fit_count != 0) {
            local_indices.reserve(local_fit_count);
            for (std::size_t i = 0; i < candidates.size(); ++i) {
              const auto& plan = candidates[i];
              if (plan.warm_start && !plan.have_candidate &&
                  previousDegradedAge(current_groups.expiries[i]) <
                      kDegradedAgeBeforeForcedCold &&
                  !skip_local_for_large_move)
                local_indices.push_back(i);
            }
            std::sort(local_indices.begin(), local_indices.end(),
                      [&](std::size_t a, std::size_t b) {
                        const auto age_a =
                            previousDegradedAge(current_groups.expiries[a]);
                        const auto age_b =
                            previousDegradedAge(current_groups.expiries[b]);
                        return age_a != age_b ? age_a > age_b : a < b;
                      });
            rho_workers.parallelFor(
                local_indices.size(), [&](std::size_t begin, std::size_t end) {
                  for (std::size_t task = begin; task < end; ++task) {
                    const std::size_t i = local_indices[task];
                    auto& plan = candidates[i];
                    plan.local_attempted = true;
                    {
                      detail::MmBoundCallScope local_bound_scope(
                          &plan.local_bound_calls);
                      plan.local = fitSlice(
                          plan.prepared, current_groups.expiries[i],
                          plan.old_previous, rho_workers, plan.warm_start,
                          nullptr, options.quote_cap,
                          &plan.candidate_headroom);
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
        out.local_solver_starts += plan.local.local_solver_starts;
        out.local_objective_evaluations += plan.local.objective_evaluations;
        out.local_mm_bound_calls += plan.local_bound_calls;
        out.fallback_reason_counts.bound_check_failed +=
            plan.local.fallback_reason_counts.bound_check_failed;
        if (!plan.local.converged)
          ++out.fallback_reason_counts.infeasible_local_result;
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
        double candidate_headroom = plan.candidate_headroom;
        bool have_candidate = plan.have_candidate;
        bool candidate_reused = plan.candidate_reused;
        bool candidate_from_warm = plan.candidate_from_warm;
  
        const auto selectBudgetFallback = [&](const Slice* restricted_slice,
                                              double restricted_loss,
                                              double restricted_headroom) {
          Slice best_slice{};
          double best_loss = INFINITY;
          double best_headroom = 0.0;
          bool found = false;
          BoundMemo safe_bound(false);
          const auto consider = [&](const Slice& possible, double loss,
                                    double headroom) {
            if (!std::isfinite(loss)) return;
            const auto calendar =
                calendarBounds(previous_slice, expiry, possible.rho, headroom);
            if (!pairSafe(possible, safe_bound, calendar)) return;
            if (!found || loss < best_loss) {
              best_slice = possible;
              best_loss = loss;
              best_headroom = headroom;
              found = true;
            }
          };
          if (restricted_slice)
            consider(*restricted_slice, restricted_loss, restricted_headroom);
          if (have_candidate)
            consider(candidate, candidate_loss, candidate_headroom);
          if (warm_start) {
            Slice prior = *warm_start;
            prior.expiry = expiry;
            consider(prior, bandLoss(prior, plan.prepared), 0.0);
          }
          if (!found) return false;
          candidate = best_slice;
          candidate_loss = best_loss;
          candidate_headroom = best_headroom;
          have_candidate = true;
          candidate_reused = false;
          candidate_from_warm = false;
          used_fallback = true;
          ++out.fallback_slices;
          return true;
        };

        const bool forced_full_cold =
            warm_start &&
            previousDegradedAge(expiry) >= kDegradedAgeBeforeForcedCold;
        if (forced_full_cold) {
          const auto& cold = fullColdReference(plan, expiry, previous_slice);
          if (!cold.converged) {
            out.timings.slice_fitting_seconds = secondsSince(fitting_started);
            out.error = cold.error.empty() ? "forced cold refit failed" : cold.error;
            out.path = CalibrationPath::fallback;
            finish();
            return out;
          }
          candidate = cold.surface.slices.front();
          candidate_loss = cold.objective;
          candidate_headroom = plan.full_cold_headroom;
          have_candidate = true;
          candidate_reused = false;
          candidate_from_warm = false;
          used_fallback = true;
          ++out.fallback_slices;
        } else {
          const bool calendar_repair_needed =
              have_candidate &&
              !calendarSafe(candidate,
                            calendarBounds(previous_slice, expiry, candidate.rho,
                                           candidate_headroom));

          // Repair only candidates made infeasible by the updated predecessor.
          if (calendar_repair_needed && warm_start) {
            ++out.local_solver_slices;
            std::uint64_t local_bound_calls = 0;
            CalibrationResult local;
            double local_headroom = kCalendarHeadroom;
            {
              detail::MmBoundCallScope local_bound_scope(&local_bound_calls);
              local = fitSlice(plan.prepared, expiry, previous_slice,
                               rho_workers, warm_start, nullptr,
                               options.quote_cap, &local_headroom);
            }
            out.local_solver_starts += local.local_solver_starts;
            out.local_objective_evaluations += local.objective_evaluations;
            out.local_mm_bound_calls += local_bound_calls;
            out.fallback_reason_counts.bound_check_failed +=
                local.fallback_reason_counts.bound_check_failed;
            bound_calls += local_bound_calls;
            out.iterations += local.iterations;
            const Slice* repaired =
                local.converged ? &local.surface.slices.front() : nullptr;
            const bool local_safe =
                repaired &&
                calendarSafe(*repaired,
                             calendarBounds(previous_slice, expiry,
                                            repaired->rho, local_headroom));
            if (local_safe && local_headroom == kCalendarHeadroom) {
              candidate = *repaired;
              candidate_loss = local.objective;
              candidate_headroom = local_headroom;
              have_candidate = true;
              candidate_reused = false;
              candidate_from_warm = true;
            } else {
              ++out.fallback_reason_counts.calendar_repair_failed;
              if (have_candidate &&
                  calendarSafe(candidate,
                               calendarBounds(previous_slice, expiry,
                                              candidate.rho, 0.0))) {
                candidate_headroom = 0.0;
              } else if (local_safe) {
                candidate = *repaired;
                candidate_loss = local.objective;
                candidate_headroom = local_headroom;
                have_candidate = true;
                candidate_reused = false;
                candidate_from_warm = true;
              } else {
                have_candidate = false;
              }
            }
          }

          const bool candidate_fails_cap =
              candidate_from_warm && have_candidate &&
              !passesQuoteCap(candidate, plan.prepared, options.quote_cap);
          if (candidate_fails_cap)
            ++out.fallback_reason_counts.loss_above_gate;
          const bool needs_restricted =
              warm_start && (!have_candidate || candidate_fails_cap);
          if (needs_restricted) {
            ++out.restricted_cold_searches;
            double restricted_headroom = kCalendarHeadroom;
            const auto restricted = fitSlice(
                plan.prepared, expiry, previous_slice, rho_workers, nullptr,
                warm_start, INFINITY, &restricted_headroom);
            out.iterations += restricted.iterations;
            if (restricted.converged) {
              const Slice restricted_slice = restricted.surface.slices.front();
              if (passesQuoteCap(restricted_slice, plan.prepared,
                                 options.quote_cap)) {
                candidate = restricted_slice;
                candidate_loss = restricted.objective;
                candidate_headroom = restricted_headroom;
                have_candidate = true;
                candidate_reused = false;
                candidate_from_warm = false;
                used_fallback = true;
                ++out.fallback_slices;
              } else {
                ++out.fallback_reason_counts.loss_above_gate;
                out.quality_warm_loss += bandLoss(restricted_slice, plan.prepared);
                out.quality_loss_scale += plan.loss_scale;
                const auto& cold =
                    fullColdReference(plan, expiry, previous_slice);
                if (plan.full_cold_budget_exhausted) {
                  if (!selectBudgetFallback(&restricted_slice,
                                            restricted.objective,
                                            restricted_headroom)) {
                    out.timings.slice_fitting_seconds =
                        secondsSince(fitting_started);
                    out.error = "budget exhausted without feasible candidate at t=" +
                                std::to_string(expiry);
                    out.path = CalibrationPath::fallback;
                    finish();
                    return out;
                  }
                } else if (!cold.converged) {
                  out.timings.slice_fitting_seconds =
                      secondsSince(fitting_started);
                  out.error = "cold quality reference failed at t=" +
                              std::to_string(expiry);
                  out.path = CalibrationPath::fallback;
                  finish();
                  return out;
                } else {
                  ++out.quality_checks;
                  const Slice& cold_slice = cold.surface.slices.front();
                  out.quality_reference_loss +=
                      bandLoss(cold_slice, plan.prepared);
                  if (passesAdaptiveQuoteCap(restricted_slice, cold_slice,
                                             plan.prepared, options.quote_cap)) {
                    candidate = restricted_slice;
                    candidate_loss = restricted.objective;
                    candidate_headroom = restricted_headroom;
                  } else {
                    candidate = cold_slice;
                    candidate_loss = cold.objective;
                    candidate_headroom = plan.full_cold_headroom;
                  }
                  have_candidate = true;
                  candidate_reused = false;
                  candidate_from_warm = false;
                  used_fallback = true;
                  ++out.fallback_slices;
                }
              }
            } else {
              const auto& cold =
                  fullColdReference(plan, expiry, previous_slice);
              if (plan.full_cold_budget_exhausted) {
                if (!selectBudgetFallback(nullptr, INFINITY, 0.0)) {
                  out.timings.slice_fitting_seconds =
                      secondsSince(fitting_started);
                  out.error = "budget exhausted without feasible candidate at t=" +
                              std::to_string(expiry);
                  out.path = CalibrationPath::fallback;
                  finish();
                  return out;
                }
              } else if (!cold.converged) {
                out.timings.slice_fitting_seconds = secondsSince(fitting_started);
                out.error = cold.error.empty() ? "slice fit failed" : cold.error;
                out.path = CalibrationPath::fallback;
                finish();
                return out;
              } else {
                candidate = cold.surface.slices.front();
                candidate_loss = cold.objective;
                candidate_headroom = plan.full_cold_headroom;
                have_candidate = true;
                candidate_reused = false;
                candidate_from_warm = false;
                used_fallback = true;
                ++out.fallback_slices;
              }
            }
          } else if (!have_candidate) {
            const auto& cold = fullColdReference(plan, expiry, previous_slice);
            if (!cold.converged) {
              out.timings.slice_fitting_seconds = secondsSince(fitting_started);
              out.error = plan.full_cold_budget_exhausted
                              ? "full-cold budget exhausted before fitting new expiry"
                              : (cold.error.empty() ? "slice fit failed"
                                                    : cold.error);
              out.path = CalibrationPath::fallback;
              finish();
              return out;
            }
            candidate = cold.surface.slices.front();
            candidate_loss = cold.objective;
            candidate_headroom = plan.full_cold_headroom;
            have_candidate = true;
            candidate_reused = false;
            candidate_from_warm = false;
          }
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
        out.posterior_covariances.push_back(coldCovariance());
        out.reuse_ages.push_back(candidate_reused
                                     ? previousReuseAge(expiry) + 1
                                     : 0);
        const std::size_t prior_degraded_age = previousDegradedAge(expiry);
        const std::size_t degraded_age =
            passesQuoteCap(candidate, plan.prepared, options.quote_cap)
                ? 0
                : prior_degraded_age + 1;
        out.degraded_ages.push_back(degraded_age);
        if (degraded_age > 0) ++out.degraded_slices;
        double slice_ratio_square_sum = 0.0;
        std::size_t slice_quote_count = 0;
        double slice_maximum_ratio = 0.0;
        for (const auto& quote : plan.prepared) {
          if (!quote.has_band) continue;
          const double ratio = quoteExcessRatio(candidate, quote);
          slice_ratio_square_sum += ratio * ratio;
          slice_maximum_ratio = std::max(slice_maximum_ratio, ratio);
          out.quality_maximum_excess_ratio =
              std::max(out.quality_maximum_excess_ratio, ratio);
          ++slice_quote_count;
          ++out.quality_quote_count;
        }
        quality_ratio_square_sum += slice_ratio_square_sum;
        out.worst_slice_max_excess_ratio =
            std::max(out.worst_slice_max_excess_ratio, slice_maximum_ratio);
        if (slice_quote_count != 0) {
          const double slice_rms = std::sqrt(
              slice_ratio_square_sum / static_cast<double>(slice_quote_count));
          out.worst_slice_rms_excess_ratio =
              std::max(out.worst_slice_rms_excess_ratio, slice_rms);
        }
        out.objective += candidate_loss;
        if (candidate_reused)
          ++out.slices_reused;
        else
          ++out.slices_refit;
        used_warm_path = used_warm_path || candidate_from_warm;
      }
      if (out.quality_quote_count != 0)
        out.quality_rms_excess_ratio =
            std::sqrt(quality_ratio_square_sum /
                      static_cast<double>(out.quality_quote_count));
    }
  out.timings.slice_fitting_seconds = secondsSince(fitting_started);

  const auto validation_started = Clock::now();
  out.error = validate(out.surface);
  out.timings.validation_seconds = secondsSince(validation_started);
  out.converged = out.error.empty();
  if (!out.converged) {
    out.surface.slices.clear();
    out.posterior_covariances.clear();
    out.reuse_ages.clear();
    out.degraded_ages.clear();
    out.degraded_slices = 0;
  }
  out.path = out.converged &&
                     out.fallback_reason_counts.budget_exhausted != 0
                 ? CalibrationPath::degraded
                 : (used_fallback ? CalibrationPath::fallback
                           : (used_warm_path ? CalibrationPath::warm
                                             : CalibrationPath::cold));
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
