#include "quality_metrics.hpp"

#undef NDEBUG
#include <cassert>
#include <cmath>
#include <vector>

namespace {
void assertSameQuoteMetrics(const essvi::bench::QuoteExcessMetrics& actual,
                            const essvi::bench::QuoteExcessMetrics& expected) {
  assert(actual.quote_count == expected.quote_count);
  assert(actual.above_cap_count == expected.above_cap_count);
  assert(actual.maximum_ratio == expected.maximum_ratio);
  assert(actual.squared_ratio_sum == expected.squared_ratio_sum);
  assert(actual.summed_band_loss == expected.summed_band_loss);
  assert(actual.squared_excess_vol_points_sum ==
         expected.squared_excess_vol_points_sum);
  assert(actual.maximum_excess_vol_points ==
         expected.maximum_excess_vol_points);
}

void measuresPerQuoteVarianceAndVolPointExcess() {
  const std::vector<essvi::Quote> quotes{
      {"q", 1.0, 100.0, 1.0, 100.0, 0.2, 0.3, 0.25, true}};
  const essvi::Surface fit{{essvi::Slice{1.0, 0.1, 0.05, 0.0}}};
  const auto metrics =
      essvi::bench::measureExcessMetrics(quotes, fit, 0.1);

  assert(metrics.quote_count == 1);
  assert(metrics.above_cap_count == 1);
  assert(std::abs(metrics.maximum_ratio - 0.2) < 1e-12);
  assert(std::abs(metrics.rms_ratio() - 0.2) < 1e-12);
  assert(std::abs(metrics.summed_band_loss - 0.0001) < 1e-12);
  const double expected_excess_points = (std::sqrt(0.1) - 0.3) * 100.0;
  assert(std::abs(metrics.rms_excess_vol_points() - expected_excess_points) <
         1e-12);
}

void appliesWidthFloorAndExcludesInvalidRows() {
  const std::vector<essvi::Quote> quotes{
      {"locked", 1.0, 100.0, 1.0, 100.0, 0.2, 0.2, 0.2, true},
      {"excluded", 1.0, 100.0, 1.0, 100.0, 0.2, 0.3, 0.25, false},
      {"crossed", 1.0, 100.0, 1.0, 100.0, 0.3, 0.2, 0.25, true}};
  const essvi::Surface fit{{essvi::Slice{1.0, 0.040001, 0.01, 0.0}}};
  const auto metrics =
      essvi::bench::measureExcessMetrics(quotes, fit, 0.1);

  assert(essvi::bench::countValidTwoSidedBands(quotes) == 1);
  assert(metrics.quote_count == 1);
  assert(metrics.above_cap_count == 1);
  assert(std::abs(metrics.maximum_ratio - 1.0) < 1e-9);
  assert(std::abs(metrics.summed_band_loss - 1e-12) < 1e-16);
  assert(std::abs(metrics.share_above_cap() - 1.0) < 1e-12);
}

void requiresMatchedWarmAndColdQuoteCounts() {
  essvi::bench::QuoteExcessMetrics warm;
  essvi::bench::QuoteExcessMetrics cold;
  assert(!essvi::bench::hasPairedQuoteObservations(warm, cold));

  warm.quote_count = 4;
  cold.quote_count = 4;
  assert(essvi::bench::hasPairedQuoteObservations(warm, cold));

  cold.quote_count = 3;
  assert(!essvi::bench::hasPairedQuoteObservations(warm, cold));
}

void mergesOnlyCompletePerUpdateQuotePopulations() {
  essvi::bench::BenchmarkQuality empty_quality;
  empty_quality.checks = 19;
  empty_quality.paired_updates = 23;
  empty_quality.warm = {7, 2, 0.7, 1.4, 2.1, 3.2, 4.3};
  empty_quality.cold = {8, 3, 0.8, 1.5, 2.2, 3.3, 4.4};
  const auto original_quality = empty_quality;
  const essvi::bench::QuoteExcessMetrics zero_expected_warm{
      1, 1, 0.9, 1.6, 2.3, 3.4, 4.5};
  const essvi::bench::QuoteExcessMetrics zero_expected_cold{
      2, 1, 1.0, 1.7, 2.4, 3.5, 4.6};

  assert(!essvi::bench::mergePairedQuoteMetrics(
      empty_quality, zero_expected_warm, zero_expected_cold, 0));
  assert(empty_quality.checks == original_quality.checks);
  assert(empty_quality.paired_updates == original_quality.paired_updates);
  assertSameQuoteMetrics(empty_quality.warm, original_quality.warm);
  assertSameQuoteMetrics(empty_quality.cold, original_quality.cold);

  essvi::bench::BenchmarkQuality quality;
  essvi::bench::QuoteExcessMetrics warm;
  essvi::bench::QuoteExcessMetrics cold;

  warm.quote_count = 2;
  warm.summed_band_loss = 0.2;
  cold.quote_count = 2;
  cold.summed_band_loss = 0.1;

  assert(!essvi::bench::mergePairedQuoteMetrics(quality, warm, cold, 3));
  assert(quality.paired_updates == 0);
  assert(quality.warm.quote_count == 0);
  assert(quality.cold.quote_count == 0);

  warm.quote_count = 3;
  assert(!essvi::bench::mergePairedQuoteMetrics(quality, warm, cold, 3));
  assert(quality.paired_updates == 0);
  assert(quality.warm.quote_count == 0);
  assert(quality.cold.quote_count == 0);

  cold.quote_count = 3;
  assert(essvi::bench::mergePairedQuoteMetrics(quality, warm, cold, 3));
  assert(quality.paired_updates == 1);
  assert(quality.warm.quote_count == 3);
  assert(quality.cold.quote_count == 3);
  assert(std::abs(quality.warm.summed_band_loss - 0.2) < 1e-12);
  assert(std::abs(quality.cold.summed_band_loss - 0.1) < 1e-12);

  warm.quote_count = 2;
  assert(!essvi::bench::mergePairedQuoteMetrics(quality, warm, cold, 3));
  assert(quality.paired_updates == 1);
  assert(quality.warm.quote_count == 3);
  assert(quality.cold.quote_count == 3);
}

void separatesTargetAndHardLimitAndHandlesZeroReference() {
  const auto target_pass = essvi::bench::checkAtMost("boundary", 1.0, 1.0, 2.0);
  assert(target_pass.targetPassed());
  assert(target_pass.hardLimitPassed());

  const auto target_miss = essvi::bench::checkAtMost("target", 1.5, 1.0, 2.0);
  assert(!target_miss.targetPassed());
  assert(target_miss.hardLimitPassed());

  const auto hard_miss = essvi::bench::checkAtMost("hard", 2.1, 1.0, 2.0);
  assert(!hard_miss.targetPassed());
  assert(!hard_miss.hardLimitPassed());

  const auto zero_over_zero =
      essvi::bench::checkRatioAtMost("loss", 0.0, 0.0, 1.05, 1.2);
  assert(zero_over_zero.available);
  assert(zero_over_zero.value == 1.0);
  assert(zero_over_zero.targetPassed());

  const auto positive_over_zero =
      essvi::bench::checkRatioAtMost("loss", 0.1, 0.0, 1.05, 1.2);
  assert(positive_over_zero.available);
  assert(std::isinf(positive_over_zero.value));
  assert(!positive_over_zero.targetPassed());
  assert(!positive_over_zero.hardLimitPassed());
}

void reportsTargetMissAcrossChecks() {
  const std::vector<essvi::bench::QualityCheck> passing{
      essvi::bench::checkAtMost("pass", 0.5, 1.0, 2.0),
      essvi::bench::checkAtMost("boundary", 1.0, 1.0, 2.0)};
  assert(!essvi::bench::hasTargetMiss(passing));
  assert(essvi::bench::qualityCheckExitCode(
             essvi::bench::hasTargetMiss(passing)) == 0);

  const std::vector<essvi::bench::QualityCheck> target_miss{
      essvi::bench::checkAtMost("miss", 1.5, 1.0, 2.0)};
  assert(essvi::bench::hasTargetMiss(target_miss));
  assert(essvi::bench::qualityCheckExitCode(
             essvi::bench::hasTargetMiss(target_miss)) == 2);

  const std::vector<essvi::bench::QualityCheck> unavailable{
      {"unavailable", 0.0, 1.0, 2.0, false}};
  assert(essvi::bench::hasTargetMiss(unavailable));
  assert(essvi::bench::qualityCheckExitCode(
             essvi::bench::hasTargetMiss(unavailable)) == 2);
}
}  // namespace

int main() {
  measuresPerQuoteVarianceAndVolPointExcess();
  appliesWidthFloorAndExcludesInvalidRows();
  requiresMatchedWarmAndColdQuoteCounts();
  mergesOnlyCompletePerUpdateQuotePopulations();
  separatesTargetAndHardLimitAndHandlesZeroReference();
  reportsTargetMissAcrossChecks();
  return 0;
}
