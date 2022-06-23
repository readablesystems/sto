#pragma once

#include <any>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <sampling.hh>
#include <thread>

#include <PlatformFeatures.hh>

#include "clp.h"
#include "compiler.hh"
#include "sampling.hh"
//#include "Wikipedia_structs.hh"
//#include "Wikipedia_commutators.hh"

//#if TABLE_FINE_GRAINED
//#include "Wikipedia_selectors.hh"
//#endif


#include "DB_index.hh"
#include "DB_params.hh"
#include "DB_profiler.hh"
#include "DB_structs.hh"

//#include "wiki_split_params_ts.hh"
//#include "wiki_split_params_default.hh"

#include "Common_keys.hh"

namespace db_common {

// @section: clp parser definitions
struct ClpCommandParams {
    explicit ClpCommandParams()
        : db_id(db_params::db_params_id::Default),
          num_threads(1), time(10.0), enable_gc(false), enable_comm(false),
          split_type(db_params::db_split_type::None),
          spawn_perf(false), perf_counter_mode(false) {}

    static constexpr const char* enabled_str(bool enabled) {
        return enabled ? "enabled" : "disabled";
    }

    virtual void print() {
        std::cout << "====================="  << std::endl
                  << "DB id               : " << db_id << std::endl
                  << "# threads           : " << num_threads  << std::endl
                  << "Time                : " << time << " seconds" << std::endl
                  << "Garbage collection  : " << enabled_str(enable_gc) << std::endl
                  << "Deferred updates    : " << enabled_str(enable_comm) << std::endl
                  << "Timestamp splitting : " << split_type << std::endl
                  << "Spawn perf          : " << enabled_str(spawn_perf) << std::endl
                  << "Perf mode           : " << (perf_counter_mode ? "counter" : "default") << std::endl
                  << "====================="  << std ::endl;
    }

    db_params::db_params_id db_id;
    int num_threads;
    double time;
    bool enable_gc;
    bool enable_comm;
    db_params::db_split_type split_type;
    bool spawn_perf;
    bool perf_counter_mode;
};

template <class CommandParams = ClpCommandParams>
class ClpParser {
public:
    enum {
        opt_help = 1, opt_dbid, opt_nthrs, opt_time, opt_gc, opt_comm, opt_splt,
        opt_perf, opt_pfcnt, opt_addl
    };

    using ResolverType = std::function<bool(CommandParams&, std::any)>;

    ClpParser(int argc, const char * const* argv)
        : argc(argc), argv(argv), options(), resolvers(), error(false),
          addl_options_count(0) {
        for (const Clp_Option& option : BaseOptions) {
            options.emplace_back(option);
        }
        usage  << "Usage of " << std::string(argv[0]) << ":" << std::endl
               << "  --help (or -h)" << std::endl
               << "    Print this help menu and exit." << std::endl
               << "  --dbid=<STRING> (or -i<STRING>)" << std::endl
               << "    Specify the type of DB concurrency control used. Can be one of:" << std::endl
               << "      default, opaque, 2pl, adaptive, swiss, tictoc" << std::endl
               << "  --nthreads=<INT> (or -t<INT>)" << std::endl
               << "    Specify the number of parallel worker threads (default 1)." << std::endl
               << "  --time=<FLOAT> (or -l<FLOAT>)" << std::endl
               << "    Specify the time (duration) for which the benchmark is run (default 10 seconds)." << std::endl
               << "  --gc (or -g)" << std::endl
               << "    Enable garbage collection/epoch advancer thread." << std::endl
               << "  --commute (or -x)" << std::endl
               << "    Enable commutative update support." << std::endl
               << "  --split=<STRING> (or -s<STRING>)" << std::endl
               << "    Specify the split algorithm to use. Can be one of:" << std::endl
               << "      none (default), static, adaptive" << std::endl
               << "  --perf (or -p)" << std::endl
               << "    Spawns perf profiler in record mode for the duration of the benchmark run." << std::endl
               << "  --perf-counter (or -c)" << std::endl
               << "    Spawns perf profiler in counter mode for the duration of the benchmark run." << std::endl;
    }

private:
    static constexpr Clp_Option BaseOptions[] = {
        { "help",         'h', opt_help,  Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "dbid",         'i', opt_dbid,  Clp_ValString, Clp_Optional },
        { "nthreads",     't', opt_nthrs, Clp_ValInt,    Clp_Optional },
        { "time",         'l', opt_time,  Clp_ValDouble, Clp_Optional },
        { "gc",           'g', opt_gc,    Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "commute",      'x', opt_comm,  Clp_NoVal,     Clp_Negate | Clp_Optional },
        { "split",        's', opt_splt,  Clp_ValString, Clp_Optional },
        { "perf",         'p', opt_perf,  Clp_NoVal,     Clp_Optional },
        { "perf-counter", 'c', opt_pfcnt, Clp_NoVal,     Clp_Negate | Clp_Optional }
        };

public:
    template <typename... Args>
    void AddOption(
            const char* long_name, char short_name, int type, int flags,
            ResolverType&& resolver, Args... args) {
        for (auto& option : options) {
            if (!strncmp(option.long_name, long_name, strlen(long_name) + 1)) {
                std::cerr << "Conflicting long names for arguments: " << long_name << std::endl;
                error = true;
            }
            if (option.short_name == short_name && short_name) {
                std::cerr << "Conflicting short names for arguments: " << short_name << std::endl;
                error = true;
            }
        }
        options.emplace_back((Clp_Option){
                long_name, short_name, opt_addl + (addl_options_count++), type, flags
                });
        resolvers.push_back(resolver);
        if (type == Clp_NoVal) {
            usage << "  --" << long_name << " (or -" << short_name << ")" << std::endl;
        } else {
            const char* type_string = "=<VALUE>";
            switch (type) {
                case Clp_ValBool:
                    type_string = "=<BOOL>";
                    break;
                case Clp_ValDouble:
                    type_string = "=<FLOAT>";
                    break;
                case Clp_ValInt:
                    type_string = "=<INT>";
                    break;
                case Clp_ValLong:
                    type_string = "=<LONG>";
                    break;
                case Clp_ValString:
                    type_string = "=<STRING>";
                    break;
                case Clp_NoVal:
                    type_string = "";
                    break;
                default:
                    type_string = "=<VALUE>";
                    break;
            }
            usage << "  --" << long_name << type_string
                  << " (or -" << short_name << type_string << ")" << std::endl;
        }
        if constexpr (sizeof...(Args) > 0) {
            AddDescription(std::forward<Args>(args)...);
        }
    }

private:
    template <typename... Args>
    void AddDescription(const char* description, Args... args) {
        usage << "    " << description << std::endl;
        if constexpr (sizeof...(Args) > 0) {
            AddDescription(std::forward<Args>(args)...);
        }
    }

public:
    int GetParams(CommandParams& params) {
        if (error) {
            return 1;
        }

        Clp_Parser *clp = GetParser();
        int opt;
        int ret_code = 0;
        bool clp_stop = false;
        bool noargs = true;
        while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
            switch (opt) {
            case opt_help:
                PrintUsage();
                return 1;
            case opt_dbid:
                params.db_id = db_params::parse_dbid(clp->val.s);
                if (params.db_id == db_params::db_params_id::None) {
                    std::cout << "Unsupported DB CC id: "
                              << ((clp->val.s == nullptr) ? "" : std::string(clp->val.s)) << std::endl;
                    ret_code = 1;
                }
                break;
            case opt_nthrs:
                params.num_threads = clp->val.i;
                if (params.num_threads < 1) {
                    std::cerr << "Invalid thread count: " << params.num_threads << std::endl;
                    ret_code = 1;
                }
                break;
            case opt_time:
                params.time = clp->val.d;
                if (params.time < 1) {
                    std::cerr << "Cannot run benchmark for less than 1 second." << std::endl;
                    ret_code = 1;
                }
                break;
            case opt_gc:
                params.enable_gc = !clp->negated;
                break;
            case opt_comm:
                params.enable_comm = !clp->negated;
                break;
            case opt_splt:
                params.split_type = db_params::parse_split_type(clp->val.s);
                if (params.split_type == db_params::db_split_type::Invalid) {
                    std::cerr << "Invalid timestamp splitting algorithm: " << clp->val.s << std::endl;
                    ret_code = 1;
                }
                break;
            case opt_perf:
                params.spawn_perf = !clp->negated;
                break;
            case opt_pfcnt:
                params.perf_counter_mode = !clp->negated;
                break;
            default:
                auto opt_index = opt - opt_addl;
                if (opt_index >= 0 && opt_index < addl_options_count) {
                    switch (clp->option->val_type) {
                        case Clp_ValBool:
                            ret_code = !resolvers[opt_index](params, std::make_any<bool>(clp->val.i));
                            break;
                        case Clp_ValDouble:
                            ret_code = !resolvers[opt_index](params, std::make_any<double>(clp->val.d));
                            break;
                        case Clp_ValInt:
                            ret_code = !resolvers[opt_index](params, std::make_any<int>(clp->val.i));
                            break;
                        case Clp_ValLong:
                            ret_code = !resolvers[opt_index](params, std::make_any<long>(clp->val.i));
                            break;
                        case Clp_ValString:
                            ret_code = !resolvers[opt_index](params, std::make_any<const char*>(clp->val.s));
                            break;
                        case Clp_NoVal:
                            ret_code = !resolvers[opt_index](params, std::make_any<bool>(!clp->negated));
                            break;
                        default:
                            ret_code = !resolvers[opt_index](params, std::make_any<void*>(&clp->val));
                            break;
                    }
                    if (!ret_code) {
                        break;
                    }
                }
                ret_code = 1;
                break;
            }
            noargs = false;
            if (ret_code) {
                PrintUsage();
                clp_stop = true;
            }
        }

        Clp_DeleteParser(clp);
        if (noargs) {
            std::cout << "\033[32;1mExecuting default configuration. For help, use -h.\033[m" << std::endl;
        }
        return ret_code;
    }

private:
    Clp_Parser* GetParser() {
        return Clp_NewParser(argc, argv, options.size(), &options[0]);
    }

public:
    inline void PrintUsage() {
        std::cout << usage.str() << std::flush;
    }

    int argc;
    const char * const* argv;
    std::vector<Clp_Option> options;
    std::vector<ResolverType> resolvers;
    std::stringstream usage;
    bool error;
    int addl_options_count;
};
// @endsection: clp parser definitions

struct common_run_params {
    double time_limit;
};

// @section DB definitions
template <typename DBParamsValue>
class common_db {
public:
    using DBParams = DBParamsValue;

    template <typename K, typename V>
    using OIndex = typename std::conditional<DBParams::MVCC && !DBParams::UseATS,
          bench::mvcc_ordered_index<K, V, DBParams>,
          bench::ordered_index<K, V, DBParams>>::type;
    template <typename K, typename V>
    using UIndex = typename std::conditional<DBParams::MVCC && !DBParams::UseATS,
          bench::mvcc_unordered_index<K, V, DBParams>,
          bench::unordered_index<K, V, DBParams>>::type;

    // Constructor
    template <typename CommandParams>
    common_db(CommandParams&) : rng(topo_info.num_cpus) {
        int thread_id = 0;
        for (auto& gen : rng) {
            gen.seed(thread_id);
        }
    }
    common_db() = delete;

    // Destructor
    virtual ~common_db() = default;

    // Prepopulation runner
    template <uint64_t cores = 0>
    void prepopulate(std::function<void(uint64_t)> driver) {
        uint64_t nthreads = cores < 1 ? topo_info.num_cpus : cores;
        std::vector<std::thread> threads;
        for (uint64_t thread_id = 0; thread_id < nthreads; ++thread_id) {
            threads.emplace_back([&] (uint64_t thread_id) {
                set_affinity(thread_id);
                thread_init_all();

                driver(thread_id);
            }, thread_id);
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    // Prepopulation/setup step
    template <typename CommandParams, typename RunParams>
    void setup(CommandParams&, RunParams&) {}

    // Thread initialization
    virtual void thread_init_all() = 0;

    template <typename DB, typename... DBs>
    void thread_init(DB& db, DBs&... dbs) {
        // Thread initializations
        db.thread_init();
        if constexpr (sizeof...(DBs)) {
            thread_init(std::forward<DBs&>(dbs)...);
        }
    }

    std::vector<sampling::StoRandomDistribution<>::rng_type> rng;
};

template <class db_type>
class common_runner {
public:
    using DBParams = typename db_type::DBParams;

    template <typename T>
    using Record = typename std::conditional_t<
        DBParams::Split == db_params::db_split_type::Adaptive,
        typename T::RecordAccessor,
            std::conditional_t<
            DBParams::MVCC,
            bench::SplitRecordAccessor<T, (static_cast<int>(DBParams::Split) > 0)>,
            bench::UniRecordAccessor<T, (static_cast<int>(DBParams::Split) > 0)>>>;

    static constexpr bool Commute = DBParams::Commute;

    common_runner(int runner_id, db_type& database, const common_run_params& params)
        : id(runner_id), db(database), tsc_elapse_limit(),
          stats_total_commits() {
        tsc_elapse_limit =
                (uint64_t)(params.time_limit * db_params::constants::processor_tsc_frequency() * db_params::constants::billion);
    }

    void run();  // Infrastructure and framework for timing a workload
    virtual size_t run_txn() = 0;  // Makes one transaction call

    // Returns number of transactions committed
    size_t total_commits() const {
        return stats_total_commits;
    }

protected:
    int id;
    db_type& db;
    uint64_t tsc_elapse_limit;
    size_t stats_total_commits;
};

template <class db_type>
void common_runner<db_type>::run() {
    ::TThread::set_id(id);
    set_affinity(id);
    db.thread_init_all();

    auto tsc_begin = read_tsc();
    size_t count = 0;
    while (true) {
        count += run_txn();
        if ((read_tsc() - tsc_begin) >= tsc_elapse_limit)
            break;
    }
    stats_total_commits = count;
}

template <typename DBParams, class db_type, class runner_type>
class bench_access {
public:
    using profiler_type = bench::db_profiler;

    static void runner_thread(runner_type& r, size_t& txn_count) {
        r.run();
        txn_count = r.total_commits();
    }

    template <typename CommandParams>
    static int execute(CommandParams& params) {
        // Create run parameters
        common_run_params run_params = {.time_limit = params.time};

        // Create DB
        auto& db = *(new db_type(params));

        // Load the benchmark/prepopulate the database
        db.setup(params, run_params);

        // Start the GC thread if necessary
        std::thread advancer;
        std::cout << "Garbage collection: " << (params.enable_gc ? "enabled" : "disabled") << std::endl;
        if (params.enable_gc) {
            Transaction::set_epoch_cycle(1000);
            advancer = std::thread(&Transaction::epoch_advancer, nullptr);
        }

        // Execute benchmark
        std::vector<runner_type> runners;
        std::vector<std::thread> runner_threads;
        std::vector<size_t> committed_txn_counts(static_cast<size_t>(params.num_threads), 0);

        for (int id = 0; id < params.num_threads; ++id) {
            runners.push_back(runner_type(id, db, run_params));
        }

        profiler_type profiler(params.spawn_perf);
        profiler.start(params.perf_counter_mode ? Profiler::perf_mode::counters : Profiler::perf_mode::record);

        for (int t = 0; t < params.num_threads; ++t) {
            runner_threads.push_back(
                std::thread(runner_thread, std::ref(runners[t]), std::ref(committed_txn_counts[t]))
            );
        }
        for (auto& t : runner_threads) {
            t.join();
        }

        size_t total_commit_txns = 0;
        for (auto c : committed_txn_counts) {
            total_commit_txns += c;
        }
        profiler.finish(total_commit_txns);

        Transaction::rcu_release_all(advancer, params.num_threads);

        delete (&db);
        return 0;
    }
};

template <template <typename T> class bench_type, class CommandParams>
int run(ClpParser<CommandParams>& parser) {

    Sto::global_init();
    CommandParams params;
    int ret_code = parser.GetParams(params);
    if (ret_code != 0) {
        return ret_code;
    }

    params.print();

    auto cpu_freq = determine_cpu_freq();
    if (cpu_freq == 0.0) {
        return 1;
    }
    else {
        db_params::constants::processor_tsc_frequency() = cpu_freq;
    }

    switch (params.db_id) {
        case db_params::db_params_id::Default:
            if (params.enable_comm) {
                switch (params.split_type) {
                case db_params::db_split_type::None:
                    return bench_type<db_params::db_default_commute_params>::execute(params);
                case db_params::db_split_type::Static:
                    return bench_type<db_params::db_default_sts_commute_params>::execute(params);
                case db_params::db_split_type::Adaptive:
                    return bench_type<db_params::db_default_ats_commute_params>::execute(params);
                default:
                    std::cerr << "Invalid TS choice" << std::endl;
                    ret_code = 1;
                    break;
                }
            } else {
                switch (params.split_type) {
                case db_params::db_split_type::None:
                    return bench_type<db_params::db_default_params>::execute(params);
                case db_params::db_split_type::Static:
                    return bench_type<db_params::db_default_sts_params>::execute(params);
                case db_params::db_split_type::Adaptive:
                    return bench_type<db_params::db_default_ats_params>::execute(params);
                default:
                    std::cerr << "Invalid TS choice" << std::endl;
                    ret_code = 1;
                    break;
                }
            }
            break;
        case db_params::db_params_id::TicToc:
            ret_code = params.enable_comm ?
                    bench_type<db_params::db_tictoc_commute_node_params>::execute(params) :
                    bench_type<db_params::db_tictoc_node_params>::execute(params);
            break;
#if 0
        case db_params::db_params_id::Opaque:
            ret_code = bench_type<db_params::db_opaque_params>::execute(params);
            break;
        case db_params::db_params_id::TwoPL:
            ret_code = bench_type<db_params::db_2pl_params>::execute(params);
            break;
        case db_params::db_params_id::Adaptive:
            ret_code = bench_type<db_params::db_adaptive_params>::execute(params);
            break;
        case db_params::db_params_id::Swiss:
            ret_code = bench_type<db_params::db_swiss_params>::execute(params);
            break;
#endif
        case db_params::db_params_id::MVCC:
            if (params.enable_comm) {
                switch (params.split_type) {
                case db_params::db_split_type::None:
                    return bench_type<db_params::db_mvcc_commute_params>::execute(params);
                case db_params::db_split_type::Static:
                    return bench_type<db_params::db_mvcc_sts_commute_params>::execute(params);
                case db_params::db_split_type::Adaptive:
                    return bench_type<db_params::db_mvcc_ats_commute_params>::execute(params);
                default:
                    std::cerr << "Invalid TS choice" << std::endl;
                    ret_code = 1;
                    break;
                }
            } else {
                switch (params.split_type) {
                case db_params::db_split_type::None:
                    return bench_type<db_params::db_mvcc_params>::execute(params);
                case db_params::db_split_type::Static:
                    return bench_type<db_params::db_mvcc_sts_params>::execute(params);
                case db_params::db_split_type::Adaptive:
                    return bench_type<db_params::db_mvcc_ats_params>::execute(params);
                default:
                    std::cerr << "Invalid TS choice" << std::endl;
                    ret_code = 1;
                    break;
                }
            }
            break;
        default:
            std::cerr << "unknown db config parameter id" << std::endl;
            ret_code = 1;
            break;
    };

    return ret_code;
}

}; // namespace db_common
