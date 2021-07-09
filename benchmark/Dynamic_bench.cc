#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

#include "Dynamic_bench.hh"
#include "Dynamic_txns.hh"
#include "PlatformFeatures.hh"

namespace dynamic {

/*
INITIALIZE_ADAPTER(ADAPTER_OF(ordered_value));
INITIALIZE_ADAPTER(ADAPTER_OF(unordered_value));
*/

const char* dynamic_input_generator::last_names[] = {
        "BAR", "OUGHT", "ABLE", "PRI", "PRES",
        "ESE", "ANTI", "CALLY", "ATION", "EING"};

}; // namespace dynamic

const Clp_Option options[] = {
        { "dbid",         'i', opt_dbid,  Clp_ValString, Clp_Optional },
        { "threshold",    'd', opt_thld,  Clp_ValInt,    Clp_Optional },
        { "nthreads",     't', opt_nthrs, Clp_ValInt,    Clp_Optional },
        { "time",         'l', opt_time,  Clp_ValDouble, Clp_Optional },
        { "perf",         'p', opt_perf,  Clp_NoVal,     Clp_Optional },
        { "perf-counter", 'c', opt_pfcnt, Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "gc",           'g', opt_gc,    Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "gc-rate",      'r', opt_gr,    Clp_ValInt,    Clp_Optional },
        { "node",         'n', opt_node,  Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "commute",      'x', opt_comm,  Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "verbose",      'v', opt_verb,  Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "mix",          'm', opt_mix,   Clp_ValInt,    Clp_Optional },
        { "ordered",      'o', opt_ordr,  Clp_ValInt,    Clp_Optional },
        { "adapt",        'a', opt_ada,   Clp_ValString, Clp_Optional },
        { "sample",       's', opt_samp,  Clp_NoVal,     Clp_Negate| Clp_Optional },
};

const char* workload_mix_names[] = { "Uniform global", "Per-record (r+w)", "Per-record (r^w)" };

const size_t noptions = arraysize(options);

using namespace dynamic;
using namespace db_params;

void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss << "Usage of " << std::string(argv_0) << ":" << std::endl
       << "  --dbid=<STRING> (or -i<STRING>)" << std::endl
       << "    Specify the type of DB concurrency control used. Can be one of the followings:" << std::endl
       << "      default, opaque, 2pl, adaptive, swiss, tictoc, defaultnode, mvcc, mvccnode" << std::endl
       << "  --threshold=<NUM> (or -d<NUM>)" << std::endl
       << "    Specify the workload switching threshold in ms (default 1000)." << std::endl
       << "  --nthreads=<NUM> (or -t<NUM>)" << std::endl
       << "    Specify the number of threads (or Dynamic workers/terminals, default 1)." << std::endl
       << "  --time=<NUM> (or -l<NUM>)" << std::endl
       << "    Specify the time (duration) for which the benchmark is run (default 10 seconds)." << std::endl
       << "  --perf (or -p)" << std::endl
       << "    Spawns perf profiler in record mode for the duration of the benchmark run." << std::endl
       << "  --perf-counter (or -c)" << std::endl
       << "    Spawns perf profiler in counter mode for the duration of the benchmark run." << std::endl
       << "  --gc (or -g)" << std::endl
       << "    Enable garbage collection (default false)." << std::endl
       << "  --gc-rate=<NUM> (or -r<NUM>)" << std::endl
       << "    Number of microseconds between GC epochs. Defaults to 100000." << std::endl
       << "  --adapt=<STRING> (or -a<STRING>)" << std::endl
       << "    Specify the type of workload adapter to use. Can be one of the following:" << std::endl
       << "      none (default), profile, global, inline" << std::endl
       << "  --sample (or -s)" << std::endl
       << "    Sample record columns instead of scanning all columns (default false)." << std::endl
       << "  --node (or -n)" << std::endl
       << "    Enable node tracking (default false)." << std::endl
       << "  --commute (or -x)" << std::endl
       << "    Enable commutative updates in MVCC (default false)." << std::endl
       << "  --verbose (or -v)" << std::endl
       << "    Enable verbose output (default false)." << std::endl
       << "  --ordered (or -o)" << std::endl
       << "    Execute using an ordered index (default false)." << std::endl
       << "  --mix=<NUM> or -m<NUM>" << std::endl
       << "    Specify workload mix:" << std::endl
       << "    0. Uniform global (default)" << std::endl
       << "    1. Per-record (read + write)" << std::endl
       << "    2. Per-record (read xor write)" << std::endl;

    std::cout << ss.str() << std::flush;
}

double constants::processor_tsc_frequency;
bench::dummy_row bench::dummy_row::row;

int main(int argc, const char *const *argv) {
    Sto::global_init();
    auto cpu_freq = determine_cpu_freq();
    if (cpu_freq == 0.0)
        return 1;
    else
        constants::processor_tsc_frequency = cpu_freq;

    db_params_id dbid = db_params_id::Default;
    int ret_code = 0;

    std::cout << "MALLOC: " <<
        #ifdef MALLOC
        MALLOC
        #else
        "undefined."
        #endif
    << std::endl;
    std::cout << "STO_USE_EXCEPTION: " <<
        #if STO_USE_EXCEPTION
        1
        #else
        0
        #endif
    << std::endl;
   
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

    int opt;
    bool clp_stop = false;
    bool node_tracking = false;
    bool enable_commute = false;

    db_params_adapter adapter_type = db_params_adapter::None;

    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
        switch (opt) {
        case opt_dbid:
            dbid = parse_dbid(clp->val.s);
            if (dbid == db_params_id::None) {
                std::cout << "Unsupported DB CC id: "
                    << ((clp->val.s == nullptr) ? "" : std::string(clp->val.s)) << std::endl;
                print_usage(argv[0]);
                ret_code = 1;
                clp_stop = true;
            }
            break;
        case opt_node:
            node_tracking = !clp->negated;
            break;
        case opt_comm:
            enable_commute = !clp->negated;
            break;
        case opt_ada:
            adapter_type = parse_adapter(clp->val.s);
            break;
        default:
            break;
        }
    }

    Clp_DeleteParser(clp);
    if (ret_code != 0)
        return ret_code;

    (void) node_tracking;

    switch (adapter_type) {
        case db_params_adapter::Profile:
        case db_params_adapter::Global:
            sto::AdapterConfig::Enable(sto::AdapterConfig::Global);
            break;
        case db_params_adapter::Inline:
            sto::AdapterConfig::Enable(sto::AdapterConfig::Inline);
            break;
        default:
            break;
    }

    switch (dbid) {
    case db_params_id::Default:
        if (enable_commute) {
            ret_code = dynamic_access<db_default_commute_params>::execute(argc, argv);
        } else {
            ret_code = dynamic_access<db_default_params>::execute(argc, argv);
        }
        break;
    /*
    case db_params_id::Opaque:
        if (enable_commute) {
            ret_code = dynamic_oc(argc, argv);
        } else {
            ret_code = dynamic_o(argc, argv);
        }
        if (node_tracking) {
            std::cerr << "Warning: No node tracking option for opaque versions." << std::endl;
        }
        break;
    */
    /*
    case db_params_id::TwoPL:
        ret_code = dynamic_access<db_2pl_params>::execute(argc, argv);
        break;
    case db_params_id::Adaptive:
        ret_code = dynamic_access<db_adaptive_params>::execute(argc, argv);
        break;
    */
    /*
    case db_params_id::Swiss:
        ret_code = dynamic_s(argc, argv);
        break;
    case db_params_id::TicToc:
        if (enable_commute) {
            if (node_tracking) {
                ret_code = dynamic_tcn(argc, argv);
            } else {
                ret_code = dynamic_tc(argc, argv);
            }
        } else {
            if (node_tracking) {
                ret_code = dynamic_tn(argc, argv);
            } else {
                ret_code = dynamic_t(argc, argv);
            }
        }
        break;
    */
    /*
    case db_params_id::MVCC:
        if (node_tracking && enable_commute) {
            ret_code = dynamic_mcn(argc, argv);
        } else if (node_tracking) {
            ret_code = dynamic_mn(argc, argv);
        } else if (enable_commute) {
            ret_code = dynamic_mc(argc, argv);
        } else {
            ret_code = dynamic_m(argc, argv);
        }
        break;
    */
    default:
        std::cerr << "unsupported db config parameter id" << std::endl;
        ret_code = 1;
        break;
    };

    return ret_code;
}
