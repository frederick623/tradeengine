cmake_minimum_required(VERSION 3.51)
project(NanoLog LANGUAGES CXX)

# NanoLog.cpp uses <format>, which requires C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Common compile options
# ---------------------------------------------------------------------------
add_compile_options(-g -O3)

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Google Benchmark (installed via: brew install google-benchmark)
# ---------------------------------------------------------------------------
find_package(benchmark REQUIRED)

# ---------------------------------------------------------------------------
# Boost.Log (installed via: brew install boost)
# boost_system was merged into header-only in Boost ≥ 1.74; omit it here.
# CMP0167: use Boost's own CMake config files rather than the legacy FindBoost.
# ---------------------------------------------------------------------------
cmake_policy(SET CMP0167 NEW)
find_package(Boost COMPONENTS log log_setup thread filesystem)

# ---------------------------------------------------------------------------
# NanoLog static library (compiled once, linked by all targets)
# ---------------------------------------------------------------------------
add_library(nanolog STATIC NanoLog.cpp)
target_include_directories(nanolog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nanolog PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# NanoLog vs Boost.Log — Google Benchmark
#   Only built when Boost (log, log_setup, thread, filesystem) is available.
#   Compares NanoLog guaranteed-async, Boost.Log async frontend, and
#   Boost.Log sync frontend across 1 / 2 / 4 concurrent threads.
#   Output logs go to /tmp/ to avoid polluting the build tree.
# ---------------------------------------------------------------------------
if(Boost_FOUND AND PROJECT_IS_TOP_LEVEL)
    add_executable(nanolog_vs_boostlog_benchmark
        nanolog_vs_boostlog_benchmark.cpp
    )
    target_compile_definitions(nanolog_vs_boostlog_benchmark PRIVATE
        BOOST_LOG_DYN_LINK          # match the Homebrew dynamic Boost.Log libs
    )
    target_link_libraries(nanolog_vs_boostlog_benchmark PRIVATE
        nanolog
        benchmark::benchmark
        Boost::log
        Boost::log_setup
        Boost::thread
        Boost::filesystem
    )
else()
    message(STATUS "Boost not found — skipping nanolog_vs_boostlog_benchmark")
endif()

