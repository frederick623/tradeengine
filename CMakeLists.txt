cmake_minimum_required(VERSION 3.15)
project(tradeengine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

add_subdirectory(thirdparty)

# ── Optional dependencies for the live / replay packet sources ───────────────
#  libpcap is optional for pcap replay; Boost.Asio (header-only) enables the
#  live UDP / multicast connector when present.
find_path(PCAP_INCLUDE_DIR pcap.h)
find_library(PCAP_LIBRARY NAMES pcap)

find_package(Boost QUIET)

# ── Feed-handler executable ──────────────────────────────────────────────────
#  Header-only CRTP adapters live under adapter/; the common model and the
#  registry live at the repo root.  main.cpp drives the HKEX adapter from a
#  UDP / file / pcap packet source under feed/.
add_executable(tradeengine
    main.cpp
)
target_include_directories(tradeengine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/adapter
)
target_link_libraries(tradeengine
    PRIVATE
        nanolog
        orderbook
        mpmcqueue
        xdpio::ipc
)

if(PCAP_LIBRARY)
    target_compile_definitions(tradeengine PRIVATE TRADEENGINE_HAVE_PCAP)
    target_include_directories(tradeengine PRIVATE ${PCAP_INCLUDE_DIR})
    target_link_libraries(tradeengine PRIVATE ${PCAP_LIBRARY})
    message(STATUS "libpcap found — pcap replay enabled")
else()
    message(STATUS "libpcap not found — pcap replay disabled (file/udp only)")
endif()

if(Boost_FOUND)
    target_compile_definitions(tradeengine PRIVATE TRADEENGINE_HAVE_BOOST_ASIO)
    target_include_directories(tradeengine PRIVATE ${Boost_INCLUDE_DIRS})
    if(TARGET Boost::boost)
        target_link_libraries(tradeengine PRIVATE Boost::boost)
    endif()
    message(STATUS "Boost found — live UDP source enabled")
else()
    message(STATUS "Boost not found — live UDP source disabled (file/pcap only)")
endif()

if(APPLE)
    target_link_libraries(tradeengine PRIVATE "-framework CoreFoundation")
endif()

# ── Unit tests (GoogleTest via FetchContent) ─────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(testadapter
    tests/testadapter.cpp
)
target_include_directories(testadapter
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/adapter
)
target_link_libraries(testadapter
    PRIVATE
        nanolog
        orderbook
        mpmcqueue
        GTest::gtest
)

if(APPLE)
    target_link_libraries(testadapter PRIVATE "-framework CoreFoundation")
endif()

include(GoogleTest)
gtest_discover_tests(testadapter)
