add_subdirectory(fastorderbook)

add_library(mpmcqueue INTERFACE)
target_include_directories(mpmcqueue INTERFACE ${THIRD_PARTY}/MPMCSimpleQueue)

add_subdirectory(NanoLog)