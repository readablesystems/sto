#define LISTBENCH

#include <iostream>
#include <vector>
#include <random>
#include "listbench.hh"
#include "ListS.hh"

#define MAX_ELEMENTS 65532
using list_type = List<int, int>;
uint64_t bm_ctrs[2];
std::vector<TransactionTid::type> snapshot_ids;

void random_populate(list_type* l, size_t ntxn, size_t max_txn_len) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_ELEMENTS);
    for (size_t i = 0; i < ntxn; ++i) {
        size_t txnlen = ((size_t)dis(gen)) % max_txn_len + 1;
        TRANSACTION {
            for (size_t j = 0; j < txnlen; ++j) {
                (*l)[dis(gen)] = dis(gen);
            }
        } RETRY(false);
    }
}

void random_search(list_type* l, size_t ntxn, size_t max_txn_len) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_ELEMENTS);

    for (size_t i = 0; i < ntxn; ++i) {
        size_t txnlen = ((size_t)dis(gen)) % max_txn_len + 1;
        TRANSACTION {
            for (size_t j = 0; j < txnlen; ++j) {
                int v = (*l)[dis(gen)];
                assert(v <= MAX_ELEMENTS);
            }
        } RETRY(false);
    }
}

void random_populate_snapshot(list_type* l, size_t ntxn, size_t max_txn_len, int snap_factor) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_ELEMENTS);
    int snapshot_threshold = MAX_ELEMENTS / snap_factor;
    for (size_t i = 0; i < ntxn; ++i) {
        size_t txnlen = ((size_t)dis(gen)) % max_txn_len + 1;
        TRANSACTION {
            for (size_t j = 0; j < txnlen; ++j)
                (*l)[dis(gen)] = dis(gen);
        } RETRY(false);

        if (snapshot_ids.empty() || dis(gen) <= snapshot_threshold) {
            snapshot_ids.push_back(Sto::take_snapshot());
        }
    }
}

void random_search_snapshot(list_type* l, size_t ntxn, size_t max_txn_len) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, MAX_ELEMENTS);

    TransactionTid::type sid = snapshot_ids.at(snapshot_ids.size()/2);
    for (size_t i = 0; i < ntxn; ++i) {
        size_t txnlen = ((size_t)dis(gen)) % max_txn_len + 1;
        TRANSACTION {
            Sto::set_active_sid(sid);
            for (size_t j = 0; j < txnlen; ++j) {
                int v = (*l)[dis(gen)];
                assert(v <= MAX_ELEMENTS);
            }
        } RETRY(false);
    }
}
