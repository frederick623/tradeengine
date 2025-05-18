cmake_minimum_required(VERSION 4.0)
project(TradeEngine LANGUAGES CXX)

# Set C++ standard to C++20
set(CMAKE_BUILD_TYPE Debug) 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

# Find Boost (version 1.85 or higher for Cobalt)
find_package(Boost 1.86 REQUIRED COMPONENTS system cobalt)

# Check if Boost was found
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost 1.85 or higher is required. Please set BOOST_ROOT to your Boost installation directory.")
endif()

# Include Boost headers
include_directories(${Boost_INCLUDE_DIRS})

# Add the executable
add_executable(replayserver replayserver.cpp)

# Link against Boost libraries and pthread
target_link_libraries(replayserver PRIVATE Boost::system Boost::cobalt)

# Enable warnings and optimizations
target_compile_options(replayserver PRIVATE -Wall -Wextra -O2)

# Optional: Set output directory for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)