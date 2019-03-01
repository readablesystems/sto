#include "clp.h"
#include "MicroBenchmarks.hh"

// global flags
ubench::UBenchParams ubench::params;

// global cpu frequency in GHz
double db_params::constants::processor_tsc_frequency;

enum {
    opt_ccid = 1,
    opt_type,
    opt_keysz,
    opt_ntrans,
    opt_nthrs,
    opt_opsptx,
    opt_opspro,
    opt_skew,
    opt_ropct,
    opt_wrpct,
    opt_time,
    opt_perf,
    opt_dump,
    opt_gran,
    opt_insm
};

static const Clp_Option options[] = {
    { "ccid",        'i', opt_ccid,   Clp_ValString,   Clp_Optional },
    { "datatype",    't', opt_type,   Clp_ValString,   Clp_Optional },
    { "keysize",     'k', opt_keysz,  Clp_ValUnsignedLong, Clp_Optional },
    { "ntrans",      'x', opt_ntrans, Clp_ValUnsigned, Clp_Optional },
    { "nthreads",    'j', opt_nthrs,  Clp_ValUnsigned, Clp_Optional },
    { "opspertrans", 0,   opt_opsptx, Clp_ValUnsigned, Clp_Optional },
    { "opsperro",    0,   opt_opspro, Clp_ValUnsigned, Clp_Optional },
    { "skew",        's', opt_skew,   Clp_ValDouble,   Clp_Optional },
    { "readonly",    0,   opt_ropct,  Clp_ValDouble,   Clp_Optional },
    { "writeratio",  0,   opt_wrpct,  Clp_ValDouble,   Clp_Optional },
    { "time",        'l', opt_time,   Clp_ValDouble,   Clp_Optional },
    { "perf",        'p', opt_perf,   Clp_NoVal,       Clp_Negate | Clp_Optional },
    { "dump",        'd', opt_dump,   Clp_NoVal,       Clp_Negate | Clp_Optional },
    { "granule",     'g', opt_gran,   Clp_ValUnsigned, Clp_Optional },
    { "measure",     'm', opt_insm,   Clp_NoVal,       Clp_Negate | Clp_Optional }
};

inline void print_usage(const char *prog) {
    std::stringstream ss;
    ss << "Usage: " << std::string(prog) << " [parameters...]" << std::endl
       << "List of accepted parameters:" << std::endl
       << "  --ccid=STRING (-i), concurrency control policy. Accepted options are:" << std::endl
       << "      default, opaque, 2pl, adaptive, swiss, tictoc" << std::endl
       << "  --datatype=STRING (-t), data structure used to run benchmark. (Now only supports masstree.)" << std::endl
       << "  --keysize=NUMBER (-k), size of key space used in the benchmark (like ARRAY_SZ), default 10 million" << std::endl
       << "  --ntrans=NUMBER (-x), number of pre-generated transactions (loops over once runs out), default 2 million" << std::endl
       << "  --nthreads=NUMBER (-j), number of concurrent threads, default 4" << std::endl
       << "  --opspertrans=NUMBER, number of operations per transaction, default 20" << std::endl
       << "  --opsperro=NUMBER, number of operations in read-only transactions, defaults to opspertrans value" << std::endl
       << "  --skew=FLOAT, Zipf skew (theta) parameter used to generate workload, default 0.8" << std::endl
       << "  --readonly=FRACTION, fraction of transactions that are read-only, default 0.8" << std::endl
       << "  --writeratio=FRACTION, fraction of operations in non-read-only transactions that are writes, default 0.2" << std::endl
       << "  --time=FLOAT (-l), duration (in seconds) for which the benchmark is run, default 5.0" << std::endl
       << "  --perf (-p), spawn perf profiler after the benchmark starts executing, default off" << std::endl
       << "  --dump (-d), dump the trace of all generated transactions (not functional for now)" << std::endl
       << "  --granule (-g) select the granularity of concurrency control" << std::endl
       << "  --measure (-m), enable instantaneous measurements of throughput and optimistic read rates, default off" << std::endl;

    std::cout << ss.str() << std::flush;
}

template <int G, typename P, bool InsMeasure>
using tester = typename std::conditional<InsMeasure,
        ubench::MtZipfTesterMeasure<G, P>,
        ubench::MtZipfTesterDefault<G, P>>::type;

template <int G, bool InsMeasure>
inline void instantiate_and_execute_testers() {
    using ubench::MtZipfTesterDefault;
    using ubench::MtZipfTesterMeasure;
    using ubench::params;
    using db_params::db_params_id;

    switch (params.dbid) {
        case db_params_id::Default: {
            typename tester<G, db_params::db_default_params, InsMeasure>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::Opaque: {
            typename tester<G, db_params::db_opaque_params, InsMeasure>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::TwoPL: {
            typename tester<G, db_params::db_2pl_params, InsMeasure>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::Adaptive: {
            typename tester<G, db_params::db_adaptive_params, InsMeasure>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::Swiss: {
            typename tester<G, db_params::db_swiss_params, InsMeasure>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::TicToc: {
            typename tester<G, db_params::db_tictoc_params, InsMeasure>::type t(params.nthreads);
            t.execute();
            break;
        }
        default:
            std::cerr << "invalid ccid" << std::endl;
            abort();
    }
}

template <int G>
inline void instantiate_and_exec_outer(bool ins_measure) {
    if (ins_measure)
        instantiate_and_execute_testers<G, true>();
    else
        instantiate_and_execute_testers<G, false>();
}

int main(int argc, const char *argv[]) {
    using ubench::params;
    using db_params::db_params_id;

    // Default parameter values
    params.dbid = db_params_id::Default;
    params.datatype = ubench::DsType::masstree;
    params.time_limit = 5.0;
    params.zipf_skew = 0.8;
    params.key_sz = 10000000;
    params.readonly_percent = 0.8;
    params.write_percent = 0.2;
    params.opspertrans = 20;
    params.opspertrans_ro = params.opspertrans;
    params.ntrans = 2000000;
    params.nthreads = 4;
    params.proc_frequency_hz = 0;
    params.dump_trace = false;
    params.profiler = false;
    params.granules = 1;
    params.ins_measure = false;

    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

    int opt;
    int ret = 0;
    bool clp_stop = false;
    bool ro_specified = false;
    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
        switch (opt) {
            case opt_ccid:
                params.dbid = db_params::parse_dbid(clp->val.s);
                break;
            case opt_type:
                params.datatype = ubench::parse_datatype(clp->val.s);
                break;
            case opt_keysz:
                params.key_sz = clp->val.ul;
                break;
            case opt_ntrans:
                params.ntrans = clp->val.u;
                break;
            case opt_nthrs:
                params.nthreads = clp->val.u;
                break;
            case opt_opsptx:
                params.opspertrans = clp->val.u;
                break;
            case opt_opspro:
                params.opspertrans_ro = clp->val.u;
                ro_specified = true;
                break;
            case opt_skew:
                params.zipf_skew = clp->val.d;
                break;
            case opt_ropct: {
                auto d = clp->val.d;
                always_assert(d <= 1.0 && d >= 0.0, "invalid readonly ratio");
                params.readonly_percent = d;}
                break;
            case opt_wrpct: {
                auto d = clp->val.d;
                always_assert(d <= 1.0 && d >= 0.0, "invalid write ratio");
                params.write_percent = d;}
                break;
            case opt_time: {
                auto d = clp->val.d;
                always_assert(d > 0.0, "invalid time limit");
                params.time_limit = d;}
                break;
            case opt_perf:
                params.profiler = !clp->negated;
                break;
            case opt_dump:
                params.dump_trace = !clp->negated;
                break;
            case opt_gran:
                params.granules = clp->val.u;
                break;
            case opt_insm:
                params.ins_measure = !clp->negated;
                break;
            default:
                print_usage(argv[0]);
                ret = 1;
                clp_stop = true;
                break;
        }
    }
    if (!ro_specified)
        params.opspertrans_ro = params.opspertrans;

    Clp_DeleteParser(clp);

    if (ret != 0)
        return ret;

    auto freq = determine_cpu_freq();
    if (freq == 0.0)
        return 1;
    db_params::constants::processor_tsc_frequency = freq;
    params.proc_frequency_hz = (uint64_t)(freq * db_params::constants::billion);

    always_assert(params.datatype == ubench::DsType::masstree, "Only Masstree is currently supported");

    switch (params.granules) {
        case 1:
            instantiate_and_exec_outer<1>(params.ins_measure);
            break;
        case 2:
            instantiate_and_exec_outer<2>(params.ins_measure);
            break;
        case 4:
            instantiate_and_exec_outer<4>(params.ins_measure);
            break;
        case 8:
            instantiate_and_exec_outer<8>(params.ins_measure);
            break;
        default:
            std::cerr << "Error: granularity=" << params.granules << " not supported." << std::endl;
            abort();
    }

    std::cout << params << std::endl;
    Transaction::print_stats();

    return 0;
}
