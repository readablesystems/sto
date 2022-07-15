#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

using db_params::db_split_type;

int tpcc_d_ats(int argc, char const* const* argv) {
    return tpcc_access<db_default_ats_params>::execute(argc, argv);
}
