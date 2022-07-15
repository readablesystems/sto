#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

using db_params::db_split_type;

int tpcc_m_sts(int argc, char const* const* argv) {
    return tpcc_access<db_mvcc_sts_params>::execute(argc, argv);
}
