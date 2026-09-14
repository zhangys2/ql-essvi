#include <ql/quantlib.hpp>
#include <cassert>

int main() {
  QuantLib::Date d(11, QuantLib::November, 2024);
  QuantLib::Calendar calendar = QuantLib::TARGET();
  assert(calendar.isBusinessDay(d));
  QuantLib::Settings::instance().evaluationDate() = d;
  return QuantLib::Settings::instance().evaluationDate() == d ? 0 : 1;
}
