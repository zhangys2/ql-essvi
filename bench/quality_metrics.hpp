#pragma once

#include "calibrator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace essvi::bench {

inline constexpr double kQualityCaps[]{0.05, 0.1, 0.2};
inline constexpr std::size_t kQualityCapCount =
    sizeof(kQualityCaps) / sizeof(kQualityCaps[0]);

struct QuoteExcessMetrics {
  std::size_t quote_count{};
  std::size_t above_cap_count{};
  std::size_t above_cap_counts[kQualityCapCount]{};
  double maximum_ratio{};
  double squared_ratio_sum{};
  double summed_band_loss{};
  double squared_excess_vol_points_sum{};
  double maximum_excess_vol_points{};

  double share_above_cap() const {
    return quote_count ? static_cast<double>(above_cap_count) /
                             static_cast<double>(quote_count)
                       : 0.0;
  }

  double share_above_cap(std::size_t cap_index) const {
    return quote_count && cap_index < kQualityCapCount
               ? static_cast<double>(above_cap_counts[cap_index]) /
                     static_cast<double>(quote_count)
               : 0.0;
  }

  double rms_ratio() const {
    return quote_count ? std::sqrt(squared_ratio_sum /
                                   static_cast<double>(quote_count))
                       : 0.0;
  }

  double rms_excess_vol_points() const {
    return quote_count
               ? std::sqrt(squared_excess_vol_points_sum /
                           static_cast<double>(quote_count))
               : 0.0;
  }

  void merge(const QuoteExcessMetrics& other) {
    quote_count += other.quote_count;
    above_cap_count += other.above_cap_count;
    for (std::size_t i = 0; i < kQualityCapCount; ++i)
      above_cap_counts[i] += other.above_cap_counts[i];
    maximum_ratio = std::max(maximum_ratio, other.maximum_ratio);
    squared_ratio_sum += other.squared_ratio_sum;
    summed_band_loss += other.summed_band_loss;
    squared_excess_vol_points_sum += other.squared_excess_vol_points_sum;
    maximum_excess_vol_points =
        std::max(maximum_excess_vol_points, other.maximum_excess_vol_points);
  }
};

inline bool hasPairedQuoteObservations(const QuoteExcessMetrics& warm,
                                       const QuoteExcessMetrics& cold) {
  return warm.quote_count > 0 && warm.quote_count == cold.quote_count;
}

struct BenchmarkQuality {
  std::size_t checks{};
  std::size_t paired_updates{};
  QuoteExcessMetrics warm;
  QuoteExcessMetrics cold;
};

inline bool mergePairedQuoteMetrics(BenchmarkQuality& quality,
                                    const QuoteExcessMetrics& warm,
                                    const QuoteExcessMetrics& cold,
                                    std::size_t expected_quote_count) {
  if (expected_quote_count == 0 ||
      warm.quote_count != expected_quote_count ||
      cold.quote_count != expected_quote_count)
    return false;

  quality.warm.merge(warm);
  quality.cold.merge(cold);
  ++quality.paired_updates;
  return true;
}

struct SurfaceQualityCounts {
  std::size_t invalid_surfaces{};
  std::size_t dense_grid_violations{};
};

struct QualityCheck {
  std::string name;
  double value{};
  double target{};
  double hard_limit{};
  bool available{true};

  bool targetPassed() const { return available && value <= target; }
  bool hardLimitPassed() const { return available && value <= hard_limit; }
};

inline QualityCheck checkAtMost(std::string name, double value,
                                double target, double hard_limit) {
  const bool available = std::isfinite(value) && std::isfinite(target) &&
                         std::isfinite(hard_limit);
  return {std::move(name), value, target, hard_limit, available};
}

inline QualityCheck checkRatioAtMost(std::string name, double numerator,
                                     double denominator, double target,
                                     double hard_limit) {
  if (denominator == 0.0) {
    if (numerator == 0.0)
      return checkAtMost(std::move(name), 1.0, target, hard_limit);
    if (numerator > 0.0)
      return {std::move(name), std::numeric_limits<double>::infinity(),
              target, hard_limit, true};
    auto check = checkAtMost(std::move(name), 0.0, target, hard_limit);
    check.available = false;
    return check;
  }
  return checkAtMost(std::move(name), numerator / denominator, target,
                     hard_limit);
}

inline bool hasTargetMiss(const std::vector<QualityCheck>& checks) {
  return std::any_of(checks.begin(), checks.end(), [](const QualityCheck& check) {
    return !check.targetPassed();
  });
}

inline int qualityCheckExitCode(bool target_missed) {
  return target_missed ? 2 : 0;
}

inline bool hasValidTwoSidedBand(const Quote& quote) {
  if (!quote.included || !std::isfinite(quote.expiry) || quote.expiry <= 0.0 ||
      !std::isfinite(quote.forward) || quote.forward <= 0.0 ||
      !std::isfinite(quote.strike) || quote.strike <= 0.0 ||
      !std::isfinite(quote.bid_iv) || !std::isfinite(quote.ask_iv) ||
      quote.bid_iv < 0.0 || quote.ask_iv < quote.bid_iv)
    return false;
  if (!std::isfinite(std::log(quote.strike / quote.forward))) return false;
  const double bid_variance = quote.expiry * quote.bid_iv * quote.bid_iv;
  const double ask_variance = quote.expiry * quote.ask_iv * quote.ask_iv;
  return std::isfinite(bid_variance) && std::isfinite(ask_variance);
}

inline std::size_t countValidTwoSidedBands(const std::vector<Quote>& quotes) {
  std::size_t count = 0;
  for (const auto& quote : quotes) count += hasValidTwoSidedBand(quote);
  return count;
}

inline QuoteExcessMetrics measureExcessMetrics(
    const std::vector<Quote>& quotes, const Surface& surface, double cap) {
  QuoteExcessMetrics metrics;
  for (const auto& quote : quotes) {
    if (!hasValidTwoSidedBand(quote)) continue;

    const double log_moneyness = std::log(quote.strike / quote.forward);
    const double fitted_variance =
        surface.totalVariance(quote.expiry, log_moneyness);
    if (!(std::isfinite(fitted_variance) && fitted_variance > 0.0)) continue;

    const double bid_variance = quote.expiry * quote.bid_iv * quote.bid_iv;
    const double ask_variance = quote.expiry * quote.ask_iv * quote.ask_iv;
    const double width = std::max(ask_variance - bid_variance, 1e-6);
    const double excess =
        std::max({0.0, bid_variance - fitted_variance,
                  fitted_variance - ask_variance});
    const double ratio = excess / width;
    const double fitted_iv = std::sqrt(fitted_variance / quote.expiry);
    const double excess_vol_points =
        100.0 * std::max({0.0, quote.bid_iv - fitted_iv,
                          fitted_iv - quote.ask_iv});

    ++metrics.quote_count;
    metrics.above_cap_count += ratio > cap;
    for (std::size_t i = 0; i < kQualityCapCount; ++i)
      metrics.above_cap_counts[i] += ratio > kQualityCaps[i];
    metrics.maximum_ratio = std::max(metrics.maximum_ratio, ratio);
    metrics.squared_ratio_sum += ratio * ratio;
    metrics.summed_band_loss += excess * excess;
    metrics.squared_excess_vol_points_sum +=
        excess_vol_points * excess_vol_points;
    metrics.maximum_excess_vol_points =
        std::max(metrics.maximum_excess_vol_points, excess_vol_points);
  }
  return metrics;
}

}  // namespace essvi::bench
