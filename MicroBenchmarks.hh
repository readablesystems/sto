#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <thread>

#include "Sto.hh"
#include "TPCC_bench.hh"
#include "TPCC_index.hh"

#include "sampling.hh"
#include "PlatformFeatures.hh"
#include "SystemProfiler.hh"

namespace ubench {


enum class DsType : int {none, masstree};

struct UBenchParams {
    tpcc::db_params_id dbid;
    DsType datatype;
    uint32_t ntrans;
    uint32_t nthreads;
    uint32_t opspertrans;
    uint32_t opspertrans_ro;
    uint64_t key_sz;
    double time_limit;
    double zipf_skew;
    double readonly_percent;
    double write_percent;
    uint64_t proc_frequency_hz;
    bool dump_trace;
    bool profiler;
    bool ins_measure;
};

extern UBenchParams params;

enum class OpType : int {read, write, inc};

typedef int64_t value_type;

struct RWOperation {
    RWOperation() : type(OpType::read), key(), value() {}
    RWOperation(OpType t, StoSampling::index_t k, value_type v = value_type())
            : type(t), key(k), value(v) {}

    OpType type;
    StoSampling::index_t key;
    value_type value;
};

struct wl_measurement_params {
    static constexpr bool instantaneous_measurements = true;
};

struct wl_default_params {
    static constexpr bool instantaneous_measurements = false;
};

template <typename WLImpl>
class WorkloadGenerator {
public:
    typedef std::vector<RWOperation> query_type;
    typedef std::vector<query_type> workload_type;

    explicit WorkloadGenerator(size_t num_threads) {
        workloads.resize(num_threads);
    }

    void workload_init(int thread_id) {
        impl().workload_init_impl(thread_id);
    }

    std::vector<workload_type> workloads;

protected:
    WLImpl& impl() {
        return static_cast<WLImpl&>(*this);
    }
};

template <typename DSImpl, typename WLImpl>
class Tester {
public:
    typedef std::vector<std::pair<uint64_t, double>> measurement_type;

    // experimental flags
    static constexpr bool ins_measure = WLImpl::RTParams::instantaneous_measurements;

    explicit Tester(size_t num_threads) : wl_gen(num_threads) {
        measurements.resize(num_threads);
        for (auto& m : measurements) {
            m.reserve(10000);
        }
    }

    inline void execute();
    inline uint64_t run_benchmark(uint64_t start_tsc);

    void workload_init(int thread_id) {
        wl_gen.workload_init(thread_id);
    }
    void prepopulate() {
        ds_impl().prepopulate_impl();
    }
    void ds_thread_init() {
        ds_impl().thread_init_impl();
    }
    inline void run_thread(int thread_id, uint64_t start_tsc, uint64_t& txn_cnt);

    bool do_op(const RWOperation& op) {
        return ds_impl().do_op_impl(op);
    }

protected:
    DSImpl& ds_impl() {
        return static_cast<DSImpl&>(*this);
    }
    const DSImpl& ds_impl() const {
        return static_cast<const DSImpl&>(*this);
    }

    WorkloadGenerator<WLImpl> wl_gen;
    std::vector<measurement_type> measurements;
};

template <typename DSImpl, typename WLImpl>
void Tester<DSImpl, WLImpl>::execute() {
    tpcc::tpcc_profiler profiler(params.profiler);
    std::vector<std::thread> thread_pool;

    std::cout << "Generating workload..." << std::endl;
    for (auto i = 0u; i < params.nthreads; ++i)
        thread_pool.emplace_back(this->workload_init, this, (int)i);
    for (auto& t : thread_pool)
        t.join();
    thread_pool.clear();
    std::cout << "Prepopulating..." << std::endl;
    prepopulate();
    std::cout << "Running" << std::endl;

    profiler.start(Profiler::perf_mode::record);
    auto num_commits = run_benchmark(profiler.start_timestamp());
    profiler.finish(num_commits);
}

template <typename DSImpl, typename WLImpl>
uint64_t Tester<DSImpl, WLImpl>::run_benchmark(uint64_t start_tsc) {
    std::vector<std::thread> thread_pool;
    uint64_t total_txns = 0;

    uint64_t *txn_cnts = new uint64_t[params.nthreads];
    for (auto i = 0u; i < params.nthreads; ++i) {
        txn_cnts[i] = 0;
        thread_pool.emplace_back(this->run_thread, this, (int)i, start_tsc, txn_cnts[i]);
    }
    for (auto it = thread_pool.begin(); it != thread_pool.end(); ++it) {
        it->join();
        total_txns += txn_cnts[it - thread_pool.begin()];
    }
    delete[] txn_cnts;
    return total_txns;
}

template <typename DSImpl, typename WLImpl>
void Tester<DSImpl, WLImpl>::run_thread(int thread_id, uint64_t start_tsc, uint64_t& txn_cnt) {
    TThread::set_id(thread_id);
    set_affinity(thread_id);
    ds_thread_init();

    // thread workload
    auto& twl = wl_gen.workloads[thread_id];

    uint64_t local_txn_cnt = 0;

    uint64_t interval;
    uint64_t total_delta;
    typename WLImpl::workload_type::const_iterator it_prev;
    uint64_t prev_total_reads;
    uint64_t prev_total_opt_reads;

    uint64_t ticks_to_wait = (uint64_t)(params.time_limit * (double)(params.proc_frequency_hz));

    if (ins_measure) {
        interval = 10 * params.proc_frequency_hz / 1000;
        total_delta = interval;
        it_prev = twl.begin();
        prev_total_reads = 0;
        prev_total_opt_reads = 0;
    } else {
        (void)interval;
        (void)total_delta;
        (void)it_prev;
        (void)prev_total_reads;
        (void)prev_total_opt_reads;
    }

    bool stop = false;
    while (!stop) {
        for (auto txn_it = twl.begin(); txn_it != twl.end(); ++txn_it) {
            TRANSACTION {
                for (auto &req : *txn_it) {
                    bool ok = do_op(req);
                    TXN_DO(ok);
                }
                (void) __txn_committed;
            } RETRY(true);

            ++local_txn_cnt;
            uint64_t now = read_tsc();

            // instantaneous metrics measurements
            if (ins_measure) {
                if ((now - start_tsc) >= total_delta) {
                    total_delta += interval;
                    auto ntxns = txn_it - it_prev;
                    it_prev = txn_it;

                    auto total_reads = TXP_INSPECT(txp_total_r);
                    auto total_opt_reads = TXP_INSPECT(txp_total_adaptive_opt);

                    double opt_rate =
                            (double) (total_opt_reads - prev_total_opt_reads) / double(total_reads - prev_total_reads);

                    prev_total_reads = total_reads;
                    prev_total_opt_reads = total_opt_reads;

                    measurements[thread_id].emplace_back(ntxns, opt_rate);
                }
            }

            if ((now - start_tsc) >= ticks_to_wait) {
                stop = true;
                break;
            }
        }
    }

    txn_cnt = local_txn_cnt;
}

template <typename Params>
class WLZipfRW : public WorkloadGenerator<WLZipfRW<Params>> {
public:
    typedef WorkloadGenerator<WLZipfRW> WG;
    typedef Params RTParams;

    explicit WLZipfRW(size_t num_threads) : WG(num_threads) {}

    void workload_init_impl(int thread_id) {
        StoSampling::StoUniformDistribution ud(thread_id, 0, std::numeric_limits<uint32_t>::max());
        StoSampling::StoRandomDistribution *dd = nullptr;

        if (params.zipf_skew == 0.0)
            dd = new StoSampling::StoUniformDistribution(thread_id, 0, params.key_sz - 1);
        else
            dd = new StoSampling::StoZipfDistribution(thread_id, 0, params.key_sz - 1, params.zipf_skew);

        auto ro_threshold = (uint32_t)(std::numeric_limits<uint32_t>::max() * params.readonly_percent);
        auto write_threshold = (uint32_t)(std::numeric_limits<uint32_t>::max() * params.write_percent);

        auto& thread_workload = this->workloads[thread_id];
        uint64_t trans_per_thread = params.ntrans / params.nthreads;

        for (uint64_t i = 0; i < trans_per_thread; ++i) {
            typename WG::query_type query;
            bool read_only = ud.sample() < ro_threshold;
            int nops = read_only ? params.opspertrans_ro : params.opspertrans;

            std::set<StoSampling::index_t> idx_set;
            while (idx_set.size() != (size_t)nops) {
                idx_set.insert(dd->sample());
            }

            for (auto idx : idx_set) {
                if (read_only)
                    query.emplace_back(OpType::read, idx);
                else {
                    if (ud.sample() < write_threshold)
                        query.emplace_back(OpType::write, idx, idx + 1);
                    else
                        query.emplace_back(OpType::read, idx);
                }
            }
            thread_workload.emplace_back(std::move(query));
        }
#if 0
        if (params.dump_trace)
            dump_thread_trace(thread_id, thread_workload);
#endif
        delete dd;
    }
};

template <typename WLImpl, typename DBParams>
class MasstreeTester : public Tester<MasstreeTester<WLImpl, DBParams>, WLImpl> {
public:
    typedef typename Tester<MasstreeTester<WLImpl, DBParams>, WLImpl> Base;

    explicit MasstreeTester(size_t num_threads) : Base(num_threads), mt_() {}

    void prepopulate_impl() {
        for (unsigned int i = 0; i < 100000; ++i)
            mt_.nontrans_put(i, i);
    }

    void thread_init_impl() {
        mt_.thread_init();
    }

    bool do_op_impl(const RWOperation& op) {
        bool success;
        switch(op.type) {
            case OpType::read:
                std::tie(success, std::ignore, std::ignore, std::ignore)
                        = mt_.select_row(op.key, false);
                break;
            case OpType::write:
                std::tie(success, std::ignore) = mt_.insert_row(op.key, &op.value);
                break;
            case OpType::inc: {
                uintptr_t rid;
                const value_type* value;
                std::tie(success, std::ignore, rid, value)
                        = mt_.select_row(op.key, true);
                if (!success)
                    break;
                value_type new_v = (*value) + 1;
                mt_.update_row(rid, &new_v);
                break;
            }
        }
        return success;
    }

private:
    tpcc::ordered_index<StoSampling::index_t, value_type, DBParams> mt_;
};

template <DsType DS, typename WLImpl, typename DBParams>
struct TesterSelector {};

template <typename WLImpl, typename DBParams>
struct TesterSelector<DsType::masstree, WLImpl, DBParams> {
    typedef MasstreeTester<WLImpl, DBParams> type;
};

template <typename DBParams>
using MtZipfTesterDefault = TesterSelector<DsType::masstree, WLZipfRW<wl_default_params>, DBParams>;

template <typename DBParams>
using MtZipfTesterMeasure = TesterSelector<DsType::masstree, WLZipfRW<wl_measurement_params>, DBParams>;

};

