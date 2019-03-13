cmake_minimum_required(VERSION 3.8)
project(sto)

set(CMAKE_CXX_STANDARD 17)
if(APPLE)
    set(PLATFORM_LIBRARIES pthread m)
else()
    set(PLATFORM_LIBRARIES pthread m jemalloc)
endif()

include_directories(masstree-beta)
include_directories(third-party)
include_directories(third-party/xxHash)
include_directories(third-party/rpmalloc)

include_directories(lib sto-core legacy datatype benchmark)

add_subdirectory(third-party/xxHash)
add_subdirectory(masstree-beta)

add_subdirectory(lib)
add_subdirectory(sto-core)
add_subdirectory(datatype)

add_subdirectory(benchmark)
add_subdirectory(test)

add_library(rd srandomdev.c)
