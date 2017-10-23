cmake_minimum_required(VERSION 3.8)
project(sto)

set(CMAKE_CXX_STANDARD 11)
set(PLATFORM_LIBRARIES pthread m)

include_directories(masstree-beta)
include_directories(third-party)
include_directories(third-party/xxHash)

add_subdirectory(third-party/xxHash)
add_subdirectory(masstree-beta)

add_library(clp clp.c)
add_library(sto
        Packer.cc
        TransScratch.hh
        Transaction.cc
        Transaction.hh
        TransItem.hh
        TransAlloc.hh
        Interface.hh
        TWrapped.hh
        TRcu.cc
        ContentionManager.cc VersionBase.hh OCCVersions.hh EagerVersions.hh.hh TThread.hh)

add_executable(tpcc_bench
        PlatformFeatures.hh
        TPCC_bench.cc
        TPCC_structs.hh
        TPCC_index.hh
        TPCC_txns.hh)

add_executable(unit-swisstarray
        unit-swisstarray.cc
        ContentionManager.cc
        SwissTArray.hh
        TArrayProxy.hh)

target_link_libraries(tpcc_bench sto clp masstree json dprint xxhash ${PLATFORM_LIBRARIES})
target_link_libraries(unit-swisstarray sto dprint)
