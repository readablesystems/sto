#pragma once


#include "YCSB_bench.hh"

namespace ycsb {

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn_read_only() {

    TRANSACTION {

        bool abort, result;
        uintptr_t row;
        const void *value;

        for (int i = 0; i < 2; i++) {
            std::tie(abort, result, row, value) = db.ycsb_table().select_row(ycsb_key(dd->sample()), false);
            TXN_DO(abort);
            assert(result);     
        }

        (void)__txn_committed;

    // commit txn
    // retry until commits
    } RETRY(true);
}

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn_medium_contention() {

    TRANSACTION {

        bool abort, result;
        uintptr_t row;
        const void *value;

        for (int i = 0; i < 16; i++) {
            if (ud->sample() < write_threshold) {
                ycsb_key key(dd->sample());
                std::tie(abort, result, row, value) = db.ycsb_table().select_row(key, true);
                TXN_DO(abort);
                assert(result);
                db.ycsb_table().update_row(key, ycsb_value::random_ycsb_value());
            } else {
                std::tie(abort, result, row, value) = db.ycsb_table().select_row(ycsb_key(dd->sample()), false);
                TXN_DO(abort);
                assert(result);
            }
        }

    } RETRY(true);
}

template <typename DBParams>
void ycsb_runner<DBParams>::run_txn_high_contention() {
    TRANSACTION {
        
        bool abort, result;
        uintptr_t row;
        const void *value;

        for (int i = 0; i < 16; i++) {
            if (ud->sample() < write_threshold) {
                ycsb_key key(dd->sample());
                std::tie(abort, result, row, value) = db.ycsb_table().select_row(key, true);
                TXN_DO(abort);
                assert(result);
                db.ycsb_table().update_row(key, ycsb_value::random_ycsb_value());
            } else {
                std::tie(abort, result, row, value) = db.ycsb_table().select_row(ycsb_key(dd->sample()), false);
                TXN_DO(abort);
                assert(result);
            }
        }

    } RETRY(true);
}



};