#include "DB_profiler.hh"
#include "PlatformFeatures.hh"
#include "Predicate_bench.hh"

double db_params::constants::processor_tsc_frequency;

int main() {
    typedef db_params::db_default_params params;
    typedef TCounter<int, TNonopaqueWrapped<int>> w_pred;
    typedef TBox<int, TNonopaqueWrapped<int>> no_pred;
    typedef predicate_bench::predicate_runner<params, w_pred> r_type_wpred;
    typedef predicate_bench::predicate_runner<params, no_pred> r_type_nopred;
    typedef predicate_bench::predicate_db<params, w_pred> db_type_wpred;
    typedef predicate_bench::predicate_db<params, no_pred> db_type_nopred;

    using predicate_bench::initialize_db;

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
    const double time_limit = 10;
    for (size_t nthreads = 1; nthreads <= 16; nthreads *= 2) {
      std::cout << "Number of threads: " << nthreads << std::endl;

      r_type_wpred r_wpred(nthreads, time_limit, db_wpred);
      r_type_nopred r_nopred(nthreads, time_limit, db_nopred);

      size_t ncommits;

      prof.start(Profiler::perf_mode::record);
      ncommits = r_wpred.run(true /* readonly workload */);
      prof.finish(ncommits);

      prof.start(Profiler::perf_mode::record);
      ncommits = r_nopred.run(true /* readonly workload */);
      prof.finish(ncommits);
    }

    return 0;
}
