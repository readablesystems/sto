add_library(db_index DB_index.cc DB_index.hh)

set(COMMON_HEADERS ../lib/sampling.hh)

add_executable(tpcc_bench TPCC_bench.cc TPCC_structs.hh DB_structs.hh DB_params.hh DB_profiler.hh ${COMMON_HEADERS})
add_executable(ycsb_bench YCSB_bench.cc YCSB_structs.hh DB_structs.hh DB_params.hh DB_profiler.hh ${COMMON_HEADERS})
add_executable(micro_bench MicroBenchmarks.cc Micro_structs.hh ${COMMON_HEADERS})
add_executable(wiki_bench Wikipedia_structs.hh Wikipedia_bench.hh Wikipedia_txns.hh Wikipedia_loader.hh Wikipedia_data.cc ${COMMON_HEADERS})

target_link_libraries(tpcc_bench db_index sto clp masstree json dprint xxhash ${PLATFORM_LIBRARIES})
target_link_libraries(ycsb_bench db_index sto clp masstree json dprint xxhash ${PLATFORM_LIBRARIES})
target_link_libraries(micro_bench db_index sto clp masstree json dprint ${PLATFORM_LIBRARIES})
