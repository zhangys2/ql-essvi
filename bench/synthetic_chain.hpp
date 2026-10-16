#pragma once

#include "calibrator.hpp"
#include "essvi_surface.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace essvi::bench {

class Noise {
 public:
  explicit Noise(std::uint64_t seed = 0x8f3f73b5cf1c9ad1ULL)
      : state_(seed ? seed : 1) {}

  double centered() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 7;
    state_ ^= state_ << 17;
    return 2.0 * static_cast<double>(state_ >> 11) *
               (1.0 / 9007199254740992.0) -
           1.0;
  }

 private:
  std::uint64_t state_;
};

// The cold-regression fixture: keep its legacy parameters and band widths stable.
inline std::vector<Quote> syntheticChain(
    std::size_t expiry_count, std::size_t quotes_per_expiry,
    std::uint64_t seed = 0x8f3f73b5cf1c9ad1ULL) {
  std::vector<Quote> quotes;
  quotes.reserve(expiry_count * quotes_per_expiry);
  Noise noise(seed);
  for (std::size_t i = 0; i < expiry_count; ++i) {
    const double expiry = 0.05 + 0.05 * static_cast<double>(i);
    const Slice truth{expiry, 0.02 + 0.03 * expiry,
                      0.05 + 0.02 * expiry, -0.35};
    for (std::size_t j = 0; j < quotes_per_expiry; ++j) {
      const double k = quotes_per_expiry == 1
                           ? 0.0
                           : -0.5 + static_cast<double>(j) /
                                         static_cast<double>(quotes_per_expiry - 1);
      const double strike = 100.0 * std::exp(k);
      const double iv = std::sqrt(sliceTotalVariance(truth, k) / expiry);
      const double mid = iv + noise.centered() * 0.00001;
      quotes.push_back({std::to_string(i) + "-" + std::to_string(j), expiry,
                        100.0, 1.0, strike, std::max(0.001, iv - 0.00005),
                        iv + 0.00005, mid, true});
    }
  }
  return quotes;
}

}  // namespace essvi::bench
