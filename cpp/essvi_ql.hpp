#pragma once

#include "essvi_surface.hpp"

#include <ql/termstructures/volatility/equityfx/blackvoltermstructure.hpp>
#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <vector>

namespace essvi {

// Per-expiry eSSVI smile. Total variance is canonical (D14): varianceImpl calls
// the plain evaluator; volatilityImpl is derived from it.
class EssviSmileSection : public QuantLib::SmileSection {
 public:
  EssviSmileSection(QuantLib::Time exerciseTime, QuantLib::Rate forward, Slice slice,
                    QuantLib::DayCounter dc = QuantLib::Actual365Fixed());
  EssviSmileSection(const QuantLib::Date& expiry, QuantLib::Rate forward, Slice slice,
                    const QuantLib::DayCounter& dc = QuantLib::Actual365Fixed());

  QuantLib::Real minStrike() const override { return 0.0; }
  QuantLib::Real maxStrike() const override { return QL_MAX_REAL; }
  QuantLib::Real atmLevel() const override { return forward_; }
  const Slice& slice() const { return slice_; }

 protected:
  QuantLib::Real varianceImpl(QuantLib::Rate strike) const override;
  QuantLib::Volatility volatilityImpl(QuantLib::Rate strike) const override;

 private:
  QuantLib::Rate forward_;
  Slice slice_;
};

// QuantLib surface adapter around the plain eSSVI evaluator (D12).
class EssviBlackVarianceSurface : public QuantLib::BlackVarianceTermStructure {
 public:
  EssviBlackVarianceSurface(
      const QuantLib::Date& referenceDate, Surface surface,
      std::vector<QuantLib::Real> forwards,
      QuantLib::DayCounter dc = QuantLib::Actual365Fixed(),
      const QuantLib::Calendar& cal = QuantLib::NullCalendar());

  QuantLib::Date maxDate() const override { return maxDate_; }
  QuantLib::Real minStrike() const override { return 0.0; }
  QuantLib::Real maxStrike() const override { return QL_MAX_REAL; }
  QuantLib::Real atmLevel(QuantLib::Time t) const override;
  const Surface& surface() const { return surface_; }

 protected:
  QuantLib::Real blackVarianceImpl(QuantLib::Time t, QuantLib::Real strike) const override;
  QuantLib::ext::shared_ptr<QuantLib::SmileSection> smileSectionImpl(
      QuantLib::Time t) const override;

 private:
  QuantLib::Real forwardAt(QuantLib::Time t) const;

  Surface surface_;
  std::vector<QuantLib::Real> forwards_;
  QuantLib::Date maxDate_;
};

}  // namespace essvi
