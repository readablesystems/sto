#include "TPCC_bench.hh"
#include "TPCC_txns.hh"

using namespace tpcc;

int tpcc_o(int argc, char const* const* argv) {
    return tpcc_access<db_opaque_params>::execute(argc, argv);
}
