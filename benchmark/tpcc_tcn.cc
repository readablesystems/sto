#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

int tpcc_tcn(int argc, char const* const* argv) {
    return tpcc_access<db_tictoc_commute_node_params>::execute(argc, argv);
}

