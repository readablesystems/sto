#include "clp.h"

#include "DB_profiler.hh"
#include "PlatformFeatures.hh"
#include "Garbage_bench.hh"
#include "DB_params.hh"

double db_params::constants::processor_tsc_frequency;

enum { opt_dbid = 1, opt_nthrs, opt_time, opt_dbsz };

struct cmd_params {
    db_params::db_params_id dbid;
    size_t db_size;
    int num_threads;
    double time_limit;

    cmd_params() : dbid(db_params::db_params_id::Default), db_size(256), num_threads(1), time_limit(10.0) {}
};

static const Clp_Option options[] = {
    { "dbid",       'i', opt_dbid,  Clp_ValString,  Clp_Optional },
    { "nthreads",   't', opt_nthrs, Clp_ValInt,     Clp_Optional },
    { "time",       'l', opt_time,  Clp_ValDouble,  Clp_Optional },
    { "dbsize",     'z', opt_dbsz,  Clp_ValInt,     Clp_Optional },
};

template <typename DBParams>
void execute(cmd_params& p) {
    typedef garbage_bench::garbage_runner<DBParams> r_type_nopred;
    typedef garbage_bench::garbage_db<DBParams> db_type_nopred;

    db_type_nopred db_nopred;
    initialize_db(db_nopred, p.db_size);

    bench::db_profiler prof(false/*don't spawn perf*/);
    auto nthreads = p.num_threads;
    auto time_limit = p.time_limit;
    std::cout << "Number of threads: " << nthreads << std::endl;

    r_type_nopred r_nopred(nthreads, time_limit, db_nopred);

    size_t ncommits;

    auto advancer = std::thread(&Transaction::epoch_advancer, nullptr);

    prof.start(Profiler::perf_mode::record);
    ncommits = r_nopred.run();
    prof.finish(ncommits);

    std::cout << "Cleaning up\n";
    Transaction::rcu_release_all(advancer, nthreads);

    auto counters = Transaction::txp_counters_combined();
    auto ndreq = counters.p(txp_rcu_del_req);
    auto ndareq = counters.p(txp_rcu_delarr_req);
    auto nfreq = counters.p(txp_rcu_free_req);
    auto ndimpl = counters.p(txp_rcu_del_impl);
    auto ndaimpl = counters.p(txp_rcu_delarr_impl);
    auto nfimpl = counters.p(txp_rcu_free_impl);

    auto total_reqs = ndreq + ndareq + nfreq;
    auto total_impls = ndimpl + ndaimpl + nfimpl;

    printf("STO profile counters: %d\n", STO_PROFILE_COUNTERS);
#if STO_PROFILE_COUNTERS
    printf("RCU dealloc requests: %llu\n", total_reqs);
    printf("dealloc reqs/commit:  %.2lf\n", 1. * total_reqs / ncommits);
    printf("RCU dealloc calls:    %llu\n", total_impls);
    printf("dealloc calls/commit: %.2lf\n", 1. * total_impls / ncommits);
    printf("GC inserts:           %llu\n", counters.p(txp_gc_inserts));
    printf("GC deletes:           %llu\n", counters.p(txp_gc_deletes));
#else
    (void) total_reqs, (void) total_impls;
#endif
}

int main(int argc, const char * const *argv) {
    using garbage_bench::initialize_db;

    cmd_params p;

    Sto::global_init();
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
    int ret_code = 0;
    int opt;
    bool clp_stop = false;
    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
        switch (opt) {
        case opt_dbid:
            p.dbid = db_params::parse_dbid(clp->val.s);
            if (p.dbid == db_params::db_params_id::None) {
                std::cout << "Unsupported DB CC id: "
                    << ((clp->val.s == nullptr) ? "" : std::string(clp->val.s)) << std::endl;
                ret_code = 1;
                clp_stop = true;
            }
            break;
        case opt_dbsz:
            p.db_size = clp->val.i;
            break;
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

    switch (p.dbid) {
        case db_params::db_params_id::MVCC:
            execute<db_params::db_mvcc_params>(p);
            break;
        default:
            execute<db_params::db_default_params>(p);
            break;
    }

    return 0;
}
