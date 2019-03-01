#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <thread>

#include "Sto.hh"
#include "DB_profiler.hh"
#include "DB_structs.hh"
#include "DB_index.hh"
#include "DB_params.hh"

#include "Micro_structs.hh"

#include "sampling.hh"
#include "PlatformFeatures.hh"

namespace ubench {

using namespace db_params;
using bench::db_profiler;

enum class DsType : int {none, masstree};

const char *datatype_names[] = {"none", "masstree"};

inline DsType parse_datatype(const char *s) {
    if (s == nullptr)
        return DsType::none;

    for (size_t i = 0; i < sizeof(datatype_names); ++i) {
        if (std::string(datatype_names[i]) == std::string(s))
            return static_cast<DsType>(i);
    }
    return DsType::none;
}

struct UBenchParams {
    db_params_id dbid;
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
    uint32_t granules;
    bool ins_measure;
};

inline std::ostream& operator<<(std::ostream& os, const UBenchParams& p) {
    os << "Benchmark finished using CC=" << db_params::db_params_id_names[static_cast<int>(p.dbid)] << ", ";
    os << "datatype=" << datatype_names[static_cast<int>(p.datatype)] << " with "
       << p.nthreads << " threads" << std::endl;
    os << p.ntrans << " txns, " << p.opspertrans << " ops/txn (" << p.opspertrans_ro
       << " in read-only txns)" << std::endl;
    os << "Read-only fraction = " << p.readonly_percent << ", write ratio = " << p.write_percent << std::endl;
    os << "zipf skew = " << p.zipf_skew << ", " << "key space size = " << p.key_sz << std::endl;
    return os;
}

extern UBenchParams params;

enum class OpType : int {read, write, inc};

struct RWOperation {
    typedef int64_t value_type;
    RWOperation() : type(OpType::read), key(), value() {}
    RWOperation(OpType t, sampling::index_t k, value_type v = value_type())
            : type(t), key(k), value(v) {}

    OpType type;
    sampling::index_t key;
    value_type value;
};

inline std::ostream& operator<<(std::ostream& os, const RWOperation& op) {
    os << "[";
    if (op.type == OpType::read) {
        os << "r,k=" << op.key;
    } else if (op.type == OpType::write) {
        os << "w,k=" << op.key << ",v=" << op.value;
    } else {
        assert(op.type == OpType::inc);
        os << "inc,k=" << op.key;
    }
    os << "]";
    return os;
}

template <int Granules>
struct wl_default_params {
    static constexpr bool instantaneous_measurements = false;
    static constexpr int granules = Granules;
};

template <int Granules>
struct wl_measurement_params : public wl_default_params<Granules> {
    static constexpr bool instantaneous_measurements = true;
};

template <int>
struct compute_value_type {
};

template <>
struct compute_value_type<1> {
    typedef one_version_row type;
};

template <>
struct compute_value_type<2> {
    typedef two_version_row type;
};

template <>
struct compute_value_type<4> {
    typedef four_version_row type;
};

template <>
struct compute_value_type<8> {
    typedef eight_version_row type;
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
    db_profiler profiler(params.profiler);
    std::vector<std::thread> thread_pool;

    std::cout << "Generating workload..." << std::endl;
    for (auto i = 0u; i < params.nthreads; ++i)
        thread_pool.emplace_back(&Tester<DSImpl, WLImpl>::workload_init,
                                 this, (int)i);
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
        thread_pool.emplace_back(&Tester<DSImpl, WLImpl>::run_thread,
                                 this, (int)i, start_tsc, std::ref(txn_cnts[i]));
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
        typedef sampling::StoRandomDistribution<>::rng_type rng_type;
        rng_type rng(thread_id);

        sampling::StoUniformDistribution<> ud(rng, 0, std::numeric_limits<uint32_t>::max());
        sampling::StoRandomDistribution<> *dd = nullptr;

        if (params.zipf_skew == 0.0)
            dd = new sampling::StoUniformDistribution<>(rng, 0, params.key_sz - 1);
        else
            dd = new sampling::StoZipfDistribution<>(rng, 0, params.key_sz - 1, params.zipf_skew);

        auto ro_threshold = (uint32_t)(std::numeric_limits<uint32_t>::max() * params.readonly_percent);
        auto write_threshold = (uint32_t)(std::numeric_limits<uint32_t>::max() * params.write_percent);

        auto& thread_workload = this->workloads[thread_id];
        uint64_t trans_per_thread = params.ntrans / params.nthreads;

        for (uint64_t i = 0; i < trans_per_thread; ++i) {
            typename WG::query_type query;
            bool read_only = ud.sample() < ro_threshold;
            int nops = read_only ? params.opspertrans_ro : params.opspertrans;

            std::set<sampling::index_t> idx_set;
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

template <typename IntType>
struct MasstreeIntKey {
    explicit MasstreeIntKey(IntType k) : k_(bench::bswap(k)) {}

    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    IntType k_;
};

template <typename WLImpl, typename DBParams>
class MasstreeTester : public Tester<MasstreeTester<WLImpl, DBParams>, WLImpl> {
public:
    typedef Tester<MasstreeTester<WLImpl, DBParams>, WLImpl> Base;
    typedef MasstreeIntKey<sampling::index_t> key_type;
    typedef typename compute_value_type<WLImpl::RTParams::granules>::type value_type;
    typedef typename value_type::NamedColumn nc;
    typedef typename bench::ordered_index<key_type, value_type, DBParams> index_type;
    typedef typename bench::access_t access_t;

    explicit MasstreeTester(size_t num_threads) : Base(num_threads), mt_() {}

    void prepopulate_impl() {
        for (unsigned int i = 0; i < params.key_sz; ++i)
            mt_.nontrans_put(key_type(i), {i, i, i, i, i, i, i, i});
    }

    void thread_init_impl() {
        mt_.thread_init();
    }

    bool do_op_impl(const RWOperation& op) {
        //std::cout << op << std::endl;
        bool success;
        switch(op.type) {
            case OpType::read:
                std::tie(success, std::ignore, std::ignore, std::ignore)
                        = mt_.select_row(key_type(op.key),
                                         {{nc::f1, access_t::read}, {nc::f3, access_t::read}, {nc::f5, access_t::read}, {nc::f7, access_t::read}});
                break;
            case OpType::write: {
                auto v = Sto::tx_alloc<value_type>();
                *v = {op.value, op.value, op.value, op.value, op.value, op.value, op.value, op.value};
                std::tie(success, std::ignore) = mt_.insert_row(key_type(op.key), v, true);
                break;
            }
            case OpType::inc: {
                uintptr_t rid;
                const value_type* value;
                std::tie(success, std::ignore, rid, value)
                        = mt_.select_row(key_type(op.key),
                                         {{nc::f1, access_t::update}, {nc::f3, access_t::update}, {nc::f5, access_t::update}, {nc::f7, access_t::update}});
                if (!success)
                    break;
                value_type *new_v = Sto::tx_alloc(value);
                new_v->f1 += 1;
                new_v->f3 += 1;
                new_v->f5 += 1;
                new_v->f7 += 1;
                mt_.update_row(rid, new_v);
                break;
            }
        }
        return success;
    }

private:
    index_type mt_;
};

template <DsType DS, typename WLImpl, typename DBParams>
struct TesterSelector {};

template <typename WLImpl, typename DBParams>
struct TesterSelector<DsType::masstree, WLImpl, DBParams> {
    typedef MasstreeTester<WLImpl, DBParams> type;
};

template <int G, typename DBParams>
using MtZipfTesterDefault = TesterSelector<DsType::masstree, WLZipfRW<wl_default_params<G>>, DBParams>;

template <int G, typename DBParams>
using MtZipfTesterMeasure = TesterSelector<DsType::masstree, WLZipfRW<wl_measurement_params<G>>, DBParams>;

};

