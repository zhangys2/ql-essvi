#pragma once
#include "essvi_surface.hpp"
#include <string>
#include <vector>

namespace essvi {
struct Quote { std::string id; double expiry, forward, strike, mid_iv; bool included; };
struct CalibrationResult { Surface surface; bool converged{}; double objective{}; int iterations{}; std::string error; };
CalibrationResult calibrate(const std::vector<Quote>& quotes);
std::vector<Quote> readQuotes(const std::string& path);
void writeParameters(const CalibrationResult& result, const std::string& path);
}
