#pragma once

#include "synthetic_chain.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace essvi::bench {

enum class WarmScenario { quiet, normal, fast, front };

inline WarmScenario parseWarmScenario(const std::string& name) {
  if (name == "quiet") return WarmScenario::quiet;
  if (name == "normal") return WarmScenario::normal;
  if (name == "fast") return WarmScenario::fast;
  if (name == "front") return WarmScenario::front;
  throw std::invalid_argument("unknown warm scenario: " + name);
}

inline const char* warmScenarioName(WarmScenario scenario) {
  switch (scenario) {
    case WarmScenario::quiet: return "quiet";
    case WarmScenario::normal: return "normal";
    case WarmScenario::fast: return "fast";
    case WarmScenario::front: return "front";
  }
  return "unknown";
}

inline std::vector<Quote> realisticChain(std::size_t expiry_count,
                                         std::size_t quotes_per_expiry,
                                         std::uint64_t seed) {
  std::vector<Quote> quotes;
  quotes.reserve(expiry_count * quotes_per_expiry);
  Noise noise(seed);
  for (std::size_t i = 0; i < expiry_count; ++i) {
    const double expiry = 0.05 + 0.05 * static_cast<double>(i);
    const double fraction = expiry_count > 1
                                ? static_cast<double>(i) /
                                      static_cast<double>(expiry_count - 1)
                                : 0.5;
    const Slice truth{expiry, 0.02 + 0.03 * expiry,
                      0.05 + 0.02 * expiry, -0.5 + 0.2 * fraction};
    for (std::size_t j = 0; j < quotes_per_expiry; ++j) {
      const double k = quotes_per_expiry == 1
                           ? 0.0
                           : -0.5 + static_cast<double>(j) /
                                         static_cast<double>(quotes_per_expiry - 1);
      const double strike = 100.0 * std::exp(k);
      const double iv = std::sqrt(sliceTotalVariance(truth, k) / expiry);
      const double mid = iv + noise.centered() * 0.00005;
      const double half_spread = 0.003 * (1.0 + 2.0 * std::abs(k));
      quotes.push_back({std::to_string(i) + "-" + std::to_string(j), expiry,
                        100.0, 1.0, strike,
                        std::max(0.001, mid - half_spread),
                        mid + half_spread, mid, true});
    }
  }
  return quotes;
}

inline double signedAmplitude(double amplitude, Noise& noise) {
  return noise.centered() < 0.0 ? -amplitude : amplitude;
}

inline void evolveWarmScenario(std::vector<Quote>& quotes,
                               std::size_t expiry_count,
                               std::size_t quotes_per_expiry,
                               WarmScenario scenario,
                               std::size_t update_number,
                               Noise& noise) {
  double level_amplitude = 0.0;
  double tilt_amplitude = 0.0;
  double idiosyncratic_amplitude = 0.0;
  double quote_noise_amplitude = 0.0;
  switch (scenario) {
    case WarmScenario::quiet:
      tilt_amplitude = 0.00005;
      idiosyncratic_amplitude = 0.00002;
      quote_noise_amplitude = 0.00001;
      break;
    case WarmScenario::normal:
      level_amplitude = 0.002;
      tilt_amplitude = 0.0005;
      idiosyncratic_amplitude = 0.0002;
      quote_noise_amplitude = 0.00005;
      break;
    case WarmScenario::fast:
      level_amplitude = 0.01;
      tilt_amplitude = 0.002;
      idiosyncratic_amplitude = 0.0002;
      quote_noise_amplitude = 0.0001;
      break;
    case WarmScenario::front:
      tilt_amplitude = 0.0002;
      idiosyncratic_amplitude = 0.00005;
      quote_noise_amplitude = 0.00003;
      break;
  }

  const double common_level = signedAmplitude(level_amplitude, noise);
  const double common_tilt = signedAmplitude(tilt_amplitude, noise);
  const double quiet_level = signedAmplitude(0.0005, noise);
  const double front_level = signedAmplitude(0.004, noise);
  const double back_level = front_level > 0.0 ? 0.0005 : -0.0005;
  const std::size_t active_quiet_group = expiry_count == 0
                                             ? 0
                                             : (update_number - 1) % 10;
  for (std::size_t expiry_index = 0; expiry_index < expiry_count;
       ++expiry_index) {
    double expiry_level = common_level;
    if (scenario == WarmScenario::quiet) {
      const bool active = expiry_index % 10 == active_quiet_group;
      expiry_level = active ? quiet_level : 0.0;
    } else if (scenario == WarmScenario::front) {
      expiry_level = expiry_index < expiry_count / 4 ? front_level : back_level;
    }
    const double idiosyncratic =
        noise.centered() * idiosyncratic_amplitude;
    const std::size_t begin = expiry_index * quotes_per_expiry;
    const std::size_t end = std::min(begin + quotes_per_expiry, quotes.size());
    for (std::size_t i = begin; i < end; ++i) {
      auto& quote = quotes[i];
      const double k = std::log(quote.strike / quote.forward);
      const double tilt = common_tilt * std::clamp(2.0 * k, -1.0, 1.0);
      const double change = expiry_level + idiosyncratic + tilt +
                            noise.centered() * quote_noise_amplitude;
      quote.bid_iv = std::max(0.001, quote.bid_iv + change);
      quote.ask_iv = std::max(quote.bid_iv, quote.ask_iv + change);
      quote.mid_iv = std::max(0.001, quote.mid_iv + change);
    }
  }
}

}  // namespace essvi::bench
