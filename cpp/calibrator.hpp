#pragma once
#include "essvi_surface.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace essvi {
struct Quote {
  std::string id;
  double expiry, forward, discount_factor, strike, bid_iv, ask_iv, mid_iv;
  bool included;
  double weight{1.0};
};

enum class CalibrationPath { cold, warm, fallback, degraded };

struct CalibrationTimings {
  double quote_grouping_seconds{};
  double slice_fitting_seconds{};
  double validation_seconds{};
};

struct WarmFallbackReasonCounts {
  std::uint64_t infeasible_local_result{};
  std::uint64_t loss_above_gate{};
  std::uint64_t calendar_repair_failed{};
  std::uint64_t bound_check_failed{};
  std::uint64_t budget_exhausted{};
};

struct CalibrationResult {
  Surface surface;
  // Consecutive unchanged-parameter reuses, aligned with surface.slices.
  std::vector<std::size_t> reuse_ages;
  bool converged{};
  double objective{};
  int iterations{};
  std::uint64_t objective_evaluations{};
  std::size_t local_solver_slices{};
  std::size_t local_solver_starts{};
  WarmFallbackReasonCounts fallback_reason_counts;
  std::size_t restricted_cold_searches{};
  std::size_t full_cold_searches{};
  std::size_t forced_full_cold_searches{};
  std::size_t degraded_slices{};
  // Consecutive quality-gate failures, aligned with surface.slices.
  std::vector<std::size_t> degraded_ages;
  std::uint64_t quality_quote_count{};
  double quality_maximum_excess_ratio{};
  double quality_rms_excess_ratio{};
  double worst_slice_max_excess_ratio{};
  double worst_slice_rms_excess_ratio{};
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
  double loss_tol{0.05};  // Deprecated; retained for source compatibility only.
  double quote_cap{0.1};
};

CalibrationResult calibrate(const std::vector<Quote>& quotes);
CalibrationResult calibrateWarm(const std::vector<Quote>& current_quotes,
                                const CalibrationResult* previous_result,
                                const std::vector<Quote>& previous_quotes,
                                const WarmOptions& options = {});
std::vector<Quote> readQuotes(const std::string& path);
void writeParameters(const CalibrationResult& result, const std::string& path);
void writeDiagnostics(const CalibrationResult& result, const std::string& path);
}  // namespace essvi
