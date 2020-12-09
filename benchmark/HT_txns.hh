#pragma once


#include <set>
#include "HT_bench.hh"

namespace htbench {

static constexpr uint64_t max_txns = 200000;

/*
template <typename DBParams>
void htbench_runner<DBParams>::gen_workload(int txn_size) {
    dist_init();
    for (uint64_t i = 0; i < max_txns; ++i) {
        htbench_txn_t txn {};
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
            ht_op_t op {};
            op.is_write = is_write;
            op.key = *it;
            if (is_write) {
                any_write = true;
                ig.random_ht_col_value_inplace(&op.value);
            }
            txn.ops.push_back(std::move(op));
        }
        txn.rw_txn = any_write;
        workload.push_back(std::move(txn));
    }
}
*/

template <typename DBParams>
void htbench_runner<DBParams>::gen_workload(uint64_t read_txn_size) {
    bool is_write = write_threshold > 0;
    const uint64_t txn_size = is_write ? 1 : read_txn_size;
    dist_init();
    for (uint64_t i = 0; i < max_txns; i += txn_size) {
        htbench_txn_t txn {};
        txn.ops.reserve(txn_size);
        for (uint64_t j = 0; j < txn_size; j++) {
            uint32_t key = dd->sample();
            bool any_write = is_write;
            ht_op_t op {};
            op.is_write = is_write;
            op.key = key;
            if (is_write) {
                any_write = true;
                ig.random_ht_col_value_inplace(&op.value);
            }
            txn.ops.push_back(std::move(op));
            txn.rw_txn |= any_write;
        }
        workload.push_back(std::move(txn));
    }
}

using bench::RowAccess;

template <typename DBParams>
typename htbench_runner<DBParams>::txn_type htbench_runner<DBParams>::run_txn(
        const htbench_txn_t& txn) {
    ht_value::col_type output;

    //uint64_t op_start_tick = 0;
    //uint64_t op_end_tick = 0;

    if (!nontrans || !txn.ops[0].is_write) {
        //uint64_t txn_start_tick = read_tsc();

        TRANSACTION {
            //bool success, result;
            bool failure, result;
            uintptr_t row;
            const void* value;
            if (DBParams::MVCC && txn.rw_txn) {
                Sto::mvcc_rw_upgrade();
            }
            for (auto& op : txn.ops) {
                //op_start_tick = read_tsc();
                if (op.is_write) {
                    ht_key key(op.key);
                    /*
                    std::tie(success, result, row, value)
                        = table.table().select_row(key,
                        Commute ? RowAccess::None : RowAccess::ObserveValue
                    );
                    */
                    std::tie(failure, result, row, value)
                        = table.table().transGet(
                                key, table_type::ReadOnlyAccess).to_tuple();

                    //op_end_tick = read_tsc();
                    //TSC_ACCOUNT(tc_txn_write, op_end_tick - op_start_tick);

                    //TXN_DO(success);
                    TXN_DO(!failure);
                    assert(result);

                    //op_start_tick = read_tsc();

                    if (Commute) {
                        /*
                        commutators::Commutator<ht_value> comm(op.value);
                        table.table().update_row(row, comm);
                        */
                    } else {
                        auto old_val = reinterpret_cast<const ht_value*>(value);
                        auto new_val = Sto::tx_alloc(old_val);
                        new_val->value = op.value;
                        //table.table().update_row(row, new_val);
                        table.table().transUpdate(row, new_val);
                    }

                    //op_end_tick = read_tsc();
                    //TSC_ACCOUNT(tc_txn_write, op_end_tick - op_start_tick);
                } else {
                    //auto key_start_t = read_tsc();
                    ht_key key(op.key);
                    //TSC_ACCOUNT(tc_keygen, read_tsc() - key_start_t);

                    //auto table_start_t = read_tsc();
                    auto& tbl = table.table();
                    //TSC_ACCOUNT(tc_table, read_tsc() - table_start_t);

                    //auto sel_start_t = read_tsc();
                    /*
                    auto res
                        = tbl.select_row(key,
                        RowAccess::ObserveValue
                    );
                    */
                    auto res = tbl.transGet(
                            key, table_type::ReadOnlyAccess).to_tuple();
                    //TSC_ACCOUNT(tc_select_call, read_tsc() - sel_start_t);

                    //auto untie_start_t = read_tsc();
                    //std::tie(success, result, row, value) = res;
                    std::tie(failure, result, row, value) = res;
                    //TSC_ACCOUNT(tc_untie, read_tsc() - untie_start_t);

                    //op_end_tick = read_tsc();
                    //TSC_ACCOUNT(tc_txn_read, op_end_tick - op_start_tick);

                    //TXN_DO(success);
                    TXN_DO(!failure);
                    assert(result);

                    //op_start_tick = read_tsc();

                    output = reinterpret_cast<const ht_value*>(value)->value;

                    //op_end_tick = read_tsc();
                    //TSC_ACCOUNT(tc_txn_read, op_end_tick - op_start_tick);
                }
            }
        } RETRY(true);
        //uint64_t txn_end_tick = read_tsc();
        //TSC_ACCOUNT(tc_txn_total, txn_end_tick - txn_start_tick);
    } else {
        //uint64_t txn_start_tick = read_tsc();
        //op_start_tick = read_tsc();

        ht_key key(txn.ops[0].key);
        auto* value_p = table.table().nontrans_get(key);
        if (value_p) {
            output = value_p->value;
        }

        //op_end_tick = read_tsc();
        //TSC_ACCOUNT(tc_txn_read, op_end_tick - op_start_tick);
        /*
        for (auto& op : txn.ops) {
            op_start_tick = read_tsc();
            if (op.is_write) {
                ht_key key(op.key);
                ht_value* value_p = table.table().nontrans_get(key);
                if (value_p) {
                    value_p->value = op.value;
                    table.table().nontrans_put(key, *value_p);
                } else {
                    ht_value value;
                    value.value = op.value;
                    table.table().nontrans_put(key, value);
                }

                op_end_tick = read_tsc();
                write_ticks += op_end_tick - op_start_tick;
            } else {
                ht_key key(op.key);
                auto* value_p = table.table().nontrans_get(key);
                if (value_p) {
                    output = value_p->value;
                }

                op_end_tick = read_tsc();
                read_ticks += op_end_tick - op_start_tick;
            }
        }
        */
        //uint64_t txn_end_tick = read_tsc();
        //TSC_ACCOUNT(tc_txn_total, txn_end_tick - txn_start_tick);
    }

    (void)output;
}

template <typename DBParams>
void htbench_barerunner<DBParams>::gen_workload(uint64_t read_txn_size) {
    bool is_write = write_threshold > 0;
    const uint64_t txn_size = is_write ? 1 : read_txn_size;
    dist_init();
    for (uint64_t i = 0; i < max_txns; i += txn_size) {
        htbench_txn_t txn {};
        txn.ops.reserve(txn_size);
        for (uint64_t j = 0; j < txn_size; j++) {
            uint32_t key = dd->sample();
            bool any_write = is_write;
            ht_op_t op {};
            op.is_write = is_write;
            op.key = key;
            if (is_write) {
                any_write = true;
                ig.random_ht_col_value_inplace(&op.value);
            }
            txn.ops.push_back(std::move(op));
            txn.rw_txn |= any_write;
        }
        workload.push_back(std::move(txn));
    }
}

using bench::RowAccess;

template <typename DBParams>
typename htbench_barerunner<DBParams>::txn_type
htbench_barerunner<DBParams>::run_txn(
        const htbench_txn_t&) {
    ht_value::col_type output;

    if (!nontrans) {
        TRANSACTION {
            //auto start_tick = read_tsc();
            /*
            bool success, result;
            uintptr_t row;
            const void* value;
            if (DBParams::MVCC && txn.rw_txn) {
                Sto::mvcc_rw_upgrade();
            }
            for (auto& op : txn.ops) {
                if (op.is_write) {
                    ht_key key(op.key);
                    std::tie(success, result, row, value)
                        = table.table().select_row(key,
                        Commute ? RowAccess::None : RowAccess::ObserveValue
                    );

                    TXN_DO(success);
                    assert(result);

                    if (Commute) {
                        commutators::Commutator<ht_value> comm(op.value);
                        table.table().update_row(row, comm);
                    } else {
                        auto old_val = reinterpret_cast<const ht_value*>(value);
                        auto new_val = Sto::tx_alloc(old_val);
                        new_val->value = op.value;
                        table.table().update_row(row, new_val);
                    }
                } else {
                    ht_key key(op.key);

                    auto& tbl = table.table();

                    auto res
                        = tbl.select_row(key,
                        RowAccess::ObserveValue
                    );

                    std::tie(success, result, row, value) = res;

                    TXN_DO(success);
                    assert(result);

                    output = reinterpret_cast<const ht_value*>(value)->value;
                }
            }
            */
            int foo = 0;
            (void) foo;
            TXN_DO(true);
            /*
            for (char foo = 0; foo >= 0; foo++) {
                (void) foo;
            }
            */
            //TSC_ACCOUNT(tc_txn_read, read_tsc() - start_tick);
        } RETRY(true);
    } else {
        do {
            TransactionLoopGuard __txn_guard;
            int foo = 0;
            (void) foo;
            /*
            for (char foo = 0; foo >= 0; foo++) {
                (void) foo;
            }
            */
        } while (false);
    }

    (void)output;
}

};
