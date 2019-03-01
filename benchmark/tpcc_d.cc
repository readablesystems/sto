#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

int tpcc_d(int argc, char const* const* argv) {
    return tpcc_access<db_default_params>::execute(argc, argv);
}
