#include "essvi_surface.hpp"

#include <cassert>
#include <cmath>

int main() {
  essvi::Slice s{1.0, 0.04, 0.20, -0.4};
  assert(std::abs(essvi::sliceTotalVariance(s, 0.0) - s.theta) < 1e-12);
  assert(essvi::butterflySafe(s));
  assert(essvi::sliceTotalVariance(s, 1.0) > 0.0);

  essvi::Surface surface{{{0.5, 0.04, 0.20, -0.4},
                          {1.0, 0.07, 0.30, -0.35}}};
  assert(essvi::validate(surface).empty());
  assert(surface.totalVariance(0.75, 0.0) > 0.04);
  return 0;
}
