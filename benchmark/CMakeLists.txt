add_executable(tpcc_bench TPCC_bench.cc)
add_executable(ycsb_bench YCSB_bench.cc)
add_executable(micro_bench MicroBenchmarks.cc)

target_link_libraries(tpcc_bench sto clp masstree json dprint xxhash ${PLATFORM_LIBRARIES})
target_link_libraries(ycsb_bench sto clp masstree json dprint xxhash ${PLATFORM_LIBRARIES})
target_link_libraries(micro_bench sto clp masstree json dprint ${PLATFORM_LIBRARIES})
