#pragma once
#include "essvi_surface.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace essvi {
struct Quote { std::string id; double expiry, forward, discount_factor, strike, bid_iv, ask_iv, mid_iv; bool included; };

enum class CalibrationPath { cold, warm, fallback };

struct CalibrationTimings {
  double quote_grouping_seconds{};
  double slice_fitting_seconds{};
  double validation_seconds{};
};

struct CalibrationResult {
  Surface surface;
  bool converged{};
  double objective{};
  int iterations{};
  std::uint64_t objective_evaluations{};
  std::size_t local_solver_slices{};
  std::uint64_t local_objective_evaluations{};
  std::uint64_t local_mm_bound_calls{};
  std::uint64_t mm_bound_calls{};
  std::size_t slices_refit{};
  std::size_t slices_reused{};
  std::size_t fallback_slices{};
  std::size_t quality_checks{};
  double quality_warm_loss{};
  double quality_reference_loss{};
  double quality_loss_scale{};
  double loss_tolerance{};
  CalibrationPath path{CalibrationPath::cold};
  CalibrationTimings timings;
  double runtime_seconds{};
  std::string error;
};

struct WarmOptions {
  double quote_tol{1e-5};
  double loss_tol{0.05};  // Fraction of squared total-variance band width.
  std::size_t cold_check_interval{10};
  std::size_t update_number{1};  // 1-based sequence number for deterministic periodic checks.
};

CalibrationResult calibrate(const std::vector<Quote>& quotes);
CalibrationResult calibrateWarm(const std::vector<Quote>& current_quotes,
                                const Surface& previous_surface,
                                const std::vector<Quote>& previous_quotes,
                                const WarmOptions& options = {});
std::vector<Quote> readQuotes(const std::string& path);
void writeParameters(const CalibrationResult& result, const std::string& path);
void writeDiagnostics(const CalibrationResult& result, const std::string& path);
}  // namespace essvi
