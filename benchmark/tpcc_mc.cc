#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

using db_params::db_split_type;

int tpcc_mc(int argc, char const* const* argv, db_split_type split) {
    switch (split) {
    case db_split_type::None:
        return tpcc_access<db_mvcc_commute_params>::execute(argc, argv);
    case db_split_type::Static:
        return tpcc_access<db_mvcc_sts_commute_params>::execute(argc, argv);
    case db_split_type::Adaptive:
        return tpcc_access<db_mvcc_ats_commute_params>::execute(argc, argv);
    }
    assert(false);
    return 1;
}
