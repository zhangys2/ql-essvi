#include "calibrator.hpp"
#include "synthetic_chain.hpp"

#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

double number(const std::string& row, std::size_t column) {
  std::stringstream stream(row);
  std::string value;
  for (std::size_t i = 0; i <= column; ++i) {
    assert(std::getline(stream, value, ','));
  }
  return std::stod(value);
}

void writeQuoteCsv(const std::filesystem::path& path,
                   const std::vector<essvi::Quote>& quotes,
                   bool with_weight, const std::string& weight_value = "2") {
  std::ofstream output(path);
  output << "quote_id,effective_date,expiry_date,time_to_expiry,forward,"
            "discount_factor,strike,option_type,bid_iv,ask_iv,mid_iv,included,"
            "filter_reason";
  if (with_weight) output << ",weight";
  output << "\r\n" << std::setprecision(17);
  for (const auto& quote : quotes) {
    output << quote.id << ",2026-10-12,2027-10-12," << quote.expiry << ','
           << quote.forward << ',' << quote.discount_factor << ','
           << quote.strike << ",C," << quote.bid_iv << ',' << quote.ask_iv
           << ',' << quote.mid_iv << ','
           << (quote.included ? "true" : "false") << ',';
    if (with_weight) output << ',' << weight_value;
    output << "\r\n";
  }
  assert(output.good());
}

void matchesColdBaselineAndIsDeterministic() {
  const auto quotes = essvi::bench::syntheticChain(40, 100);
  const auto result = essvi::calibrate(quotes);
  const auto repeated = essvi::calibrate(quotes);
  assert(result.converged && repeated.converged);
  assert(result.surface.slices.size() == 40);
  assert(repeated.objective == result.objective);
  assert(repeated.iterations == result.iterations);
  assert(repeated.objective_evaluations == result.objective_evaluations);
  assert(repeated.mm_bound_calls == result.mm_bound_calls);
  assert(repeated.surface.slices.size() == result.surface.slices.size());
  for (std::size_t i = 0; i < result.surface.slices.size(); ++i) {
    const auto& first = result.surface.slices[i];
    const auto& second = repeated.surface.slices[i];
    assert(first.expiry == second.expiry);
    assert(first.theta == second.theta);
    assert(first.psi == second.psi);
    assert(first.rho == second.rho);
  }

  std::ifstream baseline(std::string(ESSVI_SOURCE_DIR) +
                         "/tests/baseline_essvi_40x100.csv");
  assert(baseline.good());
  std::string row;
  assert(std::getline(baseline, row));  // Header.
  for (std::size_t i = 0; i < result.surface.slices.size(); ++i) {
    assert(std::getline(baseline, row));
    const auto& slice = result.surface.slices[i];
    const double expected_expiry = number(row, 0);
    const double expected_theta = number(row, 1);
    const double expected_psi = number(row, 2);
    const double expected_rho = number(row, 3);
    assert(slice.expiry == expected_expiry);
    assert(std::abs(slice.theta - expected_theta) <=
           1e-9 * std::abs(expected_theta));
    assert(std::abs(slice.psi - expected_psi) <= 1e-9 * std::abs(expected_psi));
    assert(std::abs(slice.rho - expected_rho) <= 1e-9 * std::abs(expected_rho));
  }
  assert(!std::getline(baseline, row));
}

void legacyAndWeightedCsvControlColdObjective() {
  const auto source_quotes = essvi::bench::syntheticChain(3, 100);
  const essvi::Quote source_compatible{
      "source-compatible", 0.5, 100.0, 1.0, 105.0, 0.2, 0.21, 0.205, true};
  assert(source_compatible.weight == 1.0);

  const auto directory = std::filesystem::temp_directory_path() /
                         ("essvi-vega-" + std::to_string(
                              std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count()));
  std::filesystem::create_directories(directory);
  const auto legacy_path = directory / "legacy.csv";
  const auto weighted_path = directory / "weighted.csv";
  const auto diagnostics_path = directory / "diagnostics.csv";
  const auto blank_weight_path = directory / "blank-weight.csv";
  const auto invalid_weight_path = directory / "invalid-weight.csv";
  const auto malformed_weight_path = directory / "malformed-weight.csv";
  writeQuoteCsv(legacy_path, source_quotes, false);
  writeQuoteCsv(weighted_path, source_quotes, true);
  writeQuoteCsv(blank_weight_path, source_quotes, true, "");
  writeQuoteCsv(invalid_weight_path, source_quotes, true, "0");
  writeQuoteCsv(malformed_weight_path, source_quotes, true, "invalid");

  const auto legacy_quotes = essvi::readQuotes(legacy_path.string());
  const auto weighted_quotes = essvi::readQuotes(weighted_path.string());
  const auto blank_weight_quotes =
      essvi::readQuotes(blank_weight_path.string());
  const auto invalid_weight_quotes =
      essvi::readQuotes(invalid_weight_path.string());
  const auto malformed_weight_quotes =
      essvi::readQuotes(malformed_weight_path.string());
  assert(legacy_quotes.size() == source_quotes.size());
  assert(weighted_quotes.size() == source_quotes.size());
  assert(blank_weight_quotes.size() == source_quotes.size());
  assert(invalid_weight_quotes.size() == source_quotes.size());
  assert(malformed_weight_quotes.size() == source_quotes.size());
  for (std::size_t i = 0; i < source_quotes.size(); ++i) {
    assert(legacy_quotes[i].weight == 1.0);
    assert(weighted_quotes[i].weight == 2.0);
    assert(blank_weight_quotes[i].weight == 1.0);
  }
  const auto invalid_csv_result = essvi::calibrate(invalid_weight_quotes);
  assert(!invalid_csv_result.converged);
  assert(invalid_csv_result.error ==
         "included quote weight must be finite and positive");
  const auto malformed_csv_result =
      essvi::calibrate(malformed_weight_quotes);
  assert(!malformed_csv_result.converged);
  assert(malformed_csv_result.error ==
         "included quote weight must be finite and positive");

  const auto legacy_result = essvi::calibrate(legacy_quotes);
  const auto weighted_result = essvi::calibrate(weighted_quotes);
  assert(legacy_result.converged && weighted_result.converged);
  assert(std::abs(weighted_result.objective - 4.0 * legacy_result.objective) <=
         1e-12 * std::max(1.0, std::abs(weighted_result.objective)));
  essvi::writeDiagnostics(weighted_result, diagnostics_path.string());
  std::ifstream diagnostics(diagnostics_path);
  std::string header;
  assert(std::getline(diagnostics, header));
  assert(header ==
         "model,converged,objective,iterations,runtime_seconds,error");

  std::filesystem::remove_all(directory);
}

void includedWeightsMustBeFiniteAndPositive() {
  const auto valid_quotes = essvi::bench::syntheticChain(1, 40);
  const auto previous = essvi::calibrate(valid_quotes);
  assert(previous.converged);

  for (const double invalid_weight :
       {0.0, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
    auto invalid_quotes = valid_quotes;
    invalid_quotes.front().weight = invalid_weight;

    const auto cold = essvi::calibrate(invalid_quotes);
    assert(!cold.converged);
    assert(cold.error ==
           "included quote weight must be finite and positive");

    const auto warm = essvi::calibrateWarm(invalid_quotes, &previous,
                                           valid_quotes);
    assert(!warm.converged);
    assert(warm.error ==
           "included quote weight must be finite and positive");
  }
}

void weightedWarmLocalFitUsesWeightedEquations() {
  const auto previous_quotes = essvi::bench::syntheticChain(1, 80);
  auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);
  previous.reuse_ages.front() = 12;

  auto unit_quotes = previous_quotes;
  auto weighted_quotes = previous_quotes;
  for (std::size_t i = 0; i < unit_quotes.size(); ++i) {
    const double shift = i == 0 ? 0.02 : (i == unit_quotes.size() - 1 ? -0.02 : 0.0);
    if (shift == 0.0) continue;
    for (auto* quote : {&unit_quotes[i], &weighted_quotes[i]}) {
      quote->bid_iv += shift;
      quote->ask_iv += shift;
      quote->mid_iv += shift;
    }
  }
  weighted_quotes.front().weight = 20.0;

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 1000.0;
  const auto unit = essvi::calibrateWarm(
      unit_quotes, &previous, previous_quotes, options);
  const auto weighted = essvi::calibrateWarm(
      weighted_quotes, &previous, previous_quotes, options);
  assert(unit.converged && weighted.converged);
  assert(unit.local_solver_slices == 1 && weighted.local_solver_slices == 1);
  assert(unit.local_solver_starts > 0 && weighted.local_solver_starts > 0);
  assert(unit.local_objective_evaluations > 1 &&
         weighted.local_objective_evaluations > 1);

  const auto& unit_slice = unit.surface.slices.front();
  const auto& weighted_slice = weighted.surface.slices.front();
  const auto calculate_loss = [](const essvi::Slice& slice,
                                 const std::vector<essvi::Quote>& quotes) {
    double loss = 0.0;
    for (const auto& quote : quotes) {
      if (!quote.included) continue;
      const double k = std::log(quote.strike / quote.forward);
      const double variance = essvi::sliceTotalVariance(slice, k);
      const double bid = quote.expiry * quote.bid_iv * quote.bid_iv;
      const double ask = quote.expiry * quote.ask_iv * quote.ask_iv;
      const double excess = variance - std::clamp(variance, bid, ask);
      loss += quote.weight * quote.weight * excess * excess;
    }
    return loss;
  };
  assert(std::abs(unit.objective - calculate_loss(unit_slice, unit_quotes)) <
         1e-14);
  assert(std::abs(weighted.objective -
                  calculate_loss(weighted_slice, weighted_quotes)) < 1e-14);
  assert(std::abs(unit_slice.theta - weighted_slice.theta) > 1e-10 ||
         std::abs(unit_slice.psi - weighted_slice.psi) > 1e-10 ||
         std::abs(unit_slice.rho - weighted_slice.rho) > 1e-10);
}

void weightedWarmGateUsesSensitivityScale() {
  const auto previous_quotes = essvi::bench::syntheticChain(3, 100);
  const auto previous = essvi::calibrate(previous_quotes);
  assert(previous.converged);

  auto current_quotes = previous_quotes;
  const auto& slice = previous.surface.slices.front();
  auto quote = std::find_if(current_quotes.begin(), current_quotes.end(),
                            [&](const auto& candidate) {
                              return candidate.expiry == slice.expiry &&
                                     candidate.id == "0-0";
                            });
  assert(quote != current_quotes.end());
  const double k = std::log(quote->strike / quote->forward);
  const double fitted_variance = essvi::sliceTotalVariance(slice, k);
  constexpr double width = 1e-3;
  constexpr double unweighted_ratio = 0.1;
  const double ask_variance = fitted_variance - unweighted_ratio * width;
  quote->bid_iv = std::sqrt((ask_variance - width) / quote->expiry);
  quote->ask_iv = std::sqrt(ask_variance / quote->expiry);
  quote->weight = 2.0;

  essvi::WarmOptions options;
  options.quote_tol = 0.0;
  options.quote_cap = 0.15;
  const auto warm = essvi::calibrateWarm(
      current_quotes, &previous, previous_quotes, options);

  assert(warm.converged);
  assert(warm.fallback_reason_counts.loss_above_gate > 0);
  assert(warm.slices_refit > 0);
  assert(warm.slices_reused < current_quotes.size() / 100);
}

}  // namespace

int main() {
  matchesColdBaselineAndIsDeterministic();
  legacyAndWeightedCsvControlColdObjective();
  includedWeightsMustBeFiniteAndPositive();
  weightedWarmLocalFitUsesWeightedEquations();
  weightedWarmGateUsesSensitivityScale();
  return 0;
}
