#include "clp.h"

#include "DB_profiler.hh"
#include "PlatformFeatures.hh"
#include "Predicate_bench.hh"

double db_params::constants::processor_tsc_frequency;

enum { opt_nthrs = 1, opt_time, opt_rw };

struct cmd_params {
    int num_threads;
    double time_limit;
    bool read_write;

    cmd_params() : num_threads(1), time_limit(10.0), read_write(false) {}
};

static const Clp_Option options[] = {
    { "nthreads", 't', opt_nthrs, Clp_ValInt, Clp_Optional },
    { "time", 'l', opt_time, Clp_ValDouble, Clp_Optional },
    { "readwrite", 'w', opt_rw, Clp_NoVal, Clp_Negate | Clp_Optional }
};

int main(int argc, const char * const *argv) {
    typedef db_params::db_default_params params;
    typedef TCounter<int, TNonopaqueWrapped<int>> w_pred;
    typedef TBox<int, TNonopaqueWrapped<int>> no_pred;
    typedef predicate_bench::predicate_runner<params, w_pred> r_type_wpred;
    typedef predicate_bench::predicate_runner<params, no_pred> r_type_nopred;
    typedef predicate_bench::predicate_db<params, w_pred> db_type_wpred;
    typedef predicate_bench::predicate_db<params, no_pred> db_type_nopred;

    using predicate_bench::initialize_db;

    cmd_params p;

    Sto::global_init();
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
    int ret_code = 0;
    int opt;
    bool clp_stop = false;
    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
        switch (opt) {
        case opt_nthrs:
            p.num_threads = clp->val.i;
            break;
        case opt_time:
            p.time_limit = clp->val.d;
            break;
        case opt_rw:
            p.read_write = !clp->negated;
            break;
        default:
            ret_code = 1;
            clp_stop = true;
            break;
        }
    }
    Clp_DeleteParser(clp);
    if (ret_code != 0)
        return ret_code;

    auto freq = determine_cpu_freq();
    if (freq ==  0.0)
        return -1;
    db_params::constants::processor_tsc_frequency = freq;

    db_type_wpred db_wpred;
    db_type_nopred db_nopred;
    const size_t db_size = 256;
    initialize_db(db_wpred, db_size);
    initialize_db(db_nopred, db_size);

    bench::db_profiler prof(false/*don't spawn perf*/);
    auto nthreads = p.num_threads;
    auto time_limit = p.time_limit;
    std::cout << "Number of threads: " << nthreads << std::endl;

    r_type_wpred r_wpred(nthreads, time_limit, db_wpred);
    r_type_nopred r_nopred(nthreads, time_limit, db_nopred);

    size_t ncommits;

    prof.start(Profiler::perf_mode::record);
    ncommits = r_wpred.run(!p.read_write);
    prof.finish(ncommits);

    prof.start(Profiler::perf_mode::record);
    ncommits = r_nopred.run(!p.read_write);
    prof.finish(ncommits);

    return 0;
}
