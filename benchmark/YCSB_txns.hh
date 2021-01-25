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
            op.col_n = ud->sample() % (2*HALF_NUM_COLUMNS); /*column number*/
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

using bench::access_t;

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn(const ycsb_txn_t& txn) {
    col_type output;
    typedef ycsb_value::NamedColumn nm;

    (void)output;

    TRANSACTION {
        if (DBParams::MVCC && txn.rw_txn) {
            Sto::mvcc_rw_upgrade();
        }
        ADAPTER_OF(ycsb_value)::Commit();
        ADAPTER_OF(ycsb_value)::ResetThread();
        for (auto& op : txn.ops) {
            bool col_parity = op.col_n % 2;
            auto col_group = col_parity ? nm::odd_columns : nm::even_columns;
            (void)col_group;
            if (op.is_write) {
                ycsb_key key(op.key);
                auto [success, result, row, value]
                    = db.ycsb_table().select_split_row(key,
                    {{col_group, Commute ? access_t::write : access_t::update}}
                );
                (void)result;
                TXN_DO(success);
                assert(result);

                if constexpr (false && Commute) {
                    commutators::Commutator<ycsb_value> comm(op.col_n, op.write_value);
                    db.ycsb_table().update_row(row, comm);
#if TABLE_FINE_GRAINED
                } else if (DBParams::MVCC) {
                    // MVCC loop also does a tx_alloc, so we don't need to do
                    // one here
                    ycsb_value new_val_base;
                    ycsb_value* new_val = &new_val_base;
                    if (col_parity) {
                        new_val->odd_columns = value.odd_columns();
                        new_val->odd_columns[op.col_n/2] = op.write_value;
                    } else {
                        new_val->even_columns = value.even_columns();
                        new_val->even_columns[op.col_n/2] = op.write_value;
                    }
                    db.ycsb_table().update_row(row, new_val);
#endif
                } else {
                    auto new_val = Sto::tx_alloc<ycsb_value>();
                    if (col_parity) {
                        new_val->odd_columns() = value.odd_columns();
                        new_val->odd_columns()[op.col_n/2] = op.write_value;
                        ADAPTER_OF(ycsb_value)::CountWrite(op.col_n);
                    } else {
                        new_val->even_columns() = value.even_columns();
                        new_val->even_columns()[op.col_n/2] = op.write_value;
                        ADAPTER_OF(ycsb_value)::CountWrite(op.col_n);
                    }
                    db.ycsb_table().update_row(row, new_val);
                }
            } else {
                ycsb_key key(op.key);
                auto [success, result, row, value]
                    = db.ycsb_table().select_split_row(key, {{col_group, access_t::read}});
                (void)result; (void)row;
                TXN_DO(success);
                assert(result);

                if (col_parity) {
                    output = value.odd_columns()[op.col_n/2];
                } else {
                    output = value.even_columns()[op.col_n/2];
                }
            }
        }
    } RETRY(true);
    ADAPTER_OF(ycsb_value)::ResetThread();
}

};
