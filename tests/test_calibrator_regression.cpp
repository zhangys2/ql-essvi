#include "calibrator.hpp"
#include "synthetic_chain.hpp"

#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
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

}  // namespace

int main() {
  matchesColdBaselineAndIsDeterministic();
  return 0;
}
