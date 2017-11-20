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
        ContentionManager.cc
        VersionBase.hh
        OCCVersions.hh
        EagerVersions.hh
        TThread.hh
        ConcurrencyControl.hh
        Sto.hh
        TicTocVersions.hh)

add_executable(tpcc_bench
        PlatformFeatures.hh
        TPCC_bench.cc
        TPCC_structs.hh
        TPCC_index.hh
        TPCC_txns.hh)

add_executable(micro_bench
        PlatformFeatures.hh
        MicroBenchmarks.cc
        MicroBenchmarks.hh
        TPCC_index.hh
        TPCC_bench.hh)

add_executable(concurrent
        srandomdev.c
        concurrent.cc)

add_executable(unit-tflexarray
        unit-tflexarray.cc
        TFlexArray.hh)

add_executable(unit-swisstarray
        unit-swisstarray.cc
        SwissTArray.hh
        TArrayProxy.hh)

add_executable(unit-tarray
        unit-tarray.cc
        TArray.hh
        TArrayAdaptive.hh
        TArrayProxy.hh)

add_executable(unit-tbox
        unit-tbox.cc
        TBox.hh)

target_link_libraries(tpcc_bench sto clp masstree json dprint xxhash ${PLATFORM_LIBRARIES})
target_link_libraries(micro_bench sto clp masstree json dprint ${PLATFORM_LIBRARIES})
target_link_libraries(unit-swisstarray sto dprint)
target_link_libraries(unit-tflexarray sto dprint)
target_link_libraries(unit-tbox sto dprint)
target_link_libraries(unit-tarray sto dprint)
target_link_libraries(concurrent sto clp dprint ${PLATFORM_LIBRARIES})
