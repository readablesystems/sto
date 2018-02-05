#pragma once


#include "YCSB_bench.hh"

namespace ycsb {

static constexpr uint64_t max_txns = 1000000;

template <typename DBParams>
void ycsb_runner<DBParams>::gen_workload(int txn_size) {
    dist_init();
    for (uint64_t i = 0; i < max_txns; ++i) {
        ycsb_txn_t txn(txn_size);
        for (int j = 0; j < txn_size; ++j) {
            uint32_t key = dd->sample();
            bool is_write = ud->sample() < write_threshold;
            txn.emplace_back(is_write, key);
        }
        workload.emplace_back(std::move(txn));
    }
}

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn(const ycsb_txn_t& txn) {
    TRANSACTION {
        bool abort, result;
        uintptr_t row;
        const void *value;
        for (auto& op : txn) {
            if (op.first) {
                ycsb_key key(op.second);
                std::tie(abort, result, row, value) = db.ycsb_table().select_row(key, true);
                TXN_DO(abort);
                assert(result);
                auto new_val = ig.random_ycsb_value();
                db.ycsb_table().update_row(row, &new_val);
            } else {
                std::tie(abort, result, row, value) = db.ycsb_table().select_row(ycsb_key(op.second), false);
                TXN_DO(abort);
                assert(result);
            }
        }
    (void)__txn_committed;
    } RETRY(true);
}

};
