#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

int tpcc_mc(int argc, char const* const* argv) {
    return tpcc_access<db_mvcc_commute_params>::execute(argc, argv);
}
