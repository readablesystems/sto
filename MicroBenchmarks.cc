#include "TPCC_bench.hh"
#include "MicroBenchmarks.hh"

// global flags
ubench::UBenchParams ubench::params;

// global cpu frequency in GHz
double tpcc::constants::processor_tsc_frequency;

enum {
    opt_ccid = 1,
    opt_type,
    opt_ntrans,
    opt_nthrs,
    opt_opsptx,
    opt_opspro,
    opt_ropct,
    opt_wrpct,
    opt_time,
    opt_perf,
    opt_dump,
    opt_insm
};

static const Clp_Option options[] = {
    { "ccid",        'i', opt_ccid,   Clp_ValString,   Clp_Optional },
    { "datatype",    't', opt_type,   Clp_ValString,   Clp_Optional },
    { "ntrans",      'x', opt_ntrans, Clp_ValUnsigned, Clp_Optional },
    { "nthreads",    'j', opt_nthrs,  Clp_ValUnsigned, Clp_Optional },
    { "opspertrans", 0,   opt_opsptx, Clp_ValUnsigned, Clp_Optional },
    { "opsperro",    0,   opt_opspro, Clp_ValUnsigned, Clp_Optional },
    { "readonly",    0,   opt_ropct,  Clp_ValDouble,   Clp_Optional },
    { "writeratio",  0,   opt_wrpct,  Clp_ValDouble,   Clp_Optional },
    { "time",        'l', opt_time,   Clp_ValDouble,   Clp_Optional },
    { "perf",        'p', opt_perf,   Clp_NoVal,       Clp_Negate | Clp_Optional },
    { "dump",        'd', opt_dump,   Clp_NoVal,       Clp_Negate | Clp_Optional },
    { "measure",     'm', opt_insm,   Clp_NoVal,       Clp_Negate | Clp_Optional }
};

template <bool InsMeasure>
void instantiate_and_execute_testers() {
    using ubench::MtZipfTesterDefault;
    using ubench::MtZipfTesterMeasure;
    using ubench::params;
    using tpcc::db_params_id;

    template <typename P>
    using tester = std::conditional<InsMeasure, MtZipfTesterMeasure<P>, MtZipfTesterDefault<P>>::type;

    switch (params.dbid) {
        case db_params_id::Default: {
            tester<tpcc::db_default_params>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::Opaque: {
            tester<tpcc::db_opaque_params>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::TwoPL: {
            tester<tpcc::db_2pl_params>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::Adaptive: {
            tester<tpcc::db_adaptive_params>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::Swiss: {
            tester<tpcc::db_swiss_params>::type t(params.nthreads);
            t.execute();
            break;
        }
        case db_params_id::TicToc: {
            tester<tpcc::db_tictoc_params>::type t(params.nthreads);
            t.execute();
            break;
        }
        default:
            std::cerr << "invalid ccid" << std::endl;
            abort();
    }
}

int main(int argc, const char *argv[]) {
    using ubench::params;
    using tpcc::db_params_id;

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
    params.proc_frequency_hz = 0;
    params.dump_trace = false;
    params.profiler = false;
    params.ins_measure = false;

    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

    int opt;
    int ret = 0;
    bool clp_stop = false;
    bool ro_specified = false;
    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
        switch (opt) {
            case opt_ccid:
                params.dbid = tpcc::parse_dbid(clp->val.s);
                break;
            case opt_type:
                params.datatype = ubench::parse_datatype(clp->val.s);
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
    tpcc::constants::processor_tsc_frequency = freq;
    params.proc_frequency_hz = (uint64_t)(freq * tpcc::constants::billion);

    always_assert(params.datatype == ubench::DsType::masstree, "Only Masstree is currently supported");

    if (params.ins_measure)
        instantiate_and_execute_testers<true>();
    else
        instantiate_and_execute_testers<false>();
}
