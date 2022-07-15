#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

int tpcc_tc_sts(int argc, char const* const* argv) {
    return tpcc_access<db_tictoc_sts_commute_params>::execute(argc, argv);
}
