#include "calibrator.hpp"
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: essvi_fit normalized_quotes.csv parameters.csv\n";
    return 2;
  }
  auto result = essvi::calibrate(essvi::readQuotes(argv[1]));
  if (!result.converged) {
    std::cerr << "calibration failed: " << result.error << "\n";
    return 1;
  }
  essvi::writeParameters(result, argv[2]);
  std::cout << "objective=" << result.objective
            << " slices=" << result.surface.slices.size() << '\n';
}
