#undef NDEBUG
#include <fstream>
#include <string>
#include <thread>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TART.hh"
#include "Transaction.hh"
#include <unistd.h>
#include <random>
#include <time.h>
#include "DB_index.hh"
#include "DB_params.hh"

enum Txns {Deposit = 1, View = 0, PayMulti = 3, Transfer = 2};
std::atomic<uint64_t> total_txns;

std::atomic<uint64_t> total_views;
std::atomic<uint64_t> total_deposits;
std::atomic<uint64_t> total_multis;
std::atomic<uint64_t> total_transfers;

class RandomSequenceOfUnique
{
private:
    unsigned int m_index;
    unsigned int m_intermediateOffset;

    static unsigned int permuteQPR(unsigned int x)
    {
        static const unsigned int prime = 4294967291u;
        if (x >= prime)
            return x;  // The 5 integers out of range are mapped to themselves.
        unsigned int residue = ((unsigned long long) x * x) % prime;
        return (x <= prime / 2) ? residue : prime - residue;
    }

public:
    RandomSequenceOfUnique(unsigned int seedBase, unsigned int seedOffset)
    {
        m_index = permuteQPR(permuteQPR(seedBase) + 0x682f0161);
        m_intermediateOffset = permuteQPR(permuteQPR(seedOffset) + 0x46790905);
    }

    unsigned int next()
    {
        return permuteQPR((permuteQPR(m_index++) + m_intermediateOffset) ^ 0x5bf03635);
    }
};


class keyval_db {
public:
    virtual void insert(uint64_t int_key, uintptr_t val) = 0;
    virtual void update(uint64_t int_key, uintptr_t val) = 0;
    virtual uintptr_t lookup(uint64_t int_key) = 0;
    virtual void erase(uint64_t int_key) = 0;
};

class oindex_wrapper : public keyval_db {
public:
    struct oi_value {
        enum class NamedColumn : int { val = 0 };
        uintptr_t val;
    };

    struct oi_key {
        uint64_t key;
        oi_key(uint64_t k) {
            key = k;
        }
        bool operator==(const oi_key& other) const {
            return (key == other.key);
        }
        bool operator!=(const oi_key& other) const {
            return !(*this == other);
        }
        operator lcdf::Str() const {
            return lcdf::Str((const char *)this, sizeof(*this));
        }
    };

    typedef bench::ordered_index<oi_key, oi_value, db_params::db_default_params> index_type;
    index_type oi;

    oindex_wrapper() {
    }

    void insert(uint64_t key, uintptr_t val) override {
        bool success;
        std::tie(success, std::ignore) = oi.insert_row(oi_key(key), new oi_value{val});
        if (!success) throw Transaction::Abort();
    }

    uintptr_t lookup(uint64_t key) override {
        uintptr_t ret;
        bool success;
        const oi_value* val;
        std::tie(success, std::ignore, std::ignore, val) = oi.select_row(oi_key(key), bench::RowAccess::None);
        if (!success) throw Transaction::Abort();
        if (!val) {
            ret = 0;
        } else {
            ret = val->val;
        }
        return ret;
    }

    void update(uint64_t key, uintptr_t val) override {
        bool success;
        uintptr_t row;
        const oi_value* value;
        std::tie(success, std::ignore, row, value) = oi.select_row(oi_key(key), bench::RowAccess::UpdateValue);
        if (!success) throw Transaction::Abort();
        auto new_oiv = Sto::tx_alloc<oi_value>(value);
        new_oiv->val = val;
        oi.update_row(row, new_oiv);
    }

    void erase(uint64_t key) override {
        oi.delete_row(oi_key(key));
    }
};

class tart_wrapper : public keyval_db {
public:
    TART* t;

    tart_wrapper() {
        t = new TART();
    }

    void insert(uint64_t int_key, uintptr_t val) override {
        t->transPut(int_key, val);
    }

    void update(uint64_t int_key, uintptr_t val) override {
        t->transPut(int_key, val);
    }

    uintptr_t lookup(uint64_t int_key) override {
        int ret;
        ret = t->transGet(int_key);
        return ret;
    }

    void erase(uint64_t int_key) override {
        t->transRemove(int_key);
    }
};

void bank_bench(int nthread, int npeople, int art, unsigned int seed) {
    keyval_db* db;
    if (art) {
        db = new tart_wrapper();
    } else {
        db = new oindex_wrapper();
    }
    uint64_t* people = new uint64_t[npeople];

    printf("Setup seed: %d\n", seed);
    RandomSequenceOfUnique rsu(seed, seed + 1);

    for (int i = 0; i < npeople; i++) {
        people[i] = rsu.next();
        TRANSACTION_E {
            db->insert(people[i], 100000);
        } RETRY_E(false);
    }

    printf("Setup complete\n");

    std::thread threads[nthread];
    auto starttime = std::chrono::system_clock::now();
    for (int i = 0; i < nthread; i++) {
        threads[i] = std::thread([people, npeople, db](int thread_id) {
            oindex_wrapper::index_type::thread_init();
            std::mt19937 rng;
            rng.seed(thread_id);
            std::uniform_int_distribution<std::mt19937::result_type> dist(0, 2);

            std::mt19937 acnt_rng;
            acnt_rng.seed(thread_id+100);
            std::uniform_int_distribution<std::mt19937::result_type> acnt_dist(0, npeople-1);

            uint64_t txns = 0;

            uint64_t views = 0;
            uint64_t deposits = 0;
            uint64_t transfers = 0;
            uint64_t multis = 0;
            TThread::set_id(thread_id);
            time_t seconds = 20*1000000; // end loop after this time has elapsed
            auto starttime = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::system_clock::now() - starttime);

            while (duration.count() < seconds) {
                uint64_t account = people[acnt_dist(acnt_rng)];
                Txns op = static_cast<Txns>(dist(rng));
                // Txns op = static_cast<Txns>(1);
                if (op == View) {
                    TRANSACTION_E {
                        db->lookup(account);
                    } RETRY_E(true);
                    views++;
                } else if (op == Deposit) {
                    TRANSACTION_E {
                        uintptr_t balance = db->lookup(account);
                        balance += 1;
                        db->update(account, balance);
                    } RETRY_E(true);
                    deposits++;
                } else if (op == Transfer) {
                    uint64_t account2 = people[acnt_dist(acnt_rng)];
                    TRANSACTION_E {
                        uintptr_t balance1 = db->lookup(account);
                        uintptr_t balance2 = db->lookup(account2);

                        balance2 += 1;
                        balance1 -= 1;

                        db->update(account, balance1);
                        db->update(account2, balance2);
                    } RETRY_E(true);
                    transfers++;
                } else if (op == PayMulti) {
                    TRANSACTION_E {
                        for (int i = 0; i < 100; i++) {
                            uintptr_t balance = db->lookup(account);
                            balance += 1;
                            db->insert(people[i], balance);
                        }
                    } RETRY_E(true);
                    multis++;
                } else {
                    printf("invalid op\n");
                    continue;
                }
                txns++;
                duration = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::system_clock::now() - starttime);
            }
            total_txns += txns;

            total_views += views;
            total_deposits += deposits;
            total_transfers += transfers;
            total_multis += multis;
        }, i);
    }

    for (int i = 0; i < nthread; i++) {
        threads[i].join();
    }
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - starttime);
    uint64_t txns = total_txns.load();
    printf("Total time: %f\n", duration.count() / 1000000.0);
    printf("Transactions completed: %lu\n", txns);
    printf("Throughput (txns/sec): %f\n", (double) txns / (duration.count() / 1000000.0));

    txp_counters tc = Transaction::txp_counters_combined();
    printf("Aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    printf("Views: %f, Deposits: %f, Transfers: %f, Multis: %f\n", total_views.load() / (double) txns, total_deposits.load() / (double) txns, total_transfers.load() / (double) txns, total_multis.load() / (double) txns);
}

int main(int argc, char *argv[]) {
    int nthread = 1;
    int npeople = 10000000;
    int art = 1;
    unsigned int seed = (unsigned int) time(NULL);
    if (argc > 1) {
        nthread = atoi(argv[1]);
    }
    if (argc > 2) {
        npeople = atoi(argv[2]);
    }
    if (argc > 3) {
        art = atoi(argv[3]);
    }
    if (argc > 4) {
        seed = atoi(argv[4]);
    }
    bank_bench(nthread, npeople, art, seed);
}
