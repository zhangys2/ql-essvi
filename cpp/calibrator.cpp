#include "calibrator.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace essvi {
namespace {
constexpr double kEps = 1e-8;

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> r;
  std::stringstream x(s);
  std::string v;
  while (std::getline(x, v, ',')) r.push_back(v);
  return r;
}
double val(const std::string& s) { return s.empty() ? NAN : std::stod(s); }

double bandLoss(const Slice& s, const std::vector<Quote>& qs) {
  double z = 0;
  for (const auto& q : qs) {
    if (!q.included || !std::isfinite(q.bid_iv) || !std::isfinite(q.ask_iv)) continue;
    const double w = sliceTotalVariance(s, std::log(q.strike / q.forward));
    const double d = w - std::clamp(w, q.expiry * q.bid_iv * q.bid_iv,
                                    q.expiry * q.ask_iv * q.ask_iv);
    z += d * d;
  }
  return z;
}

bool pairSafe(const Slice* prev, const Slice& s) {
  if (!butterflySafe(s, kEps)) return false;
  return !prev || calendarSafe(*prev, s, kEps);
}

struct Atm {
  double theta_star{NAN};
  double k_star{0};
};

Atm nearestAtm(const std::vector<Quote>& qs, double t) {
  Atm a;
  double best = INFINITY;
  for (const auto& q : qs) {
    if (!q.included || !std::isfinite(q.mid_iv)) continue;
    const double k = std::log(q.strike / q.forward);
    if (std::abs(k) < best) {
      best = std::abs(k);
      a.k_star = k;
      a.theta_star = t * q.mid_iv * q.mid_iv;
    }
  }
  return a;
}

Slice anchored(double t, const Atm& atm, double rho, double psi, const Slice* prev) {
  double theta = atm.theta_star - rho * psi * atm.k_star;
  if (prev) theta = std::max(theta, prev->theta + kEps);
  if (prev && prev->psi > kEps)
    theta = std::max(theta, psi * prev->theta / prev->psi + kEps);
  theta = std::max(theta, kEps);
  for (int i = 0; i < 48; ++i) {
    if (psi * psi <= mmPsi2Bound(theta, std::abs(rho)) - 2 * kEps) break;
    theta *= 1.08;
  }
  return {t, theta, psi, rho};
}

double psiUpper(double theta, double rho) {
  const double ar = std::abs(rho);
  if (!(ar < 1.0 - kEps) || !(theta > kEps)) return 0;
  const double rl = 4.0 / (1.0 + ar) - kEps;
  const double mm = std::sqrt(std::max(0.0, mmPsi2Bound(theta, ar))) - kEps;
  return std::min(rl, mm);
}

double psiLower(const Slice* prev, double rho) {
  double lo = 2 * kEps;
  if (!prev) return lo;
  const double ratio =
      std::max((1 + prev->rho) / (1 + rho), (1 - prev->rho) / (1 - rho));
  return std::max(lo, std::max(prev->psi + kEps, prev->psi * ratio + kEps));
}

CalibrationResult fitSlice(const std::vector<Quote>& qs, double t, const Slice* prev) {
  CalibrationResult out;
  const auto atm = nearestAtm(qs, t);
  if (!std::isfinite(atm.theta_star) || atm.theta_star <= 0) {
    out.error = "no ATM quote";
    return out;
  }
  int evals = 0;
  Slice best{};
  double bestz = INFINITY;
  auto consider = [&](const Slice& s) {
    if (!pairSafe(prev, s)) return;
    ++evals;
    const double z = bandLoss(s, qs);
    if (z < bestz) {
      bestz = z;
      best = s;
    }
  };

  const double theta0 = prev ? std::max(atm.theta_star, prev->theta + kEps) : atm.theta_star;
  const auto searchRhoWindow = [&](double rlo, double rhi, double rstep, int psi_steps) {
    for (double rho = rlo; rho <= rhi + 1e-12; rho += rstep) {
      if (std::abs(rho) >= 0.98) continue;
      const double lo = psiLower(prev, rho);
      const double hi = psiUpper(theta0, rho);
      if (!(hi > lo)) continue;
      consider(anchored(t, atm, rho, lo, prev));
      consider(anchored(t, atm, rho, hi, prev));
      double a = lo, b = hi;
      const double g = 0.381966011250105;
      double x1 = a + g * (b - a), x2 = b - g * (b - a);
      auto f = [&](double psi) {
        const auto s = anchored(t, atm, rho, psi, prev);
        if (!pairSafe(prev, s)) return 1e300;
        ++evals;
        return bandLoss(s, qs);
      };
      double f1 = f(x1), f2 = f(x2);
      for (int i = 0; i < psi_steps; ++i) {
        if (f1 < f2) {
          b = x2;
          x2 = x1;
          f2 = f1;
          x1 = a + g * (b - a);
          f1 = f(x1);
        } else {
          a = x1;
          x1 = x2;
          f1 = f2;
          x2 = b - g * (b - a);
          f2 = f(x2);
        }
      }
      consider(anchored(t, atm, rho, f1 < f2 ? x1 : x2, prev));
    }
  };

  searchRhoWindow(-0.95, 0.95, 0.05, 22);
  if (bestz < INFINITY) searchRhoWindow(std::max(-0.95, best.rho - 0.06),
                                       std::min(0.95, best.rho + 0.06), 0.01, 18);
  if (bestz < INFINITY) {
    const double tlo = prev ? prev->theta + kEps : kEps;
    for (double m : {0.90, 0.95, 1.00, 1.05, 1.10, 1.20}) {
      Slice s = best;
      s.theta = std::clamp(best.theta * m, tlo, 5.0);
      consider(s);
    }
  }

  out.iterations = evals;
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
  std::vector<double> expiries;
  for (const auto& q : quotes)
    if (q.included && std::isfinite(q.expiry)) expiries.push_back(q.expiry);
  std::sort(expiries.begin(), expiries.end());
  expiries.erase(std::unique(expiries.begin(), expiries.end()), expiries.end());
  CalibrationResult out;
  if (expiries.empty()) {
    out.error = "no included quotes";
    return out;
  }
  auto started = std::chrono::steady_clock::now();
  for (double t : expiries) {
    std::vector<Quote> slice_q;
    for (const auto& q : quotes)
      if (q.included && q.expiry == t) slice_q.push_back(q);
    const Slice* prev = out.surface.slices.empty() ? nullptr : &out.surface.slices.back();
    auto part = fitSlice(slice_q, t, prev);
    out.iterations += part.iterations;
    out.objective += part.objective < INFINITY ? part.objective : 0;
    if (!part.converged) {
      out.error = part.error.empty() ? "slice fit failed" : part.error;
      out.runtime_seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
      return out;
    }
    out.surface.slices.push_back(part.surface.slices.front());
  }
  out.runtime_seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  out.error = validate(out.surface);
  out.converged = out.error.empty();
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
