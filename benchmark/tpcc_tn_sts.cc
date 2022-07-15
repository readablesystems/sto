#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

int tpcc_tn_sts(int argc, char const* const* argv) {
    return tpcc_access<db_tictoc_sts_node_params>::execute(argc, argv);
}
