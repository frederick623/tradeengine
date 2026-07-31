add_subdirectory(fastorderbook)

add_subdirectory(MPMCSimpleQueue)

add_subdirectory(xdpio)

add_subdirectory(NanoLog)

if(Boost_FOUND)
    set(FIXER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fixer/CMakeLists.txt")
        add_subdirectory(fixer)
    else()
        include(FetchContent)
        FetchContent_Declare(
            fixer
            GIT_REPOSITORY https://github.com/frederick623/fixer.git
            GIT_TAG        main
        )
        FetchContent_MakeAvailable(fixer)
    endif()
else()
    message(STATUS "Boost.System not found — FIX broker support disabled")
endif()