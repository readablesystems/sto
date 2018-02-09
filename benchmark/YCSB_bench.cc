#include <sstream>
#include <iostream>
#include <thread>

#include "YCSB_bench.hh"
#include "YCSB_txns.hh"
#include "TPCC_bench.hh"
#include "PlatformFeatures.hh"

volatile mrcu_epoch_type active_epoch = 1;
volatile uint64_t globalepoch = 1;
volatile bool recovering = false;

namespace ycsb {


    enum {
        opt_dbid = 1, opt_nthrs, opt_mode, opt_time, opt_perf, opt_pfcnt
    };

    static const Clp_Option options[] = {
        { "dbid",         'i', opt_dbid,  Clp_ValString, Clp_Optional },
        { "nthreads",     't', opt_nthrs, Clp_ValInt,    Clp_Optional },
        { "mode",         'm', opt_mode,  Clp_ValInt,    Clp_Optional },
        { "time",         'l', opt_time,  Clp_ValDouble, Clp_Optional },
        { "perf",         'p', opt_perf,  Clp_NoVal,     Clp_Optional },
        { "perf-counter", 'c', opt_pfcnt, Clp_NoVal,     Clp_Negate| Clp_Optional }
    };

    void print_usage(const char *prog_name) {
        (void)prog_name;
        std::cout << ":)" << std::endl;
    }

    template <typename DBParams>
    void ycsb_prepopulation_thread(int thread_id, ycsb_db<DBParams>& db, uint64_t key_begin, uint64_t key_end) {
        set_affinity(thread_id);
        ycsb_input_generator<DBParams> ig(thread_id);
        db.table_thread_init();
        for (uint64_t i = key_begin; i < key_end; ++i) {
            db.ycsb_table().nontrans_put(ycsb_key(i), ig.random_ycsb_value());
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
        static void ycsb_runner_thread(ycsb_db<DBParams>& db, tpcc::tpcc_profiler& prof, ycsb_runner<DBParams>& runner, double time_limit, uint64_t& txn_cnt) {
            uint64_t local_cnt = 0;
            db.table_thread_init();

            ::TThread::set_id(runner.id());
            set_affinity(runner.id());

            uint64_t tsc_diff = (uint64_t)(time_limit * tpcc::constants::processor_tsc_frequency * tpcc::constants::billion);
            auto start_t = prof.start_timestamp();

            auto it = runner.workload.begin();

            while (true) {
                auto curr_t = read_tsc();
                if ((curr_t - start_t) >= tsc_diff)
                    break;

                runner.run_txn(*it);
                ++it;
                if (it == runner.workload.end())
                    it = runner.workload.begin();

                ++local_cnt;
            }

            txn_cnt = local_cnt;
        }

        static void workload_generation(std::vector<ycsb_runner<DBParams>>& runners, int mode) {
            std::vector<std::thread> thrs;
            int tsize = (mode == static_cast<int>(mode_id::ReadOnly)) ? 2 : 16;
            for (auto& r : runners) {
                thrs.emplace_back(&ycsb_runner<DBParams>::gen_workload, &r, tsize);
            }
            for (auto& t : thrs)
                t.join();
        }

        static uint64_t run_benchmark(ycsb_db<DBParams>& db, tpcc::tpcc_profiler& prof, std::vector<ycsb_runner<DBParams>>& runners, double time_limit) {
            int num_runners = runners.size();
            std::vector<std::thread> runner_thrs;
            std::vector<uint64_t> txn_cnts(size_t(num_runners), 0);

            for (int i = 0; i < num_runners; ++i) {
                fprintf(stdout, "runner %d created\n", i);
                runner_thrs.emplace_back(ycsb_runner_thread, std::ref(db), std::ref(prof),
                                         std::ref(runners[i]), time_limit, std::ref(txn_cnts[i]));
            }

            for (auto &t : runner_thrs)
                t.join();

            uint64_t total_txn_cnt = 0;
            for (auto& cnt : txn_cnts)
                total_txn_cnt += cnt;
            return total_txn_cnt;
        }


        static int execute(int argc, const char *const *argv) {
            int ret = 0;

            bool spawn_perf = false;
            bool counter_mode = false;
            int num_threads = 1;
            int mode = static_cast<int>(mode_id::ReadOnly);
            double time_limit = 10.0;

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
                    case opt_mode:
                        mode = clp->val.i;
                        break;
                    case opt_time:
                        time_limit = clp->val.d;
                        break;
                    case opt_perf:
                        spawn_perf = !clp->negated;
                        break;
                    case opt_pfcnt:
                        counter_mode = !clp->negated;
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

            tpcc::tpcc_profiler prof(spawn_perf);
            ycsb_db<DBParams> db;

            std::cout << "Prepopulating database..." << std::endl;
            db.prepopulate();
            std::cout << "Prepopulation complete." << std::endl;

            std::vector<ycsb_runner<DBParams>> runners;
            for (int i = 0; i < num_threads; ++i) {
                runners.emplace_back(i, db, static_cast<mode_id>(mode));
            }

            std::cout << "Generating workload..." << std::endl;
            workload_generation(runners, mode);
            std::cout << "Done." << std::endl;

            prof.start(profiler_mode);
            auto num_trans = run_benchmark(db, prof, runners, time_limit);
            prof.finish(num_trans);

            return 0;
        }


    };

}; // namespace ycsb

using namespace ycsb;

double tpcc::constants::processor_tsc_frequency;

int main(int argc, const char *const *argv) {
    db_params_id dbid = db_params_id::Default;
    int ret_code = 0;
   
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

    int opt;
    bool clp_stop = false;
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
        tpcc::constants::processor_tsc_frequency = cpu_freq;

    switch (dbid) {
    case db_params_id::Default:
        ret_code = ycsb_access<db_default_params>::execute(argc, argv);
        break;
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
    case db_params_id::TicToc:
        ret_code = ycsb_access<db_tictoc_params>::execute(argc, argv);
        break;
    default:
        std::cerr << "unknown db config parameter id" << std::endl;
        ret_code = 1;
        break;
    };

    return ret_code;

}
