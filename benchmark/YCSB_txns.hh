#pragma once


#include <set>
#include "YCSB_bench.hh"

namespace ycsb {

static constexpr uint64_t max_txns = 200000;

template <typename DBParams>
void ycsb_runner<DBParams>::gen_workload(int txn_size) {
    dist_init();
    for (uint64_t i = 0; i < max_txns; ++i) {
        ycsb_txn_t txn {};
        txn.ops.reserve(txn_size);
        std::set<uint32_t> key_set;
        for (int j = 0; j < txn_size; ++j) {
            uint32_t key;
            do {
                key = dd->sample();
            } while (key_set.find(key) != key_set.end());
            key_set.insert(key);
        }
        bool any_write = false;
        for (auto it = key_set.begin(); it != key_set.end(); ++it) {
            bool is_write = ud->sample() < write_threshold;
            ycsb_op_t op {};
            op.is_write = is_write;
            op.key = *it;
            op.col_n = ud->sample() % ycsb_value::num_cols; /*column number*/
            if (is_write) {
                any_write = true;
                ig.random_ycsb_col_value_inplace(&op.write_value);
            }
            txn.ops.push_back(std::move(op));
        }
        txn.rw_txn = any_write;
        workload.push_back(std::move(txn));
    }
}

using bench::RowAccess;

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn(const ycsb_txn_t& txn) {
    volatile ycsb_value::col_type output;
    typedef ycsb_value::NamedColumn nm;

    TRANSACTION {
        bool success, result;
        uintptr_t row;
        const void* value;
        if (DBParams::MVCC && txn.rw_txn) {
            Sto::mvcc_rw_upgrade();
        }
        for (auto& op : txn.ops) {
            bool col_parity = op.col_n % 2;
            auto col_group = col_parity ? nm::odd_columns : nm::even_columns;
            (void)col_group;
            if (op.is_write) {
                ycsb_key key(op.key);
#if TPCC_SPLIT_TABLE
                std::tie(success, result, row, value)
                    = db.ycsb_half_tables(col_parity).select_row(key,
                        Commute ? RowAccess::None : RowAccess::ObserveValue);
                TXN_DO(success);
                assert(result);

                if (Commute) {
                    commutators::Commutator<ycsb_half_value> comm(op.col_n/2, op.write_value);
                    db.ycsb_half_tables(col_parity).update_row(row, comm);
                } else {
                    auto old_val = reinterpret_cast<const ycsb_half_value*>(value);
                    auto new_val = Sto::tx_alloc(old_val);
                    new_val->cols[op.col_n/2] = op.write_value;
                    db.ycsb_half_tables(col_parity).update_row(row, new_val);
                }
#else
                std::tie(success, result, row, value)
                    = db.ycsb_table().select_row(key,
#if TABLE_FINE_GRINED
                    {{col_group, Commute ? access_t::write : access_t::update}}
#else
                    Commute ? RowAccess::None : RowAccess::ObserveValue
#endif
                );
                TXN_DO(success);
                assert(result);

                if (Commute) {
                    commutators::Commutator<ycsb_value> comm(op.col_n, op.write_value);
                    db.ycsb_table().update_row(row, comm);
                } else {
                    auto old_val = reinterpret_cast<const ycsb_value*>(value);
                    auto new_val = Sto::tx_alloc(old_val);
                    new_val->cols[op.col_n] = op.write_value;
                    db.ycsb_table().update_row(row, new_val);
                }
#endif
            } else {
                ycsb_key key(op.key);
#if TPCC_SPLIT_TABLE
                std::tie(success, result, row, value)
                    = db.ycsb_half_tables(col_parity).select_row(key,
                        RowAccess::ObserveValue);
                TXN_DO(success);
                assert(result);

                output = reinterpret_cast<const ycsb_half_value*>(value)->cols[op.col_n/2];
#else
                std::tie(success, result, row, value)
                    = db.ycsb_table().select_row(key,
#if TABLE_FINE_GRINED
                    {{col_group, access_t::read}}
#else
                    RowAccess::ObserveValue
#endif
                );
                TXN_DO(success);
                assert(result);

                output = reinterpret_cast<const ycsb_value*>(value)->cols[op.col_n];
#endif
                (void)output;
            }
        }
    } RETRY(true);
}

};
