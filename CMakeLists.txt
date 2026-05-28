cmake_minimum_required(VERSION 3.51)
project(tradeengine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# Boost (Cobalt + Asio). Cobalt requires linking the compiled library.
set(Boost_USE_STATIC_LIBS ON)
set(BUILD_SHARED_LIBS OFF)

cmake_policy(SET CMP0167 NEW)
find_package(Boost REQUIRED COMPONENTS cobalt)

add_subdirectory(thirdparty)

add_executable(tradeengine
    src/engine.cpp
)
target_include_directories(tradeengine PRIVATE src src/hkex)
target_link_libraries(tradeengine
    PRIVATE
        nanolog
        orderbook
        mpmcqueue
        Boost::cobalt
)

if(APPLE)
    target_link_libraries(tradeengine PRIVATE "-framework CoreFoundation")
endif()
