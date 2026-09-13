#include "calibrator.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
  if (argc != 5) {
    std::cerr << "usage: essvi_fit normalized_quotes.csv parameters.csv fitted_quotes.csv surface_grid.csv\n";
    return 2;
  }
  auto quotes = essvi::readQuotes(argv[1]);
  auto result = essvi::calibrate(quotes);
  if (!result.converged) {
    std::cerr << "calibration failed: " << result.error << "\n";
    return 1;
  }
  essvi::writeParameters(result, argv[2]);
  std::ofstream qout(argv[3]);
  qout << "quote_id,time_to_expiry,strike,log_moneyness,fitted_total_variance,fitted_iv,status\n";
  qout << std::setprecision(15);
  for (const auto& q : quotes) {
    if (!q.included) { qout << q.id << ",,,,,,excluded\n"; continue; }
    const double k = std::log(q.strike / q.forward);
    const double w = result.surface.totalVariance(q.expiry, k);
    qout << q.id << ',' << q.expiry << ',' << q.strike << ',' << k << ',' << w << ','
         << std::sqrt(std::max(0.0, w / q.expiry)) << ",ok\n";
  }
  std::ofstream gout(argv[4]);
  gout << "time_to_expiry,forward,discount_factor,log_moneyness,strike,fitted_total_variance,fitted_iv,is_tail\n";
  gout << std::setprecision(15);
  for (const auto& s : result.surface.slices) {
    auto fwd = 0.0; for (const auto& q : quotes) if (q.expiry == s.expiry) { fwd=q.forward; break; }
    for (int i=0; i<=100; ++i) { const double k=-0.5+i*0.01; const double w=essvi::sliceTotalVariance(s,k);
      gout << s.expiry << ',' << fwd << ',' << ',' << k << ',' << fwd*std::exp(k) << ',' << w << ',' << std::sqrt(w/s.expiry) << ",true\n"; }
  }
  std::cout << "objective=" << result.objective << " slices=" << result.surface.slices.size() << '\n';
}
