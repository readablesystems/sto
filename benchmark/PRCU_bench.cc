#include <sstream>
#include <iostream>
#include <thread>

#include "PRCU_bench.hh"
#include "PRCU_txns.hh"
#include "PlatformFeatures.hh"
#include "DB_profiler.hh"

namespace prcubench {

using namespace db_params;
using bench::db_profiler;

enum {
    opt_dbid = 1, opt_nrdrs, opt_nwtrs, opt_mode, opt_time, opt_txns, opt_perf,
    opt_pfcnt, opt_gc, opt_node, opt_comm, opt_nont, opt_rtsz, opt_bare
};

static const Clp_Option options[] = {
    { "dbid",         'i', opt_dbid,  Clp_ValString, Clp_Optional },
    { "readers",      'r', opt_nrdrs, Clp_ValInt,    Clp_Optional },
    { "writers",      'w', opt_nwtrs, Clp_ValInt,    Clp_Optional },
    { "mode",         'm', opt_mode,  Clp_ValString, Clp_Optional },
    { "time",         'l', opt_time,  Clp_ValDouble, Clp_Optional },
    { "transactions", 't', opt_txns,  Clp_ValInt,    Clp_Optional },
    { "rtsize",       'z', opt_rtsz,  Clp_ValInt,    Clp_Optional },
    { "perf",         'p', opt_perf,  Clp_NoVal,     Clp_Optional },
    { "perf-counter", 'c', opt_pfcnt, Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "gc",           'g', opt_gc,    Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "node",         'n', opt_node,  Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "commute",      'x', opt_comm,  Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "nontrans",     'N', opt_nont,  Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "bare",         'B', opt_bare,  Clp_NoVal,     Clp_Negate| Clp_Optional },
};

static inline void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss << "Usage of " << std::string(argv_0) << ":" << std::endl
       << "  --dbid=<STRING> (or -i<STRING>)" << std::endl
       << "    Specify the type of DB concurrency control used. Can be one of the followings:" << std::endl
       << "      default, opaque, 2pl, adaptive, swiss, tictoc, defaultnode, mvcc, mvccnode" << std::endl
       << "  --readers=<NUM> (or -r<NUM>)" << std::endl
       << "    Specify the number of reader threads (default 1)." << std::endl
       << "  --writers=<NUM> (or -w<NUM>)" << std::endl
       << "    Specify the number of writer threads (default 0)." << std::endl
       << "  --mode=<CHAR> (or -m<CHAR>)" << std::endl
       << "    Specify whether to use uniform or skewed distribution (U/S, default U)." << std::endl
       << "  --time=<NUM> (or -l<NUM>)" << std::endl
       << "    Specify the time (duration) for which the benchmark is run (default 10 seconds)." << std::endl
       << "  --transactions=<NUM> (or -t<NUM>)" << std::endl
       << "    Specify the number of transactions to run per thread (default 20M)." << std::endl
       << "  --rtsize=<NUM> (or -z<NUM>)" << std::endl
       << "    Specify the number of reads per read transaction (default 10)." << std::endl
       << "  --perf (or -p)" << std::endl
       << "    Spawns perf profiler in record mode for the duration of the benchmark run." << std::endl
       << "  --perf-counter (or -c)" << std::endl
       << "    Spawns perf profiler in counter mode for the duration of the benchmark run." << std::endl
       << "  --gc (or -g)" << std::endl
       << "    Enable garbage collection (default false)." << std::endl
       << "  --node (or -n)" << std::endl
       << "    Enable node tracking (default false)." << std::endl
       << "  --commute (or -x)" << std::endl
       << "    Enable commutative updates in MVCC (default false)." << std::endl
       << "  --bare (or -B)" << std::endl
       << "    Run bare framework experiments (default false)." << std::endl;
    std::cout << ss.str() << std::flush;
}


template <typename DBParams>
void ht_prepopulation_thread(int thread_id, ht_table<DBParams>& table, uint64_t key_begin, uint64_t key_end) {
    set_affinity(thread_id);
    ht_input_generator ig(thread_id);
    table.table_thread_init();
    for (uint64_t i = key_begin; i < key_end; ++i) {
        table.table().nontrans_put(ht_key(i), ig.random_ht_value<ht_value>());
    }
}

template <typename DBParams>
void ht_table<DBParams>::prepopulate() {
    static constexpr uint64_t nthreads = 32;
    uint64_t key_begin, key_end;
    uint64_t segment_size = ht_table_size / nthreads;
    key_begin = 0;
    key_end = segment_size;

    std::vector<std::thread> prepopulators;

    for (uint64_t tid = 0; tid < nthreads; ++tid) {
        prepopulators.emplace_back(ht_prepopulation_thread<DBParams>, (int)tid, std::ref(*this), key_begin, key_end);
        key_begin += segment_size;
        key_end += segment_size;
    }

    for (auto& t : prepopulators)
        t.join();
}

template <typename DBParams>
class ht_access {
public:
    struct txn_result {
        bool ro;
        double tp;
        double secs;
        double txn_secs;
        double read_secs;
        double write_secs;
        double internal_secs;
        uint64_t sum;
    };

    static void prcubench_runner_thread(ht_table<DBParams>& table, db_profiler& prof, prcubench_runner<DBParams>& runner, double duration, txn_result& results) {
        (void) prof;
        table.table_thread_init();

        ::TThread::set_id(runner.id());
        set_affinity(runner.id());

        uint64_t txn_t = 0;
        uint64_t read_t = 0;
        uint64_t write_t = 0;
        auto start_t = read_tsc();
        auto target_end = start_t + duration;

        auto it = runner.workload.begin();
        uint64_t count = 0;

        double end_t;
        for (end_t = 0; end_t < target_end; end_t = read_tsc()) {
            runner.run_txn(*it);
            count += 1;  //it->ops.size();
            ++it;
            if (it == runner.workload.end())
                it = runner.workload.begin();
        }

        auto elapsed_t = end_t - start_t;

        TSC_ACCOUNT(tc_elapsed, elapsed_t);

        auto secs = prof.ticks_to_secs(elapsed_t);

        results.ro = !runner.workload[0].rw_txn;
        results.tp = count / secs;
        results.secs = secs;
        results.txn_secs = prof.ticks_to_secs(txn_t);
        results.read_secs = prof.ticks_to_secs(read_t);
        results.write_secs = prof.ticks_to_secs(write_t);
        results.internal_secs = prof.ticks_to_secs(txn_t - read_t - write_t);
        results.sum = runner.total_sum;
    }

    static void prcubench_barerunner_thread(ht_table<DBParams>& table, db_profiler& prof, prcubench_barerunner<DBParams>& runner, uint64_t txn_count, txn_result& results) {
        (void) prof;
        table.table_thread_init();

        ::TThread::set_id(runner.id());
        set_affinity(runner.id());

        uint64_t txn_t = 0;
        uint64_t read_t = 0;
        uint64_t write_t = 0;

        auto it = runner.workload.begin();
        uint64_t count = 0;

        auto start_t = read_tsc();

        for (uint64_t i = 0; i < txn_count; i++) {
            runner.run_txn(*it);
            count += 1;  //it->ops.size();
            ++it;
            if (it == runner.workload.end())
                it = runner.workload.begin();
        }

        double end_t = read_tsc();
        auto elapsed_t = end_t - start_t;

        TSC_ACCOUNT(tc_elapsed, elapsed_t);

        auto secs = prof.ticks_to_secs(elapsed_t);

        results.ro = !runner.workload[0].rw_txn;
        results.tp = count / secs;
        results.secs = secs;
        results.txn_secs = prof.ticks_to_secs(txn_t);
        results.read_secs = prof.ticks_to_secs(read_t);
        results.write_secs = prof.ticks_to_secs(write_t);
        results.internal_secs = prof.ticks_to_secs(txn_t - read_t - write_t);
        results.sum = runner.total_sum;
    }

    static void workload_generation(std::vector<prcubench_runner<DBParams>>& runners, mode_id mode, uint64_t read_txn_size) {
        std::vector<std::thread> thrs;
        (void) mode;
        for (auto& r : runners) {
            thrs.emplace_back(&prcubench_runner<DBParams>::gen_workload, &r, read_txn_size);
        }
        for (auto& t : thrs)
            t.join();
    }

    static void bare_workload_generation(std::vector<prcubench_barerunner<DBParams>>& runners, mode_id mode, uint64_t read_txn_size) {
        std::vector<std::thread> thrs;
        (void) mode;
        for (auto& r : runners) {
            thrs.emplace_back(&prcubench_barerunner<DBParams>::gen_workload, &r, read_txn_size);
        }
        for (auto& t : thrs)
            t.join();
    }

    static std::pair<double, double> run_benchmark(ht_table<DBParams>& db, db_profiler& prof, std::vector<prcubench_runner<DBParams>>& runners, double duration_secs) {
        int num_runners = runners.size();
        std::vector<std::thread> runner_thrs;
        std::vector<txn_result> results;
        results.resize(size_t(num_runners));

        uint64_t initial_sum = 0;
        for (uint64_t key = 0; key < ht_table_size; key++) {
            auto* r = db.table().nontrans_get(key);
            if (r) {
                initial_sum += r->value;
            }
        }

        auto duration = prof.secs_to_ticks(duration_secs);

        for (int i = 0; i < num_runners; ++i) {
            //fprintf(stdout, "runner %d created\n", i);
            runner_thrs.emplace_back(prcubench_runner_thread, std::ref(db), std::ref(prof),
                                     std::ref(runners[i]), duration, std::ref(results[i]));
        }

        for (auto &t : runner_thrs)
            t.join();

        uint64_t th_ro = 0;
        uint64_t th_rw = 0;
        //double total_rosecs = 0;
        //double total_rwsecs = 0;
        double ro_tp = 0;
        double rw_tp = 0;
        /*
        double total_secs = 0;
        double total_tsecs = 0;
        double total_rsecs = 0;
        double total_wsecs = 0;
        double total_isecs = 0;
        */
        uint64_t expected_sum = 0;
        for (auto& result : results) {
            if (result.ro) {
                //total_rosecs += result.secs;
                ro_tp += result.tp;
                th_ro++;
            } else {
                //total_rwsecs += result.secs;
                rw_tp += result.tp;
                th_rw++;
            }
            expected_sum += result.sum;
            //total_secs += result.secs;
            /*
            total_tsecs += result.txn_secs;
            total_rsecs += result.read_secs;
            total_wsecs += result.write_secs;
            total_isecs += result.internal_secs;
            */
        }
        /*
        std::cout
            << "txn time: " << total_tsecs << "s, "
            << "read time: " << total_rsecs << "s, "
            << "write time: " << total_wsecs << "s, "
            << "internals time: " << total_isecs << "s"
            << std::endl;
        */

        uint64_t db_sum = 0;
        for (uint64_t key = 0; key < ht_table_size; key++) {
            auto* r = db.table().nontrans_get(key);
            if (r) {
                db_sum += r->value;
            }
        }

        auto counters = Transaction::txp_counters_combined();
        std::cout << "Initial  sum: " << initial_sum << std::endl;
        std::cout << "Expected sum: " << expected_sum << std::endl;
        std::cout << "Database sum: " << db_sum - initial_sum << std::endl;
        std::cout << "Bad versions: " << counters.p(txp_mvcc_bad_versions) << std::endl;
        std::cout << "Counters sum: " << counters.p(txp_mvcc_sum) << std::endl;
        std::cout << "Total    sum: " << counters.p(txp_total_sum) << std::endl;

        ro_tp = th_ro ? ro_tp / th_ro : 0;
        rw_tp = th_rw ? rw_tp / th_rw : 0;
        return std::make_pair(ro_tp, rw_tp);
    }

    static std::pair<double, double> run_bare_benchmark(ht_table<DBParams>& db, db_profiler& prof, std::vector<prcubench_barerunner<DBParams>>& runners, uint64_t txn_count) {
        int num_runners = runners.size();
        std::vector<std::thread> runner_thrs;
        std::vector<txn_result> results;
        results.resize(size_t(num_runners));

        for (int i = 0; i < num_runners; ++i) {
            //fprintf(stdout, "runner %d created\n", i);
            runner_thrs.emplace_back(prcubench_barerunner_thread, std::ref(db), std::ref(prof),
                                     std::ref(runners[i]), txn_count, std::ref(results[i]));
        }

        for (auto &t : runner_thrs)
            t.join();
        //prcubench_runner_thread(db, prof, runners[0], txn_count, results[0]);

        uint64_t th_ro = 0;
        uint64_t th_rw = 0;
        //double total_rosecs = 0;
        //double total_rwsecs = 0;
        double ro_tp = 0;
        double rw_tp = 0;
        /*
        double total_secs = 0;
        double total_tsecs = 0;
        double total_rsecs = 0;
        double total_wsecs = 0;
        double total_isecs = 0;
        */
        for (auto& result : results) {
            if (result.ro) {
                //total_rosecs += result.secs;
                ro_tp += result.tp;
                th_ro++;
            } else {
                //total_rwsecs += result.secs;
                rw_tp += result.tp;
                th_rw++;
            }
            //total_secs += result.secs;
            /*
            total_tsecs += result.txn_secs;
            total_rsecs += result.read_secs;
            total_wsecs += result.write_secs;
            total_isecs += result.internal_secs;
            */
        }
        /*
        std::cout
            << "txn time: " << total_tsecs << "s, "
            << "read time: " << total_rsecs << "s, "
            << "write time: " << total_wsecs << "s, "
            << "internals time: " << total_isecs << "s"
            << std::endl;
        */
        ro_tp = th_ro ? ro_tp / th_ro : 0;
        rw_tp = th_rw ? rw_tp / th_rw : 0;
        return std::make_pair(ro_tp, rw_tp);
    }

    static int execute(int argc, const char *const *argv) {
        int ret = 0;

        bool spawn_perf = false;
        bool counter_mode = false;
        int readers = 1;
        int writers = 0;
        mode_id mode = mode_id::Uniform;
        uint64_t txn_count = 2000000;
        uint64_t read_txn_size = 10;
        double duration_secs = 10;
        bool enable_gc = false;
        bool nontrans = false;
        bool bare = false;

        (void)txn_count;

        Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

        int opt;
        bool clp_stop = false;
        while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
            switch (opt) {
            case opt_dbid:
                break;
            case opt_nrdrs:
                readers = clp->val.i;
                break;
            case opt_nwtrs:
                writers = clp->val.i;
                break;
            case opt_mode: {
                switch (*clp->val.s) {
                case 'U':
                    mode = mode_id::Uniform;
                    break;
                case 'S':
                    mode = mode_id::Skewed;
                    break;
                default:
                    print_usage(argv[0]);
                    ret = 1;
                    clp_stop = true;
                    break;
                }
                break;
            }
            case opt_time:
                duration_secs = clp->val.d;
                break;
            case opt_txns:
                txn_count = clp->val.i;
                break;
            case opt_rtsz:
                read_txn_size = clp->val.i;
                break;
            case opt_perf:
                spawn_perf = !clp->negated;
                break;
            case opt_pfcnt:
                counter_mode = !clp->negated;
                break;
            case opt_gc:
                enable_gc = !clp->negated;
                break;
            case opt_node:
                break;
            case opt_comm:
                break;
            case opt_nont:
                nontrans = !clp->negated;
                break;
            case opt_bare:
                bare = !clp->negated;
                break;
            default:
                print_usage(argv[0]);
                ret = 1;
                clp_stop = true;
                break;
            }
        }
        int num_threads = readers + writers;

        Clp_DeleteParser(clp);
        if (ret != 0)
            return ret;

        auto profiler_mode = counter_mode ?
                             Profiler::perf_mode::counters : Profiler::perf_mode::record;

        if (counter_mode && !spawn_perf) {
            // turns on profiling automatically if perf counters are requested
            spawn_perf = true;
        }

        if (spawn_perf) {
            std::cout << "Info: Spawning perf profiler in "
                      << (counter_mode ? "counter" : "record") << " mode" << std::endl;
        }

        db_profiler prof(spawn_perf);
        ht_table<DBParams> table;

        std::cout << "Prepopulating database..." << std::endl;
        table.prepopulate();
        std::cout << "Prepopulation complete." << std::endl;

        if (bare) {
            std::vector<prcubench_barerunner<DBParams>> runners;
            for (int i = 0; i < num_threads; ++i) {
                uint32_t write_threshold = (i < readers) ? 0 :
                    std::numeric_limits<uint32_t>::max();
                runners.emplace_back(i, table, mode, write_threshold, nontrans);
            }

            std::thread advancer;
            std::cout << "Generating workload..." << std::endl;
            bare_workload_generation(runners, mode, read_txn_size);
            std::cout << "Done." << std::endl;
            if (enable_gc) {
                Transaction::set_epoch_cycle(1000);
                advancer = std::thread(&Transaction::epoch_advancer, nullptr);
                advancer.detach();
            }

            prof.start(profiler_mode);
            auto [ro_tp, rw_tp] = run_bare_benchmark(table, prof, runners, txn_count);

            // Print benchmark stats
            std::cout
                << "R/O-throughput: " << ro_tp << " txns/sec/thread; "
                << "R/W-throughput: " << rw_tp << " txns/sec/thread" << std::endl;
        } else {
            std::vector<prcubench_runner<DBParams>> runners;
            for (int i = 0; i < num_threads; ++i) {
                uint32_t write_threshold = (i < readers) ? 0 :
                    std::numeric_limits<uint32_t>::max();
                runners.emplace_back(i, table, mode, write_threshold, nontrans);
            }

            std::thread advancer;
            std::cout << "Generating workload..." << std::endl;
            workload_generation(runners, mode, read_txn_size);
            std::cout << "Done." << std::endl;
            if (enable_gc) {
                Transaction::set_epoch_cycle(1000);
                advancer = std::thread(&Transaction::epoch_advancer, nullptr);
                advancer.detach();
            }

            prof.start(profiler_mode);
            auto [ro_tp, rw_tp] = run_benchmark(table, prof, runners, duration_secs);

            // Print benchmark stats
            std::cout
                << "R/O-throughput: " << ro_tp << " txns/sec/thread; "
                << "R/W-throughput: " << rw_tp << " txns/sec/thread" << std::endl;
        }

        // Print STO stats
        Transaction::print_stats();

        return 0;
    }

};

}; // namespace ycsb

using namespace prcubench;
using namespace db_params;

double constants::processor_tsc_frequency;

int main(int argc, const char *const *argv) {
    db_params_id dbid = db_params_id::Default;
    int ret_code = 0;

    Sto::global_init();
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

    int opt;
    bool clp_stop = false;
    bool node_tracking = false;
    bool enable_commute = false;
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
        default:
            break;
        }
    }

    Clp_DeleteParser(clp);
    if (ret_code != 0)
        return ret_code;

    auto cpu_freq = determine_cpu_freq();
    if (cpu_freq == 0.0)
        return 1;
    else
        constants::processor_tsc_frequency = cpu_freq;

    switch (dbid) {
    case db_params_id::Default:
        if (node_tracking && enable_commute) {
            ret_code = ht_access<db_default_commute_node_params>::execute(argc, argv);
        } else if (node_tracking) {
            ret_code = ht_access<db_default_node_params>::execute(argc, argv);
        } else if (enable_commute) {
            ret_code = ht_access<db_default_commute_params>::execute(argc, argv);
        } else {
            ret_code = ht_access<db_default_params>::execute(argc, argv);
        }
        break;
    case db_params_id::Opaque:
        if (enable_commute) {
            ret_code = ht_access<db_opaque_commute_params>::execute(argc, argv);
        } else {
            ret_code = ht_access<db_opaque_params>::execute(argc, argv);
        }
        break;
    /*
    case db_params_id::TwoPL:
        ret_code = ycsb_access<db_2pl_params>::execute(argc, argv);
        break;
    case db_params_id::Adaptive:
        ret_code = ycsb_access<db_adaptive_params>::execute(argc, argv);
        break;
    case db_params_id::Swiss:
        ret_code = ycsb_access<db_swiss_params>::execute(argc, argv);
        break;
    */
    case db_params_id::TicToc:
        if (node_tracking) {
            std::cerr << "Warning: node tracking and commute options ignored." << std::endl;
        }
        if (enable_commute) {
            ret_code = ht_access<db_tictoc_commute_params>::execute(argc, argv);
        } else {
            ret_code = ht_access<db_tictoc_params>::execute(argc, argv);
        }
        break;
    case db_params_id::MVCC:
        if (node_tracking && enable_commute) {
            ret_code = ht_access<db_mvcc_commute_node_params>::execute(argc, argv);
        } else if (node_tracking) {
            ret_code = ht_access<db_mvcc_node_params>::execute(argc, argv);
        } else if (enable_commute) {
            ret_code = ht_access<db_mvcc_commute_params>::execute(argc, argv);
        } else {
            ret_code = ht_access<db_mvcc_params>::execute(argc, argv);
        }
        break;
    default:
        std::cerr << "unknown db config parameter id" << std::endl;
        ret_code = 1;
        break;
    };

    return ret_code;

}
