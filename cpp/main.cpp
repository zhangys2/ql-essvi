#include "essvi_surface.hpp"

#include <iostream>

int main() {
  // Smoke-test executable; calibration and CSV I/O are added in the next phase.
  essvi::Surface surface{{{0.5, 0.04, 0.20, -0.4},
                          {1.0, 0.07, 0.30, -0.35}}};
  if (const auto error = essvi::validate(surface); !error.empty()) {
    std::cerr << error << '\n';
    return 1;
  }
  std::cout << surface.impliedVolatility(0.75, 0.0) << '\n';
  return 0;
}
