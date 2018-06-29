#include "clp.h"

#include "DB_profiler.hh"
#include "PlatformFeatures.hh"
#include "TART_bench.hh"

double db_params::constants::processor_tsc_frequency;

enum { opt_nthrs = 1, opt_time };

struct cmd_params {
    int num_threads;
    double time_limit;

    cmd_params() : num_threads(1), time_limit(10.0) {}
};

static const Clp_Option options[] = {
    { "nthreads", 't', opt_nthrs, Clp_ValInt, Clp_Optional },
    { "time", 'l', opt_time, Clp_ValDouble, Clp_Optional },
};

int main(int argc, const char * const *argv) {
    typedef db_params::db_default_params params;
    typedef tart_bench::tart_runner<params> r_type;
    typedef tart_bench::tart_db<params> db_type;

    using tart_bench::initialize_db;

    cmd_params p;

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

    db_type db;
    const size_t db_size = 256;
    initialize_db(db, db_size);

    bench::db_profiler prof(false/*don't spawn perf*/);
    auto nthreads = p.num_threads;
    auto time_limit = p.time_limit;
    std::cout << "Number of threads: " << nthreads << std::endl;

    r_type runner(nthreads, time_limit, db);

    size_t ncommits;

    prof.start(Profiler::perf_mode::record);
    ncommits = runner.run();
    prof.finish(ncommits);

    return 0;
}
