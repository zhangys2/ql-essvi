cmake_minimum_required(VERSION 3.20)
project(ql_essvi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ESSVI_WITH_QUANTLIB "Build the QuantLib adapter when QuantLib is available" ON)
option(ESSVI_PROFILE_MM_BOUND "Instrument mmPsi2Bound calls in profiling builds" OFF)
option(ESSVI_NATIVE_OPTIMIZATION "Use -O3 -march=native for Release builds" OFF)
set(ESSVI_MAX_CALIBRATION_THREADS 4 CACHE STRING
    "Maximum worker threads for independent rho candidates (1-4)")
if(NOT ESSVI_MAX_CALIBRATION_THREADS MATCHES "^[1-4]$")
  message(FATAL_ERROR "ESSVI_MAX_CALIBRATION_THREADS must be an integer from 1 to 4")
endif()
find_package(Threads REQUIRED)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  if(ESSVI_NATIVE_OPTIMIZATION)
    add_compile_options("$<$<CONFIG:Release>:-O3>" "$<$<CONFIG:Release>:-march=native>")
  else()
    add_compile_options("$<$<CONFIG:Release>:-O2>")
  endif()
endif()

add_library(essvi_core cpp/essvi_surface.cpp cpp/calibrator.cpp)
target_include_directories(essvi_core PUBLIC cpp)
target_compile_options(essvi_core PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(essvi_core PUBLIC
  ESSVI_MAX_CALIBRATION_THREADS=${ESSVI_MAX_CALIBRATION_THREADS})
target_link_libraries(essvi_core PUBLIC Threads::Threads)
if(ESSVI_PROFILE_MM_BOUND)
  target_compile_definitions(essvi_core PUBLIC ESSVI_PROFILE_MM_BOUND)
endif()

add_executable(essvi_fit cpp/main.cpp)
target_link_libraries(essvi_fit PRIVATE essvi_core)

add_executable(essvi_bench bench/essvi_bench.cpp)
target_include_directories(essvi_bench PRIVATE bench)
target_link_libraries(essvi_bench PRIVATE essvi_core)

include(CTest)
if(BUILD_TESTING)
  target_compile_definitions(essvi_core PUBLIC ESSVI_TESTING)

  add_executable(essvi_tests tests/test_essvi.cpp)
  target_link_libraries(essvi_tests PRIVATE essvi_core)
  add_test(NAME essvi_tests COMMAND essvi_tests)

  add_executable(essvi_mm_bound_tests tests/test_mm_bound.cpp)
  target_link_libraries(essvi_mm_bound_tests PRIVATE essvi_core)
  add_test(NAME essvi_mm_bound_tests COMMAND essvi_mm_bound_tests)

  add_executable(essvi_calibrator_regression_tests
    tests/test_calibrator_regression.cpp)
  target_include_directories(essvi_calibrator_regression_tests PRIVATE bench)
  target_compile_definitions(essvi_calibrator_regression_tests PRIVATE
    ESSVI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(essvi_calibrator_regression_tests PRIVATE essvi_core)
  add_test(NAME essvi_calibrator_regression_tests
    COMMAND essvi_calibrator_regression_tests)

  add_executable(essvi_warm_calibrator_tests tests/test_warm_calibrator.cpp)
  target_include_directories(essvi_warm_calibrator_tests PRIVATE bench)
  target_link_libraries(essvi_warm_calibrator_tests PRIVATE essvi_core)
  add_test(NAME essvi_warm_calibrator_tests COMMAND essvi_warm_calibrator_tests)
endif()

if(ESSVI_WITH_QUANTLIB)
  find_package(QuantLib CONFIG QUIET
    PATHS "$ENV{QUANTLIB_BUILD}/cmake" "$ENV{HOME}/QuantLib/build/release/cmake")
  if(NOT QuantLib_FOUND)
    message(STATUS "QuantLib not found; disabling the QuantLib adapter")
    set(ESSVI_WITH_QUANTLIB OFF CACHE BOOL
        "Build the QuantLib adapter when QuantLib is available" FORCE)
  endif()
endif()

if(ESSVI_WITH_QUANTLIB)
  add_library(essvi_ql cpp/essvi_ql.cpp)
  target_include_directories(essvi_ql PUBLIC cpp)
  target_link_libraries(essvi_ql PUBLIC essvi_core QuantLib::QuantLib)
  target_compile_options(essvi_ql PRIVATE -Wall -Wextra -Wpedantic)

  if(BUILD_TESTING)
    add_executable(quantlib_smoke tests/test_quantlib.cpp)
    target_link_libraries(quantlib_smoke PRIVATE QuantLib::QuantLib)
    add_test(NAME quantlib_smoke COMMAND quantlib_smoke)

    add_executable(essvi_ql_tests tests/test_essvi_ql.cpp)
    target_link_libraries(essvi_ql_tests PRIVATE essvi_ql)
    add_test(NAME essvi_ql_tests COMMAND essvi_ql_tests)
  endif()
endif()
