add_subdirectory(fastorderbook)

add_subdirectory(MPMCSimpleQueue)

add_subdirectory(xdpio)

add_subdirectory(NanoLog)

if(Boost_FOUND)
    set(FIXER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(fixer)
else()
    message(STATUS "Boost.System not found — FIX broker support disabled")
endif()