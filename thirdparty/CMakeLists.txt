add_subdirectory(fastorderbook)

add_subdirectory(MPMCSimpleQueue)

add_subdirectory(xdpio)

add_subdirectory(NanoLog)

set(FIXER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(fixer)