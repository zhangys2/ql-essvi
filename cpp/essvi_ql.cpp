#include "essvi_ql.hpp"

#include <ql/errors.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace essvi {
namespace {

QuantLib::Real logMoneyness(QuantLib::Rate strike, QuantLib::Rate forward) {
  QL_REQUIRE(forward > 0.0, "eSSVI forward must be positive");
  return std::log(std::max(strike, 1e-12) / forward);
}

}  // namespace

EssviSmileSection::EssviSmileSection(QuantLib::Time exerciseTime, QuantLib::Rate forward,
                                     Slice slice, QuantLib::DayCounter dc)
    : QuantLib::SmileSection(exerciseTime, std::move(dc)),
      forward_(forward),
      slice_(slice) {
  QL_REQUIRE(forward_ > 0.0, "eSSVI forward must be positive");
  QL_REQUIRE(exerciseTime > 0.0, "eSSVI smile section needs positive expiry");
  slice_.expiry = exerciseTime;
}

EssviSmileSection::EssviSmileSection(const QuantLib::Date& expiry, QuantLib::Rate forward,
                                     Slice slice, const QuantLib::DayCounter& dc)
    : QuantLib::SmileSection(expiry, dc, QuantLib::Date()),
      forward_(forward),
      slice_(slice) {
  QL_REQUIRE(forward_ > 0.0, "eSSVI forward must be positive");
  QL_REQUIRE(exerciseTime() > 0.0, "eSSVI smile section needs positive expiry");
  slice_.expiry = exerciseTime();
}

QuantLib::Real EssviSmileSection::varianceImpl(QuantLib::Rate strike) const {
  return sliceTotalVariance(slice_, logMoneyness(strike, forward_));
}

QuantLib::Volatility EssviSmileSection::volatilityImpl(QuantLib::Rate strike) const {
  const QuantLib::Time t = exerciseTime();
  return std::sqrt(std::max(0.0, varianceImpl(strike) / t));
}

EssviBlackVarianceSurface::EssviBlackVarianceSurface(
    const QuantLib::Date& referenceDate, Surface surface, std::vector<QuantLib::Real> forwards,
    QuantLib::DayCounter dc, const QuantLib::Calendar& cal)
    : QuantLib::BlackVarianceTermStructure(referenceDate, cal, QuantLib::Following, std::move(dc)),
      surface_(std::move(surface)),
      forwards_(std::move(forwards)) {
  QL_REQUIRE(!surface_.slices.empty(), "eSSVI surface has no slices");
  QL_REQUIRE(forwards_.size() == surface_.slices.size(), "need one forward per eSSVI slice");
  for (std::size_t i = 0; i < surface_.slices.size(); ++i) {
    QL_REQUIRE(forwards_[i] > 0.0, "eSSVI forward must be positive");
    QL_REQUIRE(surface_.slices[i].expiry > 0.0, "eSSVI slice expiry must be positive");
    if (i) {
      QL_REQUIRE(surface_.slices[i].expiry > surface_.slices[i - 1].expiry,
                 "eSSVI slices must be strictly increasing in expiry");
    }
  }
  const QuantLib::Time tmax = surface_.slices.back().expiry;
  const auto n = static_cast<QuantLib::Integer>(std::ceil(tmax * 365.0 - 1e-12));
  maxDate_ = referenceDate + std::max(n, QuantLib::Integer(1));
  enableExtrapolation();
}

QuantLib::Real EssviBlackVarianceSurface::forwardAt(QuantLib::Time t) const {
  const auto& slices = surface_.slices;
  if (slices.size() == 1 || t <= slices.front().expiry) return forwards_.front();
  if (t >= slices.back().expiry) return forwards_.back();
  auto hi = std::lower_bound(slices.begin(), slices.end(), t,
                             [](const Slice& s, QuantLib::Time x) { return s.expiry < x; });
  const auto j = static_cast<std::size_t>(hi - slices.begin());
  const double u =
      (t - slices[j - 1].expiry) / (slices[j].expiry - slices[j - 1].expiry);
  return forwards_[j - 1] + u * (forwards_[j] - forwards_[j - 1]);
}

QuantLib::Real EssviBlackVarianceSurface::atmLevel(QuantLib::Time t) const {
  return forwardAt(t);
}

QuantLib::Real EssviBlackVarianceSurface::blackVarianceImpl(QuantLib::Time t,
                                                           QuantLib::Real strike) const {
  const QuantLib::Time te = t == 0.0 ? surface_.slices.front().expiry : t;
  const QuantLib::Real F = forwardAt(te);
  return surface_.totalVariance(te, logMoneyness(strike, F));
}

QuantLib::ext::shared_ptr<QuantLib::SmileSection>
EssviBlackVarianceSurface::smileSectionImpl(QuantLib::Time t) const {
  const QuantLib::Time te = t == 0.0 ? surface_.slices.front().expiry : t;
  return QuantLib::ext::make_shared<EssviSmileSection>(te, forwardAt(te), surface_.sliceAt(te),
                                                       dayCounter());
}

}  // namespace essvi
