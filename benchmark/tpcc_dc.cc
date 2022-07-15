#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

using db_params::db_split_type;

int tpcc_dc(int argc, char const* const* argv, db_split_type split) {
    switch (split) {
    case db_split_type::None:
        return tpcc_access<db_default_commute_params>::execute(argc, argv);
    case db_split_type::Static:
        return tpcc_dc_sts(argc, argv);
    case db_split_type::Adaptive:
        return tpcc_dc_ats(argc, argv);
    default:
        break;
    }
    assert(false);
    return 1;
}
