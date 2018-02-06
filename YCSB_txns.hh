#pragma once


#include <set>
#include "YCSB_bench.hh"

namespace ycsb {

static constexpr uint64_t max_txns = 1000000;

template <typename DBParams>
void ycsb_runner<DBParams>::gen_workload(int txn_size) {
    dist_init();
    for (uint64_t i = 0; i < max_txns; ++i) {
        ycsb_txn_t txn;
        txn.reserve(txn_size);
        std::set<uint32_t> key_set;
        for (int j = 0; j < txn_size; ++j) {
            uint32_t key;
            do {
                key = dd->sample();
            } while (key_set.find(key) != key_set.end());
            key_set.insert(key);
        }
        for (auto it = key_set.begin(); it != key_set.end(); ++it) {
            bool is_write = ud->sample() < write_threshold;
            txn.emplace_back(is_write, *it, ud->sample() % 10 /*column number*/);
        }
        workload.push_back(std::move(txn));
    }
}

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn(const ycsb_txn_t& txn) {
    TRANSACTION {
        bool success;
        for (auto& op : txn) {
            if (op.is_write) {
                ycsb_key key(op.key);
                auto value = db.ycsb_table().nontrans_get(key);
                assert(value);

                auto new_col = Sto::tx_alloc<typename ycsb_value<DBParams>::col_type>();
                ig.random_ycsb_col_value_inplace(new_col);
                success = value->trans_col_update(op.col_n, *new_col);
                TXN_DO(success);
            } else {
                auto value = db.ycsb_table().nontrans_get(ycsb_key(op.key));
                assert(value);

                std::tie(success, std::ignore) = value->trans_col_read(op.col_n);
                TXN_DO(success);
            }
        }
        (void)__txn_committed;
    } RETRY(true);
}

};
