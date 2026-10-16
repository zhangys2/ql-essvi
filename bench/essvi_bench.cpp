#include "calibrator.hpp"
#include "essvi_surface.hpp"
#include "realistic_warm.hpp"
#include "quality_metrics.hpp"
#include "synthetic_chain.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

struct Options {
  std::size_t expiries{40};
  std::size_t quotes_per_expiry{100};
  std::size_t repeats{9};
  std::size_t warm_updates{100};
  std::size_t scenario_updates{200};
  double quote_cap{0.1};
  std::uint64_t seed{0x8f3f73b5cf1c9ad1ULL};
  std::string input;
  std::string parameters_out;
  std::string timings_out;
  std::string scenario;
  std::string replay_dir;
  std::size_t replay_limit{};
  bool check{};
};

struct Summary {
  std::string name;
  std::vector<double> warm_seconds;
  std::vector<double> cold_seconds;
  std::array<std::size_t, 3> path_counts{};
  std::size_t samples{};
  std::size_t snapshots{};
  std::size_t slices_refit{};
  std::size_t slices_reused{};
  std::size_t fallback_slices{};
  std::size_t warm_failures{};
  std::size_t cold_failures{};
  essvi::bench::SurfaceQualityCounts warm_surface_quality;
  essvi::bench::SurfaceQualityCounts cold_surface_quality;
  essvi::bench::BenchmarkQuality quality;
  std::uint64_t objective_evaluations{};
  std::uint64_t mm_bound_calls{};
  std::size_t local_solver_slices{};
  std::uint64_t local_objective_evaluations{};
  std::uint64_t local_mm_bound_calls{};
  double warm_loss{};
  double cold_loss{};
  double quality_warm_loss{};
  double quality_reference_loss{};
  double quality_loss_scale{};
  double loss_tolerance{};
  double quote_cap{0.1};
  std::string load_average_before;
  std::string load_average_after;
};

struct LegacyOptions {
  std::size_t expiries{};
  std::size_t quotes_per_expiry{};
  std::size_t repeats{};
  std::size_t warm_updates{};
  std::string input;
  std::string parameters_out;
  std::string timings_out;
};

std::size_t parseSize(const std::string& value) {
  std::size_t consumed = 0;
  const auto parsed = std::stoull(value, &consumed);
  if (consumed != value.size())
    throw std::invalid_argument("not an integer: " + value);
  return static_cast<std::size_t>(parsed);
}

std::uint64_t parseSeed(const std::string& value) {
  std::size_t consumed = 0;
  const auto parsed = std::stoull(value, &consumed, 0);
  if (consumed != value.size())
    throw std::invalid_argument("not an integer seed: " + value);
  return static_cast<std::uint64_t>(parsed);
}

double parseDouble(const std::string& value) {
  std::size_t consumed = 0;
  const double parsed = std::stod(value, &consumed);
  if (consumed != value.size() || !std::isfinite(parsed))
    throw std::invalid_argument("not a finite number: " + value);
  return parsed;
}

double percentile(const std::vector<double>& values, double p) {
  if (values.empty()) return 0.0;
  auto sorted = values;
  std::sort(sorted.begin(), sorted.end());
  const double index = p * static_cast<double>(sorted.size() - 1);
  const auto low = static_cast<std::size_t>(index);
  const auto high = std::min(low + 1, sorted.size() - 1);
  const double fraction = index - static_cast<double>(low);
  return sorted[low] + fraction * (sorted[high] - sorted[low]);
}

double maximum(const std::vector<double>& values) {
  return values.empty() ? 0.0 : *std::max_element(values.begin(), values.end());
}

std::string loadAverage() {
  std::ifstream input("/proc/loadavg");
  std::string one, five, fifteen;
  if (!(input >> one >> five >> fifteen)) return "unavailable";
  return one + " " + five + " " + fifteen;
}

const char* pathName(essvi::CalibrationPath path) {
  switch (path) {
    case essvi::CalibrationPath::cold: return "cold";
    case essvi::CalibrationPath::warm: return "warm";
    case essvi::CalibrationPath::fallback: return "fallback";
  }
  return "unknown";
}

double normalizedCall(double forward, double strike, double variance) {
  if (variance <= 0.0) return std::max(forward - strike, 0.0);
  const double root = std::sqrt(variance);
  const double d1 = (std::log(forward / strike) + 0.5 * variance) / root;
  const double d2 = d1 - root;
  const auto cdf = [](double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
  };
  return forward * cdf(d1) - strike * cdf(d2);
}

std::size_t denseGridViolationCount(const essvi::Surface& surface) {
  std::size_t violations = 0;
  std::array<double, 101> previous_variance{};
  bool has_previous = false;
  for (const auto& slice : surface.slices) {
    std::array<double, 101> variance{};
    std::array<double, 101> call{};
    std::array<double, 101> strike{};
    for (std::size_t j = 0; j <= 100; ++j) {
      const double k = -0.5 + 0.01 * static_cast<double>(j);
      strike[j] = 100.0 * std::exp(k);
      variance[j] = essvi::sliceTotalVariance(slice, k);
      if (!(std::isfinite(variance[j]) && variance[j] > 0.0)) {
        ++violations;
        continue;
      }
      if (has_previous && variance[j] < previous_variance[j] - 1e-8)
        ++violations;
      call[j] = normalizedCall(100.0, strike[j], variance[j]);
    }
    for (std::size_t j = 1; j < 100; ++j) {
      const double left_slope = (call[j] - call[j - 1]) /
                                (strike[j] - strike[j - 1]);
      const double right_slope = (call[j + 1] - call[j]) /
                                 (strike[j + 1] - strike[j]);
      if (!(std::isfinite(left_slope) && std::isfinite(right_slope) &&
            right_slope >= left_slope - 1e-8))
        ++violations;
    }
    previous_variance = variance;
    has_previous = true;
  }
  return violations;
}

void record(Summary& summary, const essvi::CalibrationResult& warm,
            const essvi::CalibrationResult& cold,
            const std::vector<essvi::Quote>& quotes, double warm_seconds,
            double cold_seconds) {
  summary.warm_seconds.push_back(warm_seconds);
  summary.cold_seconds.push_back(cold_seconds);
  ++summary.samples;
  ++summary.path_counts[static_cast<std::size_t>(warm.path)];
  summary.slices_refit += warm.slices_refit;
  summary.slices_reused += warm.slices_reused;
  summary.fallback_slices += warm.fallback_slices;
  summary.warm_failures += !warm.converged;
  summary.cold_failures += !cold.converged;

  const bool warm_surface_valid = warm.converged &&
                                  !warm.surface.slices.empty() &&
                                  essvi::validate(warm.surface).empty();
  const bool cold_surface_valid = cold.converged &&
                                  !cold.surface.slices.empty() &&
                                  essvi::validate(cold.surface).empty();
  if (warm.converged) {
    if (!warm_surface_valid)
      ++summary.warm_surface_quality.invalid_surfaces;
    else
      summary.warm_surface_quality.dense_grid_violations +=
          denseGridViolationCount(warm.surface);
  }
  if (cold.converged) {
    if (!cold_surface_valid)
      ++summary.cold_surface_quality.invalid_surfaces;
    else
      summary.cold_surface_quality.dense_grid_violations +=
          denseGridViolationCount(cold.surface);
  }
  if (warm.converged && std::isfinite(warm.objective))
    summary.warm_loss += warm.objective;
  if (cold.converged && std::isfinite(cold.objective))
    summary.cold_loss += cold.objective;

  if (warm_surface_valid && cold_surface_valid) {
    const std::size_t eligible_quotes =
        essvi::bench::countValidTwoSidedBands(quotes);
    const auto warm_metrics =
        essvi::bench::measureExcessMetrics(quotes, warm.surface, 0.1);
    const auto cold_metrics =
        essvi::bench::measureExcessMetrics(quotes, cold.surface, 0.1);
    essvi::bench::mergePairedQuoteMetrics(
        summary.quality, warm_metrics, cold_metrics, eligible_quotes);
  }

  summary.objective_evaluations += warm.objective_evaluations;
  summary.mm_bound_calls += warm.mm_bound_calls;
  summary.local_solver_slices += warm.local_solver_slices;
  summary.local_objective_evaluations += warm.local_objective_evaluations;
  summary.local_mm_bound_calls += warm.local_mm_bound_calls;
  summary.quality.checks += warm.quality_checks;
  summary.quality_warm_loss += warm.quality_warm_loss;
  summary.quality_reference_loss += warm.quality_reference_loss;
  summary.quality_loss_scale += warm.quality_loss_scale;
  summary.loss_tolerance = warm.loss_tolerance;
}

void printTimes(const char* prefix, const std::vector<double>& values) {
  std::cout << prefix << "_median_seconds=" << percentile(values, 0.50) << '\n'
            << prefix << "_p90_seconds=" << percentile(values, 0.90) << '\n'
            << prefix << "_p99_seconds=" << percentile(values, 0.99) << '\n'
            << prefix << "_max_seconds=" << maximum(values) << '\n';
}

void printQuoteMetrics(const char* prefix,
                       const essvi::bench::QuoteExcessMetrics& metrics) {
  std::cout << prefix << "_quote_count=" << metrics.quote_count << '\n'
            << prefix << "_share_above_cap=" << metrics.share_above_cap()
            << '\n'
            << prefix << "_maximum_excess_ratio=" << metrics.maximum_ratio
            << '\n'
            << prefix << "_rms_excess_ratio=" << metrics.rms_ratio() << '\n'
            << prefix << "_summed_band_loss=" << metrics.summed_band_loss
            << '\n'
            << prefix << "_rms_excess_vol_points="
            << metrics.rms_excess_vol_points() << '\n'
            << prefix << "_maximum_excess_vol_points="
            << metrics.maximum_excess_vol_points << '\n';
}

void printSummary(const Summary& summary) {
  const double samples = static_cast<double>(summary.samples);
  const auto& warm_quality = summary.quality.warm;
  const auto& cold_quality = summary.quality.cold;
  const bool has_paired_quotes =
      essvi::bench::hasPairedQuoteObservations(warm_quality, cold_quality);
  std::cout << std::setprecision(17)
            << "run=" << summary.name << '\n'
            << "load_average_before=" << summary.load_average_before << '\n'
            << "load_average_after=" << summary.load_average_after << '\n'
            << "samples=" << summary.samples << '\n'
            << "snapshots=" << summary.snapshots << '\n'
            << "warm_gate_quote_cap=" << summary.quote_cap << '\n';
  printTimes("warm", summary.warm_seconds);
  printTimes("cold", summary.cold_seconds);
  std::cout << "mean_slices_refit="
            << (samples ? summary.slices_refit / samples : 0.0) << '\n'
            << "mean_slices_reused="
            << (samples ? summary.slices_reused / samples : 0.0) << '\n'
            << "path_cold=" << summary.path_counts[0] << '\n'
            << "path_warm=" << summary.path_counts[1] << '\n'
            << "path_fallback=" << summary.path_counts[2] << '\n'
            << "fallback_updates=" << summary.path_counts[2] << '\n'
            << "fallback_slices=" << summary.fallback_slices << '\n'
            << "warm_local_solver_slices=" << summary.local_solver_slices << '\n'
            << "warm_failures=" << summary.warm_failures << '\n'
            << "cold_failures=" << summary.cold_failures << '\n'
            << "warm_invalid_surfaces="
            << summary.warm_surface_quality.invalid_surfaces << '\n'
            << "cold_invalid_surfaces="
            << summary.cold_surface_quality.invalid_surfaces << '\n'
            << "warm_dense_grid_violations="
            << summary.warm_surface_quality.dense_grid_violations << '\n'
            << "cold_dense_grid_violations="
            << summary.cold_surface_quality.dense_grid_violations << '\n'
            << "summed_warm_objective=" << summary.warm_loss << '\n'
            << "summed_cold_objective=" << summary.cold_loss << '\n'
            << "paired_quote_updates=" << summary.quality.paired_updates << '\n'
            << "summed_warm_band_loss=" << warm_quality.summed_band_loss
            << '\n'
            << "summed_cold_band_loss=" << cold_quality.summed_band_loss
            << '\n';
  if (has_paired_quotes)
    std::cout << "warm_cold_share_above_cap_delta_pp="
              << (warm_quality.share_above_cap() -
                  cold_quality.share_above_cap()) *
                     100.0
              << '\n';
  else
    std::cout << "warm_cold_share_above_cap_delta_pp=unavailable\n";
  printQuoteMetrics("warm", warm_quality);
  printQuoteMetrics("cold", cold_quality);
  std::cout << "warm_objective_evaluations_per_refit_slice="
            << (summary.slices_refit
                    ? static_cast<double>(summary.objective_evaluations) /
                          static_cast<double>(summary.slices_refit)
                    : 0.0)
            << '\n'
            << "warm_mm_bound_calls_per_refit_slice="
            << (summary.slices_refit
                    ? static_cast<double>(summary.mm_bound_calls) /
                          static_cast<double>(summary.slices_refit)
                    : 0.0)
            << '\n'
            << "warm_local_objective_evaluations_per_slice="
            << (summary.local_solver_slices
                    ? static_cast<double>(summary.local_objective_evaluations) /
                          static_cast<double>(summary.local_solver_slices)
                    : 0.0)
            << '\n'
            << "warm_local_mm_bound_calls_per_slice="
            << (summary.local_solver_slices
                    ? static_cast<double>(summary.local_mm_bound_calls) /
                          static_cast<double>(summary.local_solver_slices)
                    : 0.0)
            << '\n'
            << "per_quote_gate_checks=" << summary.quality.checks << '\n'
            << "gate_candidate_band_loss=" << summary.quality_warm_loss
            << '\n'
            << "gate_cold_reference_band_loss="
            << summary.quality_reference_loss << '\n'
            << "legacy_band_loss_scale_diagnostic=" << summary.quality_loss_scale
            << '\n'
            << "loss_tol_compatibility_value=" << summary.loss_tolerance << '\n';
  if (has_paired_quotes) {
    const auto loss_ratio = essvi::bench::checkRatioAtMost(
        "summed_band_loss_ratio", warm_quality.summed_band_loss,
        cold_quality.summed_band_loss, 1.05, 1.20);
    if (loss_ratio.available)
      std::cout << "warm_cold_band_loss_ratio=" << loss_ratio.value << '\n';
    else
      std::cout << "warm_cold_band_loss_ratio=undefined (cold loss is zero)\n";
  } else {
    std::cout << "warm_cold_band_loss_ratio=unavailable\n";
  }
}

void writeJsonTimes(std::ostream& output, const char* name,
                    const std::vector<double>& values) {
  output << "\"" << name << "\": {\"median\":" << percentile(values, 0.50)
         << ",\"p90\":" << percentile(values, 0.90)
         << ",\"p99\":" << percentile(values, 0.99)
         << ",\"max\":" << maximum(values) << '}';
}

void writeJsonQuoteMetrics(std::ostream& output, const char* name,
                           const essvi::bench::QuoteExcessMetrics& metrics) {
  output << "\"" << name << "\":{" << "\"quote_count\":"
         << metrics.quote_count << ",\"above_cap_count\":"
         << metrics.above_cap_count << ",\"share_above_cap\":"
         << metrics.share_above_cap() << ",\"maximum_excess_ratio\":"
         << metrics.maximum_ratio << ",\"rms_excess_ratio\":"
         << metrics.rms_ratio() << ",\"summed_band_loss\":"
         << metrics.summed_band_loss << ",\"rms_excess_vol_points\":"
         << metrics.rms_excess_vol_points()
         << ",\"maximum_excess_vol_points\":"
         << metrics.maximum_excess_vol_points << '}';
}

void writeJsonSummary(std::ostream& output, const Summary& summary) {
  const double samples = static_cast<double>(summary.samples);
  const bool has_paired_quotes = essvi::bench::hasPairedQuoteObservations(
      summary.quality.warm, summary.quality.cold);
  output << "{\"run\":\"" << summary.name << "\","
         << "\"load_average_before\":\"" << summary.load_average_before
         << "\","
         << "\"load_average_after\":\"" << summary.load_average_after
         << "\","
         << "\"samples\":" << summary.samples << ','
         << "\"snapshots\":" << summary.snapshots << ','
         << "\"warm_gate_quote_cap\":" << summary.quote_cap << ',';
  writeJsonTimes(output, "warm_seconds", summary.warm_seconds);
  output << ',';
  {
    writeJsonTimes(output, "cold_seconds", summary.cold_seconds);
    const auto writeSamples = [&](const char* name, const std::vector<double>& values) {
      output << char(34) << name << char(34) << ":[";
      for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) output << ',';
        output << values[i];
      }
      output << ']';
    };
    output << ',';
    writeSamples("warm_seconds_samples", summary.warm_seconds);
    output << ',';
    writeSamples("cold_seconds_samples", summary.cold_seconds);
  }
  output << ",\"mean_slices_refit\":"
         << (samples ? summary.slices_refit / samples : 0.0)
         << ",\"mean_slices_reused\":"
         << (samples ? summary.slices_reused / samples : 0.0)
         << ",\"path_counts\":{\"cold\":" << summary.path_counts[0]
         << ",\"warm\":" << summary.path_counts[1]
         << ",\"fallback\":" << summary.path_counts[2] << "}"
         << ",\"fallback_slices\":" << summary.fallback_slices
         << ",\"local_solver_slices\":" << summary.local_solver_slices
         << ",\"warm_failures\":" << summary.warm_failures
         << ",\"cold_failures\":" << summary.cold_failures
         << ",\"warm_invalid_surfaces\":"
         << summary.warm_surface_quality.invalid_surfaces
         << ",\"cold_invalid_surfaces\":"
         << summary.cold_surface_quality.invalid_surfaces
         << ",\"warm_dense_grid_violations\":"
         << summary.warm_surface_quality.dense_grid_violations
         << ",\"cold_dense_grid_violations\":"
         << summary.cold_surface_quality.dense_grid_violations
         << ",\"summed_warm_objective\":" << summary.warm_loss
         << ",\"summed_cold_objective\":" << summary.cold_loss
         << ",\"paired_updates\":" << summary.quality.paired_updates << ',';
  writeJsonQuoteMetrics(output, "warm_quality", summary.quality.warm);
  output << ',';
  {
    writeJsonQuoteMetrics(output, "cold_quality", summary.quality.cold);
    output << ",\"cap_sensitivity\":[";
    for (std::size_t i = 0; i < essvi::bench::kQualityCapCount; ++i) {
      if (i) output << ',';
      output << "{\"cap\":" << essvi::bench::kQualityCaps[i]
             << ",\"paired_updates\":" << summary.quality.paired_updates
             << ",\"warm_above_cap_count\":"
             << summary.quality.warm.above_cap_counts[i]
             << ",\"cold_above_cap_count\":"
             << summary.quality.cold.above_cap_counts[i]
             << ",\"warm_share_above_cap\":"
             << summary.quality.warm.share_above_cap(i)
             << ",\"cold_share_above_cap\":"
             << summary.quality.cold.share_above_cap(i) << '}';
    }
    output << ']';
  }
  output << ",\"share_above_cap_delta_pp\":";
  if (has_paired_quotes) {
    output << (summary.quality.warm.share_above_cap() -
               summary.quality.cold.share_above_cap()) *
                  100.0;
  } else {
    output << "null";
  }
  output << ",\"summed_band_loss_ratio\":";
  if (has_paired_quotes) {
    const auto loss_ratio = essvi::bench::checkRatioAtMost(
        "summed_band_loss_ratio", summary.quality.warm.summed_band_loss,
        summary.quality.cold.summed_band_loss, 1.05, 1.20);
    if (loss_ratio.available && std::isfinite(loss_ratio.value))
      output << loss_ratio.value;
    else
      output << "null";
  } else {
    output << "null";
  }
  output << ",\"warm_objective_evaluations_per_refit_slice\":"
         << (summary.slices_refit
                 ? static_cast<double>(summary.objective_evaluations) /
                       static_cast<double>(summary.slices_refit)
                 : 0.0)
         << ",\"warm_mm_bound_calls_per_refit_slice\":"
         << (summary.slices_refit
                 ? static_cast<double>(summary.mm_bound_calls) /
                       static_cast<double>(summary.slices_refit)
                 : 0.0)
         << ",\"warm_local_objective_evaluations_per_slice\":"
         << (summary.local_solver_slices
                 ? static_cast<double>(summary.local_objective_evaluations) /
                       static_cast<double>(summary.local_solver_slices)
                 : 0.0)
         << ",\"warm_local_mm_bound_calls_per_slice\":"
         << (summary.local_solver_slices
                 ? static_cast<double>(summary.local_mm_bound_calls) /
                       static_cast<double>(summary.local_solver_slices)
                 : 0.0)
         << ",\"per_quote_gate_checks\":" << summary.quality.checks
         << ",\"gate_candidate_band_loss\":"
         << summary.quality_warm_loss
         << ",\"gate_cold_reference_band_loss\":"
         << summary.quality_reference_loss
         << ",\"legacy_band_loss_scale_diagnostic\":"
         << summary.quality_loss_scale
         << ",\"loss_tol_compatibility_value\":" << summary.loss_tolerance << '}';
}

void writeJsonSummaries(const std::string& path,
                        const std::vector<Summary>& summaries,
                        std::uint64_t seed, std::size_t updates) {
  std::ofstream output(path);
  if (!output) throw std::runtime_error("cannot write timing report: " + path);
  output << std::setprecision(17) << "{\"seed\":" << seed
         << ",\"updates\":" << updates << ",\"runs\":[";
  for (std::size_t i = 0; i < summaries.size(); ++i) {
    if (i) output << ',';
    writeJsonSummary(output, summaries[i]);
  }
  output << "]}\n";
}

std::vector<std::filesystem::path> replayFiles(
    const std::filesystem::path& directory) {
  if (!std::filesystem::is_directory(directory))
    throw std::runtime_error("replay path is not a directory: " +
                             directory.string());
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (!entry.is_regular_file()) continue;
    const auto filename = entry.path().filename().string();
    if (filename.rfind("normalized_quotes_", 0) == 0 &&
        entry.path().extension() == ".csv")
      files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

void runScenario(const Options& options, essvi::bench::WarmScenario scenario,
                 Summary& summary) {
  summary.name = essvi::bench::warmScenarioName(scenario);
  summary.quote_cap = options.quote_cap;
  summary.load_average_before = loadAverage();
  auto previous_quotes = essvi::bench::realisticChain(
      options.expiries, options.quotes_per_expiry, options.seed);
  const auto initial = essvi::calibrate(previous_quotes);
  if (!initial.converged)
    throw std::runtime_error("initial scenario fit failed: " + initial.error);

  auto previous_result = initial;
  essvi::bench::Noise noise(options.seed + 1);
  essvi::WarmOptions warm_options;
  warm_options.quote_cap = options.quote_cap;
  for (std::size_t update = 1; update <= options.scenario_updates; ++update) {
    auto current_quotes = previous_quotes;
    essvi::bench::evolveWarmScenario(
        current_quotes, options.expiries, options.quotes_per_expiry,
        scenario, update, noise);

    const auto warm_start = Clock::now();
    const auto warm = essvi::calibrateWarm(
        current_quotes, &previous_result, previous_quotes, warm_options);
    const double warm_seconds =
        std::chrono::duration<double>(Clock::now() - warm_start).count();

    const auto cold_start = Clock::now();
    const auto cold = essvi::calibrate(current_quotes);
    const double cold_seconds =
        std::chrono::duration<double>(Clock::now() - cold_start).count();
    record(summary, warm, cold, current_quotes, warm_seconds, cold_seconds);

    if (warm.converged)
      previous_result = warm;
    else if (cold.converged)
      previous_result = cold;
    else
      previous_result = {};
    previous_quotes = std::move(current_quotes);
  }
  summary.load_average_after = loadAverage();
}

void runReplay(const Options& options, Summary& summary) {
  summary.name = "replay (Black-Scholes European stand-in for American IVs)";
  summary.quote_cap = options.quote_cap;
  summary.load_average_before = loadAverage();
  auto files = replayFiles(options.replay_dir);
  if (options.replay_limit && files.size() > options.replay_limit)
    files.resize(options.replay_limit);
  if (files.empty()) throw std::runtime_error("replay needs at least one normalized snapshot");
  summary.snapshots = files.size();

  auto previous_quotes = essvi::readQuotes(files.front().string());
  if (previous_quotes.empty())
    throw std::runtime_error("no quotes in " + files.front().string());
  const auto initial = essvi::calibrate(previous_quotes);
  if (!initial.converged)
    throw std::runtime_error("initial replay fit failed: " + initial.error);
  auto previous_result = initial;

  essvi::WarmOptions warm_options;
  warm_options.quote_cap = options.quote_cap;
  for (std::size_t file_index = 1; file_index < files.size(); ++file_index) {
    auto current_quotes = essvi::readQuotes(files[file_index].string());
    if (current_quotes.empty())
      throw std::runtime_error("no quotes in " + files[file_index].string());

    const auto warm_start = Clock::now();
    const auto warm = essvi::calibrateWarm(
        current_quotes, &previous_result, previous_quotes, warm_options);
    const double warm_seconds =
        std::chrono::duration<double>(Clock::now() - warm_start).count();

    const auto cold_start = Clock::now();
    const auto cold = essvi::calibrate(current_quotes);
    const double cold_seconds =
        std::chrono::duration<double>(Clock::now() - cold_start).count();
    record(summary, warm, cold, current_quotes, warm_seconds, cold_seconds);

    if (warm.converged)
      previous_result = warm;
    else if (cold.converged)
      previous_result = cold;
    else
      previous_result = {};
    previous_quotes = std::move(current_quotes);
  }
  summary.load_average_after = loadAverage();
}

void writeLegacyTimings(const std::string& path, const Options& options,
                        const std::string& mode,
                        const std::string& load_average_before,
                        const std::string& load_average_after,
                        const essvi::CalibrationResult& cold,
                        double cold_seconds,
                        const std::vector<double>& repeated_seconds,
                        const std::vector<double>& warm_seconds,
                        const essvi::CalibrationResult& warm) {
  std::ofstream output(path);
  if (!output) throw std::runtime_error("cannot write timing report: " + path);
  output << std::setprecision(17)
         << "{\n"
         << "  \"mode\": \"" << mode << "\",\n"
         << "  \"load_average_before\": \"" << load_average_before
         << "\",\n"
         << "  \"load_average_after\": \"" << load_average_after
         << "\",\n"
         << "  \"expiries\": " << options.expiries << ",\n"
         << "  \"quotes_per_expiry\": " << options.quotes_per_expiry << ",\n"
         << "  \"cold\": {\n"
         << "    \"wall_seconds\": " << cold_seconds << ",\n"
         << "    \"runtime_seconds\": " << cold.runtime_seconds << ",\n"
         << "    \"quote_grouping_seconds\": "
         << cold.timings.quote_grouping_seconds << ",\n"
         << "    \"slice_fitting_seconds\": "
         << cold.timings.slice_fitting_seconds << ",\n"
         << "    \"validation_seconds\": " << cold.timings.validation_seconds
         << ",\n"
         << "    \"objective_evaluations\": "
         << cold.objective_evaluations << ",\n"
         << "    \"mm_bound_calls\": " << cold.mm_bound_calls << ",\n"
         << "    \"slices_refit\": " << cold.slices_refit << ",\n"
         << "    \"objective\": " << cold.objective << "\n"
         << "  },\n"
         << "  \"repeat\": {\n"
         << "    \"count\": " << repeated_seconds.size() << ",\n"
         << "    \"median_seconds\": " << percentile(repeated_seconds, 0.50)
         << ",\n"
         << "    \"p90_seconds\": " << percentile(repeated_seconds, 0.90)
         << ",\n"
         << "    \"p99_seconds\": " << percentile(repeated_seconds, 0.99)
         << ",\n"
         << "    \"max_seconds\": " << maximum(repeated_seconds) << "\n"
         << "  },\n"
         << "  \"warm\": {\n"
         << "    \"updates\": " << warm_seconds.size() << ",\n"
         << "    \"quote_cap\": " << options.quote_cap << ",\n"
         << "    \"median_seconds\": " << percentile(warm_seconds, 0.50)
         << ",\n"
         << "    \"p90_seconds\": " << percentile(warm_seconds, 0.90)
         << ",\n"
         << "    \"p99_seconds\": " << percentile(warm_seconds, 0.99)
         << ",\n"
         << "    \"max_seconds\": " << maximum(warm_seconds) << ",\n"
         << "    \"path\": \"" << pathName(warm.path) << "\",\n"
         << "    \"slices_refit\": " << warm.slices_refit << ",\n"
         << "    \"slices_reused\": " << warm.slices_reused << ",\n"
         << "    \"objective_evaluations\": "
         << warm.objective_evaluations << ",\n"
         << "    \"mm_bound_calls\": " << warm.mm_bound_calls << ",\n"
         << "    \"local_solver_slices\": " << warm.local_solver_slices << ",\n"
         << "    \"local_objective_evaluations\": "
         << warm.local_objective_evaluations << ",\n"
         << "    \"local_mm_bound_calls\": "
         << warm.local_mm_bound_calls << ",\n"
         << "    \"quote_grouping_seconds\": "
         << warm.timings.quote_grouping_seconds << ",\n"
         << "    \"slice_fitting_seconds\": "
         << warm.timings.slice_fitting_seconds << ",\n"
         << "    \"validation_seconds\": "
         << warm.timings.validation_seconds << "\n"
         << "  }\n"
         << "}\n";
}

void runLegacy(const Options& options) {
  const std::string load_average_before = loadAverage();
  const std::string mode = options.input.empty() ? "synthetic" : "csv";
  auto quotes = options.input.empty()
                    ? essvi::bench::syntheticChain(options.expiries,
                                                   options.quotes_per_expiry,
                                                   options.seed)
                    : essvi::readQuotes(options.input);
  if (quotes.empty()) throw std::runtime_error("no quotes loaded");

#ifdef ESSVI_PROFILE_MM_BOUND
  essvi::resetMmBoundProfile();
#endif
  const auto cold_start = Clock::now();
  const auto cold = essvi::calibrate(quotes);
  const double cold_seconds =
      std::chrono::duration<double>(Clock::now() - cold_start).count();
  if (!cold.converged)
    throw std::runtime_error("cold calibration failed: " + cold.error);
#ifdef ESSVI_PROFILE_MM_BOUND
  const auto cold_profile = essvi::mmBoundProfile();
  essvi::resetMmBoundProfile();
#endif

  std::vector<double> repeated_seconds;
  repeated_seconds.reserve(options.repeats);
  for (std::size_t i = 0; i < options.repeats; ++i) {
    const auto start = Clock::now();
    const auto repeated = essvi::calibrate(quotes);
    repeated_seconds.push_back(
        std::chrono::duration<double>(Clock::now() - start).count());
    if (!repeated.converged)
      throw std::runtime_error("repeated calibration failed: " + repeated.error);
  }
  std::sort(repeated_seconds.begin(), repeated_seconds.end());

#ifdef ESSVI_PROFILE_MM_BOUND
  const auto repeat_profile = essvi::mmBoundProfile();
#endif

  std::vector<double> warm_seconds;
  warm_seconds.reserve(options.warm_updates);
  essvi::WarmOptions warm_options;
  warm_options.quote_cap = options.quote_cap;
  warm_options.quote_tol = 1e-6;
  warm_options.loss_tol = 1e-6;
  auto previous_quotes = quotes;
  auto previous_result = cold;
  essvi::CalibrationResult warm = cold;
  for (std::size_t i = 0; i < options.warm_updates; ++i) {
    auto current_quotes = previous_quotes;
    for (auto& quote : current_quotes) {
      if (quote.included && std::isfinite(quote.bid_iv)) quote.bid_iv += 1e-7;
      if (quote.included && std::isfinite(quote.ask_iv)) quote.ask_iv += 1e-7;
    }
    const auto start = Clock::now();
    warm = essvi::calibrateWarm(current_quotes, &previous_result,
                                previous_quotes, warm_options);
    warm_seconds.push_back(
        std::chrono::duration<double>(Clock::now() - start).count());
    if (!warm.converged)
      throw std::runtime_error("warm calibration failed: " + warm.error);
    previous_result = warm;
    previous_quotes = std::move(current_quotes);
  }
  std::sort(warm_seconds.begin(), warm_seconds.end());
  const std::string load_average_after = loadAverage();

  if (!options.parameters_out.empty())
    essvi::writeParameters(cold, options.parameters_out);
  if (!options.timings_out.empty())
    writeLegacyTimings(options.timings_out, options, mode, load_average_before,
                       load_average_after, cold, cold_seconds, repeated_seconds,
                       warm_seconds, warm);

  std::cout << std::setprecision(17)
            << "mode=" << mode << '\n'
            << "expiries=" << options.expiries << '\n'
            << "quotes_per_expiry=" << options.quotes_per_expiry << '\n'
            << "quotes=" << quotes.size() << '\n'
            << "repeats=" << options.repeats << '\n'
            << "warm_updates=" << options.warm_updates << '\n'
            << "compiler=" << __VERSION__ << '\n'
            << "cold_seconds=" << cold_seconds << '\n'
            << "cold_runtime_seconds=" << cold.runtime_seconds << '\n'
            << "repeat_median_seconds=" << percentile(repeated_seconds, 0.50)
            << '\n'
            << "repeat_p90_seconds=" << percentile(repeated_seconds, 0.90)
            << '\n'
            << "repeat_p99_seconds=" << percentile(repeated_seconds, 0.99)
            << '\n'
            << "repeat_max_seconds=" << maximum(repeated_seconds) << '\n'
            << "warm_median_seconds=" << percentile(warm_seconds, 0.50) << '\n'
            << "warm_p90_seconds=" << percentile(warm_seconds, 0.90) << '\n'
            << "warm_p99_seconds=" << percentile(warm_seconds, 0.99) << '\n'
            << "warm_max_seconds=" << maximum(warm_seconds) << '\n'
            << "warm_gate_quote_cap=" << options.quote_cap << '\n'
            << "warm_path=" << pathName(warm.path) << '\n'
            << "warm_slices_refit=" << warm.slices_refit << '\n'
            << "warm_slices_reused=" << warm.slices_reused << '\n'
            << "warm_local_solver_slices=" << warm.local_solver_slices << '\n'
            << "warm_local_objective_evaluations="
            << warm.local_objective_evaluations << '\n'
            << "warm_local_mm_bound_calls=" << warm.local_mm_bound_calls << '\n'
            << "load_average_before=" << load_average_before << '\n'
            << "load_average_after=" << load_average_after << '\n'
            << "cold_objective_evaluations=" << cold.objective_evaluations
            << '\n'
            << "cold_mm_bound_calls=" << cold.mm_bound_calls << '\n'
            << "iterations=" << cold.iterations << '\n'
            << "objective=" << cold.objective << '\n';
#ifdef ESSVI_PROFILE_MM_BOUND
  std::cout << "profiled_cold_mm_bound_calls=" << cold_profile.calls << '\n'
            << "profiled_cold_mm_bound_seconds="
            << static_cast<double>(cold_profile.nanoseconds) * 1e-9 << '\n'
            << "profiled_repeat_mm_bound_calls_per_run="
            << static_cast<double>(repeat_profile.calls) /
                   static_cast<double>(options.repeats)
            << '\n'
            << "profiled_repeat_mm_bound_seconds_per_run="
            << static_cast<double>(repeat_profile.nanoseconds) * 1e-9 /
                   static_cast<double>(options.repeats)
            << '\n';
#endif
}

std::vector<essvi::bench::QualityCheck> qualityChecks(const Summary& summary) {
  using essvi::bench::checkAtMost;
  using essvi::bench::checkRatioAtMost;
  const auto& warm = summary.quality.warm;
  const auto& cold = summary.quality.cold;
  const bool has_quotes =
      essvi::bench::hasPairedQuoteObservations(warm, cold);
  const double unavailable = std::numeric_limits<double>::quiet_NaN();
  const auto metric = [has_quotes, unavailable](double value) {
    return has_quotes ? value : unavailable;
  };
  const bool is_fast = summary.name == "fast";
  const double p99_target_ms = is_fast ? 10.0 : 3.0;
  const double p99_hard_ms = is_fast ? 20.0 : 10.0;
  const double fallback_target_percent = is_fast ? 10.0 : 1.0;
  const double fallback_hard_percent = is_fast ? 25.0 : 5.0;
  const double fallback_rate_percent =
      summary.samples
          ? 100.0 * static_cast<double>(summary.path_counts[2]) /
                static_cast<double>(summary.samples)
          : unavailable;
  const double max_target = std::max(0.1, cold.maximum_ratio + 0.01);
  const double max_hard = std::max(0.2, cold.maximum_ratio + 0.05);
  std::vector<essvi::bench::QualityCheck> checks;
  checks.reserve(13);
  checks.push_back(
        checkAtMost("unpaired or unscored updates",
                    static_cast<double>(summary.samples -
                                        summary.quality.paired_updates),
                    0.0, 0.0));
  checks.push_back(checkAtMost(
      "share above cap warm minus cold (percentage points)",
      metric((warm.share_above_cap() - cold.share_above_cap()) * 100.0),
      0.5, 2.0));
  checks.push_back(checkAtMost("maximum excess ratio, warm",
                               metric(warm.maximum_ratio), max_target,
                               max_hard));
  checks.push_back(checkAtMost("RMS excess ratio, warm",
                               metric(warm.rms_ratio()), 0.02, 0.05));
  checks.push_back(checkRatioAtMost("summed band loss warm / cold",
                                    metric(warm.summed_band_loss),
                                    metric(cold.summed_band_loss), 1.05, 1.20));
  checks.push_back(checkAtMost(
      "RMS excess IV points warm minus cold",
      metric(warm.rms_excess_vol_points() - cold.rms_excess_vol_points()),
      0.02, 0.05));
  checks.push_back(checkAtMost("warm p99 (milliseconds)",
                               percentile(summary.warm_seconds, 0.99) * 1000.0,
                               p99_target_ms, p99_hard_ms));
  checks.push_back(checkAtMost("fallback updates (percent)",
                               fallback_rate_percent,
                               fallback_target_percent,
                               fallback_hard_percent));
  checks.push_back(checkAtMost("warm failures", summary.warm_failures, 0.0,
                               0.0));
  checks.push_back(checkAtMost("warm invalid surfaces",
                               summary.warm_surface_quality.invalid_surfaces,
                               0.0, 0.0));
  checks.push_back(checkAtMost(
      "dense-grid butterfly/calendar violations",
      summary.warm_surface_quality.dense_grid_violations, 0.0, 0.0));
  if (summary.name == "quiet") {
    checks.push_back(checkAtMost(
        "quiet median non-regression vs 49fed2b (milliseconds)",
        percentile(summary.warm_seconds, 0.50) * 1000.0, 0.47, 0.47));
    checks.push_back(checkAtMost(
        "quiet p99 non-regression vs 49fed2b (milliseconds)",
        percentile(summary.warm_seconds, 0.99) * 1000.0, 7.6, 7.6));
  }
  return checks;
}

void printCheckTable(const Summary& summary) {
  std::cout << "quality_check_table=" << summary.name << '\n'
            << "metric | value | target | hard limit | target status | hard status\n";
  for (const auto& check : qualityChecks(summary)) {
    const char* target_status =
        !check.available ? "UNAVAILABLE"
                         : (check.targetPassed() ? "PASS" : "MISS");
    const char* hard_status =
        !check.available ? "UNAVAILABLE"
                         : (check.hardLimitPassed() ? "PASS" : "BREACH");
    std::cout << check.name << " | ";
    if (check.available) std::cout << check.value;
    else std::cout << "unavailable";
    std::cout << " | <= " << check.target << " | <= " << check.hard_limit
              << " | " << target_status << " | " << hard_status << '\n';
  }
}

Options parseOptions(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--help") {
      std::cout
          << "usage: essvi_bench [--expiries N] [--quotes-per-expiry N] "
             "[--repeats N] [--warm-updates N] [--scenario-updates N] "
             "[--quote-cap RATIO] [--input normalized_quotes.csv] "
             "[--parameters-out path] [--timings-out path] "
             "[--scenario quiet|normal|fast|front|all] [--seed N] "
             "[--replay-dir directory] [--replay-limit N] [--check]\n";
      std::exit(0);
    }
    if (argument == "--check") {
      options.check = true;
      continue;
    }
    if (i + 1 == argc)
      throw std::invalid_argument("missing value for " + argument);
    const std::string value = argv[++i];
    if (argument == "--expiries") options.expiries = parseSize(value);
    else if (argument == "--quotes-per-expiry")
      options.quotes_per_expiry = parseSize(value);
    else if (argument == "--repeats") options.repeats = parseSize(value);
    else if (argument == "--warm-updates")
      options.warm_updates = parseSize(value);
    else if (argument == "--scenario-updates")
      options.scenario_updates = parseSize(value);
    else if (argument == "--quote-cap")
      options.quote_cap = parseDouble(value);
    else if (argument == "--input") options.input = value;
    else if (argument == "--parameters-out") options.parameters_out = value;
    else if (argument == "--timings-out") options.timings_out = value;
    else if (argument == "--scenario") options.scenario = value;
    else if (argument == "--seed") options.seed = parseSeed(value);
    else if (argument == "--replay-dir") options.replay_dir = value;
    else if (argument == "--replay-limit")
      options.replay_limit = parseSize(value);
    else
      throw std::invalid_argument("unknown argument: " + argument);
  }

  if (!options.expiries || !options.quotes_per_expiry || !options.repeats ||
      !options.scenario_updates)
    throw std::invalid_argument("expiries, quote count, repeats, and scenario updates must be positive");
  if (options.quote_cap < 0.0)
    throw std::invalid_argument("quote cap must be non-negative");
  if (!options.scenario.empty() && options.scenario != "quiet" &&
      options.scenario != "normal" && options.scenario != "fast" &&
      options.scenario != "front" && options.scenario != "all")
    throw std::invalid_argument("invalid scenario: " + options.scenario);
  if (!options.scenario.empty() &&
      (!options.replay_dir.empty() || !options.input.empty()))
    throw std::invalid_argument("--scenario cannot be combined with --input or --replay-dir");
  if (!options.replay_dir.empty() && !options.input.empty())
    throw std::invalid_argument("--replay-dir and --input are mutually exclusive");
  if ((!options.scenario.empty() || !options.replay_dir.empty()) &&
      !options.parameters_out.empty())
    throw std::invalid_argument("--parameters-out is only supported by legacy mode");
  if (options.check && options.scenario.empty())
    throw std::invalid_argument("--check requires --scenario");
  return options;
}

bool runNewBenchmarks(const Options& options) {
  std::vector<Summary> summaries;
  if (!options.replay_dir.empty()) {
    Summary summary;
    runReplay(options, summary);
    printSummary(summary);
    summaries.push_back(std::move(summary));
  } else {
    const std::array<essvi::bench::WarmScenario, 4> scenarios{
        essvi::bench::WarmScenario::quiet,
        essvi::bench::WarmScenario::normal,
        essvi::bench::WarmScenario::fast,
        essvi::bench::WarmScenario::front};
    for (const auto scenario : scenarios) {
      const std::string name = essvi::bench::warmScenarioName(scenario);
      if (options.scenario != "all" && options.scenario != name) continue;
      Summary summary;
      runScenario(options, scenario, summary);
      printSummary(summary);
      if (options.check) printCheckTable(summary);
      summaries.push_back(std::move(summary));
    }
  }
  if (!options.timings_out.empty())
    writeJsonSummaries(options.timings_out, summaries, options.seed,
                       summaries.empty() ? 0 : summaries.front().samples);
  const bool target_missed = options.check && std::any_of(
      summaries.begin(), summaries.end(), [](const Summary& summary) {
        const auto checks = qualityChecks(summary);
        return essvi::bench::hasTargetMiss(checks);
      });
  return target_missed;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parseOptions(argc, argv);
    if (!options.scenario.empty() || !options.replay_dir.empty()) {
      const bool target_missed = runNewBenchmarks(options);
      return essvi::bench::qualityCheckExitCode(target_missed);
    }
    runLegacy(options);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
