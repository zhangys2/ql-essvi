#include "calibrator.hpp"
#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 6) {
    std::cerr << "usage: essvi_fit normalized_quotes.csv parameters.csv fitted_quotes.csv surface_grid.csv diagnostics.csv\n";
    return 2;
  }
  std::vector<essvi::Quote> quotes;
  try {
    quotes = essvi::readQuotes(argv[1]);
  } catch (const std::exception& error) {
    std::cerr << "input error: " << error.what() << '\n';
    return 1;
  }
  auto result = essvi::calibrate(quotes);
  essvi::writeDiagnostics(result, argv[5]);
  if (!result.converged) {
    std::cerr << "calibration failed: " << result.error << "\n";
    return 1;
  }
  essvi::writeParameters(result, argv[2]);
  std::ofstream qout(argv[3]);
  qout << "quote_id,time_to_expiry,strike,log_moneyness,fitted_total_variance,fitted_iv,status\n";
  qout << std::fixed << std::setprecision(17);
  for (const auto& q : quotes) {
    if (!q.included) { qout << q.id << ",,,,,,excluded\n"; continue; }
    const double k = std::log(q.strike / q.forward);
    const double w = result.surface.totalVariance(q.expiry, k);
    qout << q.id << ',' << q.expiry << ',' << q.strike << ',' << k << ',' << w << ','
         << std::sqrt(std::max(0.0, w / q.expiry)) << ",ok\n";
  }
  std::ofstream gout(argv[4]);
  gout << "time_to_expiry,forward,discount_factor,log_moneyness,strike,fitted_total_variance,fitted_iv,is_tail\n";
  gout << std::fixed << std::setprecision(17);
  for (const auto& s : result.surface.slices) {
    auto fwd = 0.0, df = 1.0, min_k = 1e9, max_k = -1e9; for (const auto& q : quotes) if (q.expiry == s.expiry) { fwd=q.forward; df=q.discount_factor; if(q.included){const auto k=std::log(q.strike/q.forward);min_k=std::min(min_k,k);max_k=std::max(max_k,k);} }
    for (int i=0; i<=100; ++i) { const double k=-0.5+i*0.01; const double w=essvi::sliceTotalVariance(s,k); const bool tail=k<min_k||k>max_k;
      gout << s.expiry << ',' << fwd << ',' << df << ',' << k << ',' << fwd*std::exp(k) << ',' << w << ',' << std::sqrt(w/s.expiry) << ',' << (tail?"true":"false") << "\n"; }
  }
  std::cout << "objective=" << result.objective << " slices=" << result.surface.slices.size() << '\n';
}
