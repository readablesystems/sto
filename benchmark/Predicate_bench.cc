#include "DB_profiler.hh"
#include "PlatformFeatures.hh"
#include "Predicate_bench.hh"

double db_params::constants::processor_tsc_frequency;

int main() {
    typedef db_params::db_default_params params;
    typedef predicate_bench::predicate_runner<params> runner_type;
    typedef predicate_bench::predicate_db<params> db_type;

    using predicate_bench::initialize_db;

    auto freq = determine_cpu_freq();
    if (freq ==  0.0)
        return -1;
    db_params::constants::processor_tsc_frequency = freq;

    db_type db;
    initialize_db(db, 256);

    bench::db_profiler prof(false /*don't spawn perf*/);
    runner_type r(1/*nthreads*/, 10.0/*time limit*/, db);

    prof.start(Profiler::perf_mode::counters);
    size_t ncommits = r.run();
    prof.finish(ncommits);

    return 0;
}
