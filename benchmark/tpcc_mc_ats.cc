#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

using db_params::db_split_type;

int tpcc_mc_ats(int argc, char const* const* argv) {
    return tpcc_access<db_mvcc_ats_commute_params>::execute(argc, argv);
}
