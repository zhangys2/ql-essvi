cmake_minimum_required(VERSION 3.20)
project(ql_essvi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(QuantLib CONFIG REQUIRED
  PATHS "$ENV{QUANTLIB_BUILD}/cmake" "$ENV{HOME}/QuantLib/build/release/cmake")
find_package(NLopt CONFIG REQUIRED)

add_library(essvi cpp/essvi_surface.cpp cpp/calibrator.cpp)
target_include_directories(essvi PUBLIC cpp)
target_link_libraries(essvi PUBLIC QuantLib::QuantLib NLopt::nlopt)
target_compile_options(essvi PRIVATE -Wall -Wextra -Wpedantic)

add_executable(essvi_fit cpp/main.cpp)
target_link_libraries(essvi_fit PRIVATE essvi)

include(CTest)
if(BUILD_TESTING)
  add_executable(essvi_tests tests/test_essvi.cpp)
  target_link_libraries(essvi_tests PRIVATE essvi)
  add_test(NAME essvi_tests COMMAND essvi_tests)

  add_executable(quantlib_smoke tests/test_quantlib.cpp)
  target_link_libraries(quantlib_smoke PRIVATE QuantLib::QuantLib)
  add_test(NAME quantlib_smoke COMMAND quantlib_smoke)
endif()
