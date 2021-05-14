#include <sstream>
#include <iostream>
#include <thread>

#include "YCSB_bench.hh"
#include "YCSB_txns.hh"
#include "PlatformFeatures.hh"
#include "DB_profiler.hh"

namespace ycsb {

using namespace db_params;
using bench::db_profiler;

enum {
    opt_dbid = 1, opt_nthrs, opt_mode, opt_time, opt_perf, opt_pfcnt, opt_gc,
    opt_node, opt_comm
};

static const Clp_Option options[] = {
    { "dbid",         'i', opt_dbid,  Clp_ValString, Clp_Optional },
    { "nthreads",     't', opt_nthrs, Clp_ValInt,    Clp_Optional },
    { "mode",         'm', opt_mode,  Clp_ValString, Clp_Optional },
    { "time",         'l', opt_time,  Clp_ValDouble, Clp_Optional },
    { "perf",         'p', opt_perf,  Clp_NoVal,     Clp_Optional },
    { "perf-counter", 'c', opt_pfcnt, Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "gc",           'g', opt_gc,    Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "node",         'n', opt_node,  Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "commute",      'x', opt_comm,  Clp_NoVal,     Clp_Negate| Clp_Optional },
};

static inline void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss << "Usage of " << std::string(argv_0) << ":" << std::endl
       << "  --dbid=<STRING> (or -i<STRING>)" << std::endl
       << "    Specify the type of DB concurrency control used. Can be one of the followings:" << std::endl
       << "      default, opaque, 2pl, adaptive, swiss, tictoc, defaultnode, mvcc, mvccnode" << std::endl
       << "  --nthreads=<NUM> (or -t<NUM>)" << std::endl
       << "    Specify the number of threads (or TPCC workers/terminals, default 1)." << std::endl
       << "  --mode=<CHAR> (or -m<CHAR>)" << std::endl
       << "    Specify which YCSB variant to run (A/B/C, default C)." << std::endl
       << "  --time=<NUM> (or -l<NUM>)" << std::endl
       << "    Specify the time (duration) for which the benchmark is run (default 10 seconds)." << std::endl
       << "  --perf (or -p)" << std::endl
       << "    Spawns perf profiler in record mode for the duration of the benchmark run." << std::endl
       << "  --perf-counter (or -c)" << std::endl
       << "    Spawns perf profiler in counter mode for the duration of the benchmark run." << std::endl
       << "  --gc (or -g)" << std::endl
       << "    Enable garbage collection (default false)." << std::endl
       << "  --node (or -n)" << std::endl
       << "    Enable node tracking (default false)." << std::endl
       << "  --commute (or -x)" << std::endl
       << "    Enable commutative updates in MVCC (default false)." << std::endl;
    std::cout << ss.str() << std::flush;
}


template <typename DBParams>
void ycsb_prepopulation_thread(int thread_id, ycsb_db<DBParams>& db, uint64_t key_begin, uint64_t key_end) {
    set_affinity(thread_id);
    ycsb_input_generator ig(thread_id);
    db.table_thread_init();
    for (uint64_t i = key_begin; i < key_end; ++i) {
#if TPCC_SPLIT_TABLE
        db.ycsb_half_tables(true).nontrans_put(ycsb_key(i), ig.random_ycsb_value<ycsb_half_value>());
        db.ycsb_half_tables(false).nontrans_put(ycsb_key(i), ig.random_ycsb_value<ycsb_half_value>());
#else
        db.ycsb_table().nontrans_put(ycsb_key(i), ig.random_ycsb_value<ycsb_value>());
#endif
    }
}

template <typename DBParams>
void ycsb_db<DBParams>::prepopulate() {
    static constexpr uint64_t nthreads = 32;
    uint64_t key_begin, key_end;
    uint64_t segment_size = ycsb_table_size / nthreads;
    key_begin = 0;
    key_end = segment_size;

    std::vector<std::thread> prepopulators;

    for (uint64_t tid = 0; tid < nthreads; ++tid) {
        prepopulators.emplace_back(ycsb_prepopulation_thread<DBParams>, (int)tid, std::ref(*this), key_begin, key_end);
        key_begin += segment_size;
        key_end += segment_size;
    }

    for (auto& t : prepopulators)
        t.join();
}

template <typename DBParams>
class ycsb_access {
public:
    struct results {
        results() : count(0), collapse1_count(0), collapse2_count(0) {}

        uint64_t count;
        uint64_t collapse1_count;
        uint64_t collapse2_count;
    };

    static void ycsb_runner_thread(ycsb_db<DBParams>& db, db_profiler& prof, ycsb_runner<DBParams>& runner, double time_limit, results& txn_result) {
        uint64_t local_cnt = 0;
        uint64_t collapse_cnt[2] = {0, 0};
        db.table_thread_init();

        ::TThread::set_id(runner.id());
        set_affinity(runner.id());

        uint64_t tsc_diff = (uint64_t)(time_limit * constants::processor_tsc_frequency * constants::billion);
        auto start_t = prof.start_timestamp();

        auto it = runner.workload.begin();

        while (true) {
            auto curr_t = read_tsc();
            if ((curr_t - start_t) >= tsc_diff)
                break;

            runner.run_txn(*it);
            if (it->collapse_type) {
                ++collapse_cnt[it->collapse_type - 1];
            }
            ++it;
            if (it == runner.workload.end())
                it = runner.workload.begin();

            ++local_cnt;
        }

        txn_result.count = local_cnt;
        txn_result.collapse1_count = collapse_cnt[0];
        txn_result.collapse2_count = collapse_cnt[1];
    }

    static void workload_generation(std::vector<ycsb_runner<DBParams>>& runners, mode_id mode) {
        std::vector<std::thread> thrs;
        int tsize = 16;
        if (mode == mode_id::ReadOnly) {
            tsize = 2;
        } else if (mode == mode_id::WriteCollapse) {
            tsize = -1;
        } else if (mode == mode_id::RWCollapse) {
            tsize = -2;
        } else if (mode == mode_id::ReadCollapse) {
            tsize = -3;
        }
        for (uint64_t i = 0; i < runners.size(); ++i) {
            thrs.emplace_back(
                    &ycsb_runner<DBParams>::gen_workload, std::ref(runners[i]), i, tsize);
        }
        for (auto& t : thrs)
            t.join();
    }

    static results run_benchmark(ycsb_db<DBParams>& db, db_profiler& prof, std::vector<ycsb_runner<DBParams>>& runners, double time_limit) {
        int num_runners = runners.size();
        std::vector<std::thread> runner_thrs;
        std::vector<results> txn_cnts;
        txn_cnts.resize(num_runners);

        for (int i = 0; i < num_runners; ++i) {
            txn_cnts.emplace_back();
            runner_thrs.emplace_back(ycsb_runner_thread, std::ref(db), std::ref(prof),
                                     std::ref(runners[i]), time_limit, std::ref(txn_cnts[i]));
        }

        for (auto &t : runner_thrs)
            t.join();

        results total_txn_cnt;
        for (auto& cnt : txn_cnts) {
            total_txn_cnt.count += cnt.count;
            total_txn_cnt.collapse1_count += cnt.collapse1_count;
            total_txn_cnt.collapse2_count += cnt.collapse2_count;
        }
        return total_txn_cnt;
    }


    static int execute(int argc, const char *const *argv) {
        int ret = 0;

        bool spawn_perf = false;
        bool counter_mode = false;
        int num_threads = 1;
        mode_id mode = mode_id::ReadOnly;
        double time_limit = 10.0;
        bool enable_gc = false;

        Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

        int opt;
        bool clp_stop = false;
        while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
            switch (opt) {
            case opt_dbid:
                break;
            case opt_nthrs:
                num_threads = clp->val.i;
                break;
            case opt_mode: {
                switch (*clp->val.s) {
                case 'A':
                    mode = mode_id::HighContention;
                    break;
                case 'B':
                    mode = mode_id::MediumContention;
                    break;
                case 'C':
                    mode = mode_id::ReadOnly;
                    break;
                case 'X':
                    mode = mode_id::WriteCollapse;
                    break;
                case 'Y':
                    mode = mode_id::RWCollapse;
                    break;
                case 'Z':
                    mode = mode_id::ReadCollapse;
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
                time_limit = clp->val.d;
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
            default:
                print_usage(argv[0]);
                ret = 1;
                clp_stop = true;
                break;
            }
        }

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
        ycsb_db<DBParams> db;

        std::cout << "Prepopulating database..." << std::endl;
        db.prepopulate();
        std::cout << "Prepopulation complete." << std::endl;

        std::vector<ycsb_runner<DBParams>> runners;
        for (int i = 0; i < num_threads; ++i) {
            runners.emplace_back(i, db, mode);
        }

        std::thread advancer;
        std::cout << "Generating workload..." << std::endl;
        workload_generation(runners, mode);
        std::cout << "Done." << std::endl;
        std::cout << "Garbage collection: ";
        if (enable_gc) {
            std::cout << "enabled, running every 1 ms";
            Transaction::set_epoch_cycle(1000);
            advancer = std::thread(&Transaction::epoch_advancer, nullptr);
        } else {
            std::cout << "disabled";
        }
        std::cout << std::endl << std::flush;

        prof.start(profiler_mode);
        auto result = run_benchmark(db, prof, runners, time_limit);
        auto elapsed_ms = prof.finish(result.count);
        if (result.collapse1_count || result.collapse2_count) {
            std::cout << "Collapse 1 throughput: " << (double)result.collapse1_count / (elapsed_ms / 1000) << " txns/sec" << std::endl;
            std::cout << "Collapse 2 throughput: " << (double)result.collapse2_count / (elapsed_ms / 1000) << " txns/sec" << std::endl;
        }

        Transaction::rcu_release_all(advancer, num_threads);

        return 0;
    }

};

}; // namespace ycsb

using namespace ycsb;
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
            ret_code = ycsb_access<db_default_commute_node_params>::execute(argc, argv);
        } else if (node_tracking) {
            ret_code = ycsb_access<db_default_node_params>::execute(argc, argv);
        } else if (enable_commute) {
            ret_code = ycsb_access<db_default_commute_params>::execute(argc, argv);
        } else {
            ret_code = ycsb_access<db_default_params>::execute(argc, argv);
        }
        break;
    /*
    case db_params_id::Opaque:
        ret_code = ycsb_access<db_opaque_params>::execute(argc, argv);
        break;
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
            ret_code = ycsb_access<db_tictoc_commute_params>::execute(argc, argv);
        } else {
            ret_code = ycsb_access<db_tictoc_params>::execute(argc, argv);
        }
        break;
    case db_params_id::MVCC:
        if (node_tracking && enable_commute) {
            ret_code = ycsb_access<db_mvcc_commute_node_params>::execute(argc, argv);
        } else if (node_tracking) {
            ret_code = ycsb_access<db_mvcc_node_params>::execute(argc, argv);
        } else if (enable_commute) {
            ret_code = ycsb_access<db_mvcc_commute_params>::execute(argc, argv);
        } else {
            ret_code = ycsb_access<db_mvcc_params>::execute(argc, argv);
        }
        break;
    default:
        std::cerr << "unknown db config parameter id" << std::endl;
        ret_code = 1;
        break;
    };

    return ret_code;

}
