#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "calibrator.hpp"

#include <cstddef>
#include <filesystem>
#include <limits>
#include <type_traits>

namespace nb = nanobind;
using namespace nb::literals;

using QuoteVector = std::vector<essvi::Quote>;
NB_MAKE_OPAQUE(QuoteVector);

static_assert(std::is_standard_layout_v<essvi::Slice>);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
static_assert(sizeof(essvi::Slice) == 4 * sizeof(double));
static_assert(offsetof(essvi::Slice, expiry) == 0);
static_assert(offsetof(essvi::Slice, theta) == sizeof(double));
static_assert(offsetof(essvi::Slice, psi) == 2 * sizeof(double));
static_assert(offsetof(essvi::Slice, rho) == 3 * sizeof(double));

using ParameterView = nb::ndarray<nb::memview, const double,
                                  nb::shape<-1, 4>, nb::c_contig>;

NB_MODULE(_core, m) {
  using namespace essvi;

  nb::class_<Quote>(m, "Quote")
      .def(nb::init<std::string,
                    double, double, double, double,
                    double, double, double, bool, double>(),
           "id"_a, "expiry"_a, "forward"_a, "discount_factor"_a,
           "strike"_a, "bid_iv"_a, "ask_iv"_a, "mid_iv"_a,
           "included"_a = true, "weight"_a = 1.0)
      .def_ro("id", &Quote::id)
      .def_ro("expiry", &Quote::expiry)
      .def_ro("forward", &Quote::forward)
      .def_ro("discount_factor", &Quote::discount_factor)
      .def_ro("strike", &Quote::strike)
      .def_ro("bid_iv", &Quote::bid_iv)
      .def_ro("ask_iv", &Quote::ask_iv)
      .def_ro("mid_iv", &Quote::mid_iv)
      .def_ro("included", &Quote::included)
      .def_ro("weight", &Quote::weight);

  nb::bind_vector<QuoteVector>(m, "Quotes");

  nb::class_<Slice>(m, "Slice")
      .def_ro("expiry", &Slice::expiry)
      .def_ro("theta", &Slice::theta)
      .def_ro("psi", &Slice::psi)
      .def_ro("rho", &Slice::rho);

  nb::class_<Surface>(m, "Surface")
      .def_ro("slices", &Surface::slices)
      .def("parameter_view", [](const Surface& surface) {
        // A null buffer makes NumPy allocate a writable array even at size zero.
        const void* data = surface.slices.empty()
                               ? static_cast<const void*>(&surface)
                               : surface.slices.data();
        return ParameterView(data, {surface.slices.size(), 4});
      }, nb::rv_policy::reference_internal,
      "Read-only zero-copy memoryview with columns expiry, theta, psi, rho.")
      .def("slice_at", &Surface::sliceAt, "expiry"_a)
      .def("total_variance", &Surface::totalVariance, "expiry"_a, "k"_a)
      .def("implied_volatility", &Surface::impliedVolatility, "expiry"_a, "k"_a);

  nb::class_<WarmOptions>(m, "WarmOptions")
      .def(nb::init<>())
      .def_rw("quote_tol", &WarmOptions::quote_tol)
      .def_rw("quote_cap", &WarmOptions::quote_cap);

  nb::enum_<CalibrationPath>(m, "CalibrationPath")
      .value("cold", CalibrationPath::cold)
      .value("warm", CalibrationPath::warm)
      .value("fallback", CalibrationPath::fallback)
      .value("degraded", CalibrationPath::degraded);

  nb::class_<CalibrationTimings>(m, "CalibrationTimings")
      .def_ro("quote_grouping_seconds", &CalibrationTimings::quote_grouping_seconds)
      .def_ro("slice_fitting_seconds", &CalibrationTimings::slice_fitting_seconds)
      .def_ro("validation_seconds", &CalibrationTimings::validation_seconds);

  nb::class_<WarmFallbackReasonCounts>(m, "WarmFallbackReasonCounts")
      .def_ro("infeasible_local_result", &WarmFallbackReasonCounts::infeasible_local_result)
      .def_ro("loss_above_gate", &WarmFallbackReasonCounts::loss_above_gate)
      .def_ro("calendar_repair_failed", &WarmFallbackReasonCounts::calendar_repair_failed)
      .def_ro("bound_check_failed", &WarmFallbackReasonCounts::bound_check_failed)
      .def_ro("budget_exhausted", &WarmFallbackReasonCounts::budget_exhausted);

  nb::class_<CalibrationResult>(m, "CalibrationResult")
      .def_ro("surface", &CalibrationResult::surface)
      .def_ro("reuse_ages", &CalibrationResult::reuse_ages)
      .def_ro("converged", &CalibrationResult::converged)
      .def_ro("objective", &CalibrationResult::objective)
      .def_ro("iterations", &CalibrationResult::iterations)
      .def_ro("objective_evaluations", &CalibrationResult::objective_evaluations)
      .def_ro("local_solver_slices", &CalibrationResult::local_solver_slices)
      .def_ro("local_solver_starts", &CalibrationResult::local_solver_starts)
      .def_ro("fallback_reason_counts", &CalibrationResult::fallback_reason_counts)
      .def_ro("restricted_cold_searches", &CalibrationResult::restricted_cold_searches)
      .def_ro("full_cold_searches", &CalibrationResult::full_cold_searches)
      .def_ro("forced_full_cold_searches", &CalibrationResult::forced_full_cold_searches)
      .def_ro("degraded_slices", &CalibrationResult::degraded_slices)
      .def_ro("degraded_ages", &CalibrationResult::degraded_ages)
      .def_ro("quality_quote_count", &CalibrationResult::quality_quote_count)
      .def_ro("quality_maximum_excess_ratio", &CalibrationResult::quality_maximum_excess_ratio)
      .def_ro("quality_rms_excess_ratio", &CalibrationResult::quality_rms_excess_ratio)
      .def_ro("worst_slice_max_excess_ratio", &CalibrationResult::worst_slice_max_excess_ratio)
      .def_ro("worst_slice_rms_excess_ratio", &CalibrationResult::worst_slice_rms_excess_ratio)
      .def_ro("local_objective_evaluations", &CalibrationResult::local_objective_evaluations)
      .def_ro("local_mm_bound_calls", &CalibrationResult::local_mm_bound_calls)
      .def_ro("mm_bound_calls", &CalibrationResult::mm_bound_calls)
      .def_ro("slices_refit", &CalibrationResult::slices_refit)
      .def_ro("slices_reused", &CalibrationResult::slices_reused)
      .def_ro("fallback_slices", &CalibrationResult::fallback_slices)
      .def_ro("quality_checks", &CalibrationResult::quality_checks)
      .def_ro("quality_warm_loss", &CalibrationResult::quality_warm_loss)
      .def_ro("quality_reference_loss", &CalibrationResult::quality_reference_loss)
      .def_ro("quality_loss_scale", &CalibrationResult::quality_loss_scale)
      .def_ro("loss_tolerance", &CalibrationResult::loss_tolerance)
      .def_ro("path", &CalibrationResult::path)
      .def_ro("timings", &CalibrationResult::timings)
      .def_ro("runtime_seconds", &CalibrationResult::runtime_seconds)
      .def_ro("error", &CalibrationResult::error)
      .def_prop_ro(
          "posterior_covariances",
          [](const CalibrationResult& result) {
            nb::list snapshots;
            for (const auto& covariance : result.posterior_covariances)
              snapshots.append(nb::make_tuple(
                  covariance[0], covariance[1], covariance[2], covariance[3],
                  covariance[4], covariance[5]));
            return nb::tuple(snapshots);
          });

  m.def("read_quotes", [](const std::filesystem::path& path) {
    return readQuotes(path.string());
  }, "path"_a);
  m.def("calibrate", &calibrate, nb::arg("quotes").noconvert(),
        nb::call_guard<nb::gil_scoped_release>());
  m.def("calibrate_warm", &calibrateWarm,
        nb::arg("current_quotes").noconvert(),
        nb::arg("previous_result").none(),
        nb::arg("previous_quotes").noconvert(),
        nb::arg("options") = WarmOptions{},
        nb::call_guard<nb::gil_scoped_release>());
}
