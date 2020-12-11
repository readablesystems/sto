#include <thread>

#include "Wikipedia_bench.hh"
#include "Wikipedia_loader.hh"
#include "Wikipedia_txns.hh"

#include "DB_profiler.hh"
#include "clp.h"

using db_params::constants;
using db_params::db_params_id;
using db_params::db_default_params;
using db_params::db_default_commute_params;
// TicToc requires node tracking for phantom protection
using db_params::db_tictoc_node_params;
using db_params::db_tictoc_commute_node_params;
using db_params::db_mvcc_params;
using db_params::db_mvcc_commute_params;
using db_params::parse_dbid;

template <typename DBParams>
int* wikipedia::wikipedia_loader<DBParams>::user_revision_cnts = nullptr;
template <typename DBParams>
int* wikipedia::wikipedia_loader<DBParams>::page_last_rev_ids = nullptr;
template <typename DBParams>
int* wikipedia::wikipedia_loader<DBParams>::page_last_rev_lens = nullptr;

// @section: clp parser definitions
enum {
    opt_dbid = 1, opt_nthrs, opt_users, opt_pages, opt_time, opt_gc, opt_comm, opt_perf, opt_pfcnt
};

static const Clp_Option options[] = {
        { "dbid",         'i', opt_dbid,  Clp_ValString, Clp_Optional },
        { "nthreads",     't', opt_nthrs, Clp_ValInt,    Clp_Optional },
        { "scaleusers",   'u', opt_users, Clp_ValInt,    Clp_Optional },
        { "scalepages",   'g', opt_pages, Clp_ValInt,    Clp_Optional },
        { "time",         'l', opt_time,  Clp_ValDouble, Clp_Optional },
        { "garbage-collect", 'b', opt_gc, Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "commute",      'x', opt_comm,  Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "perf",         'p', opt_perf,  Clp_NoVal,     Clp_Optional },
        { "perf-counter", 'c', opt_pfcnt, Clp_NoVal,     Clp_Negate | Clp_Optional }
};

static inline void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss << "Usage of " << std::string(argv_0) << ":" << std::endl
       << "  --dbid=<STRING> (or -i<STRING>)" << std::endl
       << "    Specify the type of DB concurrency control used. Can be one of the followings:" << std::endl
       << "      default, opaque, 2pl, adaptive, swiss, tictoc" << std::endl
       << "  --nthreads=<NUM> (or -t<NUM>)" << std::endl
       << "    Specify the number of parallel worker threads (default 1)." << std::endl
       << "  --scaleusers=<NUM> (or -u<NUM>)" << std::endl
       << "    Specify the scale factor of the number of users (default 10)." << std::endl
       << "  --scalepages=<NUM> (or -g<NUM>)" << std::endl
       << "    Specify the scale factor of the number of pages (default 10)." << std::endl
       << "  --time=<NUM> (or -l<NUM>)" << std::endl
       << "    Specify the time (duration) for which the benchmark is run (default 10 seconds)." << std::endl
       << "  --garbage-collect (or -b)" << std::endl
       << "    Enable garbage collection/epoch advancer thread." << std::endl
       << "  --commute (or -x)" << std::endl
       << "    Enable commutative update support." << std::endl
       << "  --perf (or -p)" << std::endl
       << "    Spawns perf profiler in record mode for the duration of the benchmark run." << std::endl
       << "  --perf-counter (or -c)" << std::endl
       << "    Spawns perf profiler in counter mode for the duration of the benchmark run." << std::endl;
    std::cout << ss.str() << std::flush;
}

struct cmd_params {
    db_params::db_params_id db_id;
    int num_threads;
    int scale_user;
    int scale_page;
    double time;
    bool enable_gc;
    bool enable_comm;
    bool spawn_perf;
    bool perf_counter_mode;

    explicit cmd_params()
        : db_id(db_params::db_params_id::Default),
          num_threads(1), scale_user(10), scale_page(10),
          time(10.0), enable_gc(false), enable_comm(false),
          spawn_perf(false), perf_counter_mode(false) {}
};

// @endsection: clp parser definitions

template <typename DBParams>
class bench_access {
public:
    using db_type = wikipedia::wikipedia_db<DBParams>;
    using loader_type = wikipedia::wikipedia_loader<DBParams>;
    using runner_type = wikipedia::wikipedia_runner<DBParams>;
    using profiler_type = bench::db_profiler;

    static void runner_thread(runner_type& r, size_t& txn_cnt) {
        r.run();
        txn_cnt = r.total_commits();
    }

    static int execute(cmd_params p) {
        size_t num_users = wikipedia::constants::users * (size_t)p.scale_user;
        size_t num_pages = wikipedia::constants::pages * (size_t)p.scale_page;
        wikipedia::load_params lp = {num_users, num_pages};
        wikipedia::run_params rp(num_users, num_pages, p.time, wikipedia::workload_weightgram);

        // Create DB
        auto& db = *(new db_type());

        // Load DB
        loader_type loader(db, lp);
        loader.load();

        // Start the GC thread if necessary
        std::thread advancer;
        std::cout << "Garbage collection: " << (p.enable_gc ? "enabled" : "disabled") << std::endl;
        if (p.enable_gc) {
            Transaction::set_epoch_cycle(1000);
            advancer = std::thread(&Transaction::epoch_advancer, nullptr);
        }

        // Execute benchmark
        std::vector<runner_type> runners;
        std::vector<std::thread> runner_threads;
        std::vector<size_t> committed_txn_cnts((size_t)p.num_threads, 0);

        for (int id = 0; id < p.num_threads; ++id) {
            runners.push_back(runner_type(id, db, rp));
        }

        profiler_type profiler(p.spawn_perf);
        profiler.start(p.perf_counter_mode ? Profiler::perf_mode::counters : Profiler::perf_mode::record);

        for (int t = 0; t < p.num_threads; ++t) {
            runner_threads.push_back(
                std::thread(runner_thread, std::ref(runners[t]), std::ref(committed_txn_cnts[t]))
            );
        }
        for (auto& t : runner_threads) {
            t.join();
        }

        size_t total_commit_txns = 0;
        for (auto c : committed_txn_cnts) {
            total_commit_txns += c;
        }
        profiler.finish(total_commit_txns);

        Transaction::rcu_release_all(advancer, p.num_threads);

        //print_abort_histogram(runners);

        delete (&db);
        return 0;
    }

    static void print_abort_histogram(std::vector<runner_type>& runners) {
        std::stringstream ss;
        std::vector<size_t> overall_histogram(wikipedia::workload_weightgram.size(), 0ul);
        int idx;
        for (runner_type& r : runners) {
            idx = 0;
            for (size_t n : r.aborts_by_txn()) {
                overall_histogram[idx] += n;
                ++idx;
            }
        }

        idx = 0;
        ss << "Abort analysis:" << std::endl;
        ss << '{';
        for (size_t n : overall_histogram) {
            ss << static_cast<wikipedia::TxnType>(idx) << ":" << n;
            if ((size_t)idx != (overall_histogram.size() - 1))
                ss << ", ";
            ++idx;
        }
        ss << '}';

        std::cout << ss.str() << std::endl;
    }
};

double constants::processor_tsc_frequency;

int main(int argc, const char * const *argv) {
    cmd_params params;

    Sto::global_init();
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

    int ret_code = 0;
    int opt;
    bool clp_stop = false;
    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
        switch (opt) {
        case opt_dbid:
            params.db_id = parse_dbid(clp->val.s);
            if (params.db_id == db_params::db_params_id::None) {
                std::cout << "Unsupported DB CC id: "
                          << ((clp->val.s == nullptr) ? "" : std::string(clp->val.s)) << std::endl;
                print_usage(argv[0]);
                ret_code = 1;
                clp_stop = true;
            }
            break;
        case opt_nthrs:
            params.num_threads = clp->val.i;
            break;
        case opt_users:
            params.scale_user = clp->val.i;
            break;
        case opt_pages:
            params.scale_page = clp->val.i;
            break;
        case opt_time:
            params.time = clp->val.d;
            break;
        case opt_gc:
            params.enable_gc = !clp->negated;
            break;
        case opt_comm:
            params.enable_comm = !clp->negated;
            break;
        case opt_perf:
            params.spawn_perf = !clp->negated;
            break;
        case opt_pfcnt:
            params.perf_counter_mode = !clp->negated;
            break;
        default:
            print_usage(argv[0]);
            ret_code = 1;
            clp_stop = true;
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

    switch (params.db_id) {
        case db_params_id::Default:
            ret_code = params.enable_comm ?
                    bench_access<db_default_commute_params>::execute(params) :
                    bench_access<db_default_params>::execute(params);
            break;
        case db_params_id::TicToc:
            ret_code = params.enable_comm ?
                    bench_access<db_tictoc_commute_node_params>::execute(params) :
                    bench_access<db_tictoc_node_params>::execute(params);
            break;
#if 0
        case db_params_id::Opaque:
            ret_code = bench_access<db_opaque_params>::execute(params);
            break;
        case db_params_id::TwoPL:
            ret_code = bench_access<db_2pl_params>::execute(params);
            break;
        case db_params_id::Adaptive:
            ret_code = bench_access<db_adaptive_params>::execute(params);
            break;
        case db_params_id::Swiss:
            ret_code = bench_access<db_swiss_params>::execute(params);
            break;
#endif
        case db_params_id::MVCC:
            ret_code = params.enable_comm ?
                    bench_access<db_mvcc_commute_params>::execute(params) :
                    bench_access<db_mvcc_params>::execute(params);
            break;
        default:
            std::cerr << "unknown db config parameter id" << std::endl;
            ret_code = 1;
            break;
    };

    return ret_code;
}
