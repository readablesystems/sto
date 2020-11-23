#pragma once

#include <iostream>
#include <string>

#include "compiler.hh"
#include "clp.h"
#include "sampling.hh"
#include "SystemProfiler.hh"
#include "PRCU_structs.hh"
#include "PRCU_commutators.hh"
#if TABLE_FINE_GRAINED
#include "PRCU_selectors.hh"
#endif
#include "DB_index.hh"
#include "DB_params.hh"
#include "Hashtable.hh"

namespace prcubench {

using bench::mvcc_ordered_index;
using bench::ordered_index;
using bench::mvcc_unordered_index;
using bench::unordered_index;

static constexpr uint64_t ht_table_size = 10000000;

template <typename DBParams>
class ht_table {
public:
    template <typename K, typename V>
    using OIndex = typename std::conditional<DBParams::MVCC,
          mvcc_ordered_index<K, V, DBParams>,
          ordered_index<K, V, DBParams>>::type;
    template <typename K, typename V>
    using UIndex = typename std::conditional<DBParams::MVCC,
        mvcc_unordered_index<K, V, DBParams>,
        unordered_index<K, V, DBParams>>::type;

    //typedef UIndex<ht_key, ht_value> ht_table_type;
    typedef std::conditional_t<
        DBParams::MVCC,
        Hashtable_mvcc_params<ht_key, ht_value>,
        std::conditional_t<
            DBParams::Opaque,
            Hashtable_opaque_params<ht_key, ht_value>,
            Hashtable_params<ht_key, ht_value>>
            > ht_params_type;
    typedef Hashtable<ht_params_type> ht_table_type;
    static constexpr auto BlindAccess = ht_table_type::BlindAccess;
    static constexpr auto ReadOnlyAccess = ht_table_type::ReadOnlyAccess;
    static constexpr auto ReadWriteAccess = ht_table_type::ReadWriteAccess;

    explicit ht_table() : ht_table_(ht_table_size) {}

    ht_table_type& table() {
        return ht_table_;
    }

    void table_thread_init() {
        //ht_table_.thread_init();
    }

    void prepopulate();

private:
    ht_table_type ht_table_;
};

struct ht_op_t {
    ht_op_t() : is_write(), key() {}
    ht_op_t(const bool w, const uint32_t& k)
            : is_write(w), key(k) {}
    bool is_write;
    uint32_t key;
    prcubench::ht_value::col_type value;
};

struct prcubench_txn_t {
    prcubench_txn_t() : rw_txn(false), ops() {}

    bool rw_txn;
    std::vector<ht_op_t> ops;
};

template <typename DBParams>
class prcubench_runner {
public:
    //typedef std::tuple<uint64_t, uint64_t, uint64_t> txn_type;
    typedef void txn_type;

    static constexpr bool Commute = DBParams::Commute;
    prcubench_runner(int tid, ht_table<DBParams>& table, mode_id mid, uint32_t write_threshold, const bool nontrans)
        : total_sum(0),
          table(table), ig(tid), runner_id(tid), mode(mid),
          ud(), dd(), write_threshold(write_threshold), nontrans(nontrans) {}

    inline void dist_init() {
        ud = new sampling::StoUniformDistribution<>(ig.generator(), 0, std::numeric_limits<uint32_t>::max());
        switch(mode) {
            case mode_id::Uniform:
                dd = new sampling::StoUniformDistribution<>(ig.generator(), 0, ht_table_size - 1);
                break;
            case mode_id::Skewed:
                dd = new sampling::StoZipfDistribution<>(ig.generator(), 0, ht_table_size - 1, 1.03);
                break;
            default:
                break;
        }
    }

    //inline void gen_workload(int txn_size);
    inline void gen_workload(uint64_t read_txn_size);

    int id() const {
        return runner_id;
    }

    inline txn_type run_txn(const prcubench_txn_t& txn);

    std::vector<prcubench_txn_t> workload;

    uint64_t total_sum;

protected:
    typedef ht_table<DBParams> table_type;
    table_type table;
    ht_input_generator ig;
    int runner_id;
    mode_id mode;

    sampling::StoUniformDistribution<> *ud;
    sampling::StoRandomDistribution<> *dd;

    uint32_t write_threshold;
    bool nontrans;
};

template <typename DBParams>
class prcubench_barerunner : public prcubench_runner<DBParams> {
public:
    using base = prcubench_runner<DBParams>;
    using base::Commute;
    typedef typename base::txn_type txn_type;

    prcubench_barerunner(int tid, ht_table<DBParams>& table, mode_id mid, uint32_t write_threshold, const bool nontrans) : base(tid, table, mid, write_threshold, nontrans) {}

    using base::dist_init;
    using base::id;
    using base::workload;

    inline void gen_workload(uint64_t read_txn_size);
    inline txn_type run_txn(const prcubench_txn_t& txn);

    using base::total_sum;

protected:
    using base::table;
    using base::ig;
    using base::runner_id;
    using base::mode;
    using base::ud;
    using base::dd;
    using base::write_threshold;
    using base::nontrans;
};

}; // namespace prcu_bench
