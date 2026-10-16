#include "calibrator.hpp"
#include "essvi_surface.hpp"
#include "realistic_warm.hpp"
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
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

struct Options {
  std::size_t expiries{40};
  std::size_t quotes_per_expiry{100};
  std::size_t repeats{9};
  std::size_t warm_updates{100};
  std::size_t scenario_updates{200};
  std::size_t warm_cold_check_interval{10};
  std::uint64_t seed{0x8f3f73b5cf1c9ad1ULL};
  std::string input;
  std::string parameters_out;
  std::string timings_out;
  std::string scenario;
  std::string replay_dir;
  std::size_t replay_limit{};
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
  std::size_t warm_invalid_surfaces{};
  std::size_t cold_invalid_surfaces{};
  std::size_t quality_checks{};
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
  std::string load_average_before;
  std::string load_average_after;
};

struct LegacyOptions {
  std::size_t expiries{};
  std::size_t quotes_per_expiry{};
  std::size_t repeats{};
  std::size_t warm_updates{};
  std::size_t warm_cold_check_interval{};
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

bool denseGridValid(const essvi::Surface& surface) {
  if (surface.slices.empty() || !essvi::validate(surface).empty()) return false;
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
      if (!(std::isfinite(variance[j]) && variance[j] > 0.0)) return false;
      if (has_previous &&
          variance[j] < previous_variance[j] - 1e-8)
        return false;
      call[j] = normalizedCall(100.0, strike[j], variance[j]);
    }
    for (std::size_t j = 1; j < 100; ++j) {
      const double left_slope = (call[j] - call[j - 1]) /
                                (strike[j] - strike[j - 1]);
      const double right_slope = (call[j + 1] - call[j]) /
                                 (strike[j + 1] - strike[j]);
      if (right_slope < left_slope - 1e-8) return false;
    }
    previous_variance = variance;
    has_previous = true;
  }
  return true;
}

void record(Summary& summary, const essvi::CalibrationResult& warm,
            const essvi::CalibrationResult& cold, double warm_seconds,
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
  if (warm.converged && !denseGridValid(warm.surface))
    ++summary.warm_invalid_surfaces;
  if (cold.converged && !denseGridValid(cold.surface))
    ++summary.cold_invalid_surfaces;
  if (warm.converged && std::isfinite(warm.objective))
    summary.warm_loss += warm.objective;
  if (cold.converged && std::isfinite(cold.objective))
    summary.cold_loss += cold.objective;
  summary.objective_evaluations += warm.objective_evaluations;
  summary.mm_bound_calls += warm.mm_bound_calls;
  summary.local_solver_slices += warm.local_solver_slices;
  summary.local_objective_evaluations += warm.local_objective_evaluations;
  summary.local_mm_bound_calls += warm.local_mm_bound_calls;
  summary.quality_checks += warm.quality_checks;
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

void printSummary(const Summary& summary) {
  const double samples = static_cast<double>(summary.samples);
  std::cout << std::setprecision(17)
            << "run=" << summary.name << '\n'
            << "load_average_before=" << summary.load_average_before << '\n'
            << "load_average_after=" << summary.load_average_after << '\n'
            << "samples=" << summary.samples << '\n'
            << "snapshots=" << summary.snapshots << '\n';
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
            << "warm_invalid_surfaces=" << summary.warm_invalid_surfaces << '\n'
            << "cold_invalid_surfaces=" << summary.cold_invalid_surfaces << '\n'
            << "summed_warm_band_loss=" << summary.warm_loss << '\n'
            << "summed_cold_band_loss=" << summary.cold_loss << '\n'
            << "warm_objective_evaluations_per_refit_slice="
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
            << "quality_checks=" << summary.quality_checks << '\n'
            << "quality_warm_loss=" << summary.quality_warm_loss << '\n'
            << "quality_reference_loss=" << summary.quality_reference_loss << '\n'
            << "quality_loss_scale=" << summary.quality_loss_scale << '\n'
            << "loss_tolerance=" << summary.loss_tolerance << '\n';
  if (summary.cold_loss > 0.0)
    std::cout << "warm_cold_loss_ratio="
              << summary.warm_loss / summary.cold_loss << '\n';
  else
    std::cout << "warm_cold_loss_ratio=undefined (cold loss is zero)\n";
}

void writeJsonTimes(std::ostream& output, const char* name,
                    const std::vector<double>& values) {
  output << "\"" << name << "\": {\"median\":" << percentile(values, 0.50)
         << ",\"p90\":" << percentile(values, 0.90)
         << ",\"p99\":" << percentile(values, 0.99)
         << ",\"max\":" << maximum(values) << '}';
}

void writeJsonSummary(std::ostream& output, const Summary& summary) {
  const double samples = static_cast<double>(summary.samples);
  output << "{\"run\":\"" << summary.name << "\","
         << "\"load_average_before\":\"" << summary.load_average_before << "\","
         << "\"load_average_after\":\"" << summary.load_average_after << "\","
         << "\"samples\":" << summary.samples << ','
         << "\"snapshots\":" << summary.snapshots << ',';
  writeJsonTimes(output, "warm_seconds", summary.warm_seconds);
  output << ',';
  writeJsonTimes(output, "cold_seconds", summary.cold_seconds);
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
         << ",\"warm_invalid_surfaces\":" << summary.warm_invalid_surfaces
         << ",\"cold_invalid_surfaces\":" << summary.cold_invalid_surfaces
         << ",\"summed_warm_band_loss\":" << summary.warm_loss
         << ",\"summed_cold_band_loss\":" << summary.cold_loss
         << ",\"warm_objective_evaluations_per_refit_slice\":"
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
         << ",\"quality_checks\":" << summary.quality_checks
         << ",\"quality_warm_loss\":" << summary.quality_warm_loss
         << ",\"quality_reference_loss\":" << summary.quality_reference_loss
         << ",\"quality_loss_scale\":" << summary.quality_loss_scale
         << ",\"loss_tolerance\":" << summary.loss_tolerance << '}';
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
  summary.load_average_before = loadAverage();
  auto previous_quotes = essvi::bench::realisticChain(
      options.expiries, options.quotes_per_expiry, options.seed);
  const auto initial = essvi::calibrate(previous_quotes);
  if (!initial.converged)
    throw std::runtime_error("initial scenario fit failed: " + initial.error);

  auto previous_surface = initial.surface;
  essvi::bench::Noise noise(options.seed + 1);
  essvi::WarmOptions warm_options;
  warm_options.cold_check_interval = options.warm_cold_check_interval;
  for (std::size_t update = 1; update <= options.scenario_updates; ++update) {
    auto current_quotes = previous_quotes;
    essvi::bench::evolveWarmScenario(
        current_quotes, options.expiries, options.quotes_per_expiry,
        scenario, update, noise);
    warm_options.update_number = update;

    const auto warm_start = Clock::now();
    const auto warm = essvi::calibrateWarm(
        current_quotes, previous_surface, previous_quotes, warm_options);
    const double warm_seconds =
        std::chrono::duration<double>(Clock::now() - warm_start).count();

    const auto cold_start = Clock::now();
    const auto cold = essvi::calibrate(current_quotes);
    const double cold_seconds =
        std::chrono::duration<double>(Clock::now() - cold_start).count();
    record(summary, warm, cold, warm_seconds, cold_seconds);

    if (warm.converged)
      previous_surface = warm.surface;
    else if (cold.converged)
      previous_surface = cold.surface;
    previous_quotes = std::move(current_quotes);
  }
  summary.load_average_after = loadAverage();
}

void runReplay(const Options& options, Summary& summary) {
  summary.name = "replay (Black-Scholes European stand-in for American IVs)";
  summary.load_average_before = loadAverage();
  auto files = replayFiles(options.replay_dir);
  if (options.replay_limit && files.size() > options.replay_limit)
    files.resize(options.replay_limit);
  if (files.size() < 2)
    throw std::runtime_error("replay needs at least two normalized snapshots");
  summary.snapshots = files.size();

  auto previous_quotes = essvi::readQuotes(files.front().string());
  if (previous_quotes.empty())
    throw std::runtime_error("no quotes in " + files.front().string());
  const auto initial = essvi::calibrate(previous_quotes);
  if (!initial.converged)
    throw std::runtime_error("initial replay fit failed: " + initial.error);
  auto previous_surface = initial.surface;

  essvi::WarmOptions warm_options;
  warm_options.cold_check_interval = options.warm_cold_check_interval;
  for (std::size_t file_index = 1; file_index < files.size(); ++file_index) {
    auto current_quotes = essvi::readQuotes(files[file_index].string());
    if (current_quotes.empty())
      throw std::runtime_error("no quotes in " + files[file_index].string());
    warm_options.update_number = file_index;

    const auto warm_start = Clock::now();
    const auto warm = essvi::calibrateWarm(
        current_quotes, previous_surface, previous_quotes, warm_options);
    const double warm_seconds =
        std::chrono::duration<double>(Clock::now() - warm_start).count();

    const auto cold_start = Clock::now();
    const auto cold = essvi::calibrate(current_quotes);
    const double cold_seconds =
        std::chrono::duration<double>(Clock::now() - cold_start).count();
    record(summary, warm, cold, warm_seconds, cold_seconds);

    if (warm.converged)
      previous_surface = warm.surface;
    else if (cold.converged)
      previous_surface = cold.surface;
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
         << "    \"cold_check_interval\": "
         << options.warm_cold_check_interval << ",\n"
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
  warm_options.quote_tol = 1e-6;
  warm_options.loss_tol = 1e-6;
  warm_options.cold_check_interval = options.warm_cold_check_interval;
  auto previous_quotes = quotes;
  auto previous_surface = cold.surface;
  essvi::CalibrationResult warm = cold;
  for (std::size_t i = 0; i < options.warm_updates; ++i) {
    auto current_quotes = previous_quotes;
    for (auto& quote : current_quotes) {
      if (quote.included && std::isfinite(quote.bid_iv)) quote.bid_iv += 1e-7;
      if (quote.included && std::isfinite(quote.ask_iv)) quote.ask_iv += 1e-7;
    }
    warm_options.update_number = i + 1;
    const auto start = Clock::now();
    warm = essvi::calibrateWarm(current_quotes, previous_surface,
                                previous_quotes, warm_options);
    warm_seconds.push_back(
        std::chrono::duration<double>(Clock::now() - start).count());
    if (!warm.converged)
      throw std::runtime_error("warm calibration failed: " + warm.error);
    previous_surface = warm.surface;
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
            << "warm_cold_check_interval="
            << options.warm_cold_check_interval << '\n'
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

Options parseOptions(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--help") {
      std::cout
          << "usage: essvi_bench [--expiries N] [--quotes-per-expiry N] "
             "[--repeats N] [--warm-updates N] [--scenario-updates N] "
             "[--warm-cold-check-interval N] [--input normalized_quotes.csv] "
             "[--parameters-out path] [--timings-out path] "
             "[--scenario quiet|normal|fast|front|all] [--seed N] "
             "[--replay-dir directory] [--replay-limit N]\n";
      std::exit(0);
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
    else if (argument == "--warm-cold-check-interval")
      options.warm_cold_check_interval = parseSize(value);
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
  return options;
}

void runNewBenchmarks(const Options& options) {
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
      summaries.push_back(std::move(summary));
    }
  }
  if (!options.timings_out.empty())
    writeJsonSummaries(options.timings_out, summaries, options.seed,
                       summaries.empty() ? 0 : summaries.front().samples);
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parseOptions(argc, argv);
    if (!options.scenario.empty() || !options.replay_dir.empty())
      runNewBenchmarks(options);
    else
      runLegacy(options);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
