#pragma once


#include <set>
#include "PRCU_bench.hh"

namespace prcubench {

static constexpr uint64_t max_txns = 200000;

template <typename DBParams>
void prcubench_runner<DBParams>::gen_workload(uint64_t) {
    bool is_write = write_threshold > 0;
    dist_init();
    const auto long_txn = 64;
    const auto cycle = dd->sample() % long_txn;
    for (uint64_t i = 0; i < long_txn; i++) {
        prcubench_txn_t txn {};
        const bool is_long_txn = (i % long_txn == cycle);
        const uint64_t txn_size = is_long_txn ? long_txn : 1;
        txn.ops.reserve(txn_size);
        auto first_key = dd->sample() % 1024;
        for (uint64_t j = 0; j < txn_size; j++) {
            uint32_t key = first_key + j;
            bool any_write = is_write;
            ht_op_t op {};
            op.is_write = is_write;
            op.key = key;
            if (is_write) {
                any_write = true;
                op.value = 1;
                //ig.random_ht_col_value_inplace(&op.value);
            }
            txn.ops.push_back(std::move(op));
            txn.rw_txn |= any_write;
        }
        workload.push_back(std::move(txn));
    }
}

using bench::RowAccess;

template <typename DBParams>
typename prcubench_runner<DBParams>::txn_type prcubench_runner<DBParams>::run_txn(
        const prcubench_txn_t& txn) {
    ht_value::col_type output;

    if (nontrans) {
        ht_key key(txn.ops[0].key);
        auto* value_p = table.table().nontrans_get(key);
        if (value_p) {
            output = value_p->value;
        }
        return;
    }
    if (txn.rw_txn) {
        uint64_t txn_sum = 0;
        size_t attempts = 0;
        RWTRANSACTION {
            ++attempts;
            if (attempts > 1) {
                //printf("Attempts: %zu\n", attempts);
            }
            bool failure, result;
            uintptr_t row;
            const void* value;
            txn_sum = 0;
            if (DBParams::MVCC && txn.rw_txn) {
                Sto::mvcc_rw_upgrade();
            }
            for (auto& op : txn.ops) {
                if (op.is_write) {
                    ht_key key(op.key);
                    std::tie(failure, result, row, value)
                        = table.table().transGet(
                                key, table_type::ReadWriteAccess).to_tuple();

                    TXN_DO(!failure);

                    assert(result);

                    txn_sum += op.value;
                    TXP_ACCOUNT(txp_total_sum, op.value);
                    if constexpr (Commute) {
                        commutators::Commutator<ht_value> comm(op.value);
                        table.table().transUpdate(row, comm);
                    } else {
                        auto old_val = reinterpret_cast<const ht_value*>(value);
                        auto new_val = Sto::tx_alloc(old_val);
                        new_val->value += op.value;
                        table.table().transUpdate(row, new_val);
                    }
                } else {
                    ht_key key(op.key);

                    auto& tbl = table.table();

                    auto res = tbl.transGet(
                            key, table_type::ReadOnlyAccess).to_tuple();

                    std::tie(failure, result, row, value) = res;

                    TXN_DO(!failure);
                    assert(result);

                    output = reinterpret_cast<const ht_value*>(value)->value;
                }
            }
        } RETRY(true);
        total_sum += txn_sum;
        TXP_ACCOUNT(txp_mvcc_sum, txn.ops.size());
        if (attempts > 1) {
            //printf("%d attempts\n", attempts);
        }
    }
    (void)output;
}

template <typename DBParams>
void prcubench_barerunner<DBParams>::gen_workload(uint64_t read_txn_size) {
    bool is_write = write_threshold > 0;
    const uint64_t txn_size = is_write ? 1 : read_txn_size;
    dist_init();
    for (uint64_t i = 0; i < max_txns; i += txn_size) {
        prcubench_txn_t txn {};
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
typename prcubench_barerunner<DBParams>::txn_type
prcubench_barerunner<DBParams>::run_txn(
        const prcubench_txn_t&) {
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
