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

enum Txns {Deposit = 1, View = 0, PayAll = 3, Transfer = 2};
std::atomic<uint64_t> total_txns;

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
        oi.insert_row(oi_key(key), new oi_value{val});
    }

    uintptr_t lookup(uint64_t key) override {
        uintptr_t ret;
        const oi_value* val;
        std::tie(std::ignore, std::ignore, std::ignore, val) = oi.select_row(oi_key(key), bench::RowAccess::None);
        if (!val) {
            ret = 0;
        } else {
            ret = val->val;
        }
        return ret;
    }

    void update(uint64_t key, uintptr_t val) override {
        const oi_value* found;
        std::tie(std::ignore, std::ignore, std::ignore, found) = oi.select_row(oi_key(key), bench::RowAccess::UpdateValue);
        oi.update_row(reinterpret_cast<uintptr_t>(found), new oi_value{val});
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

void bank_bench(int nthread, int npeople) {
    keyval_db* db = new tart_wrapper();
    uint64_t* people = new uint64_t[npeople];

    unsigned int seed = (unsigned int) time(NULL);
    RandomSequenceOfUnique rsu(seed, seed + 1);

    for (int i = 0; i < npeople; i++) {
        people[i] = rsu.next();
        TRANSACTION_E{
            db->insert(people[i], 100000);
        } RETRY_E(true);
    }

    printf("Setup complete\n");

    std::thread threads[nthread];
    for (int i = 0; i < nthread; i++) {
        threads[i] = std::thread([people, npeople, db](int thread_id) {
            oindex_wrapper::index_type::thread_init();
            std::mt19937 rng;
            rng.seed(std::random_device()());
            std::uniform_int_distribution<std::mt19937::result_type> dist(0, 2);

            std::mt19937 acnt_rng;
            acnt_rng.seed(std::random_device()());
            std::uniform_int_distribution<std::mt19937::result_type> acnt_dist(0, npeople-1);

            size_t txns = 0;
            TThread::set_id(thread_id);
            time_t endwait;
            time_t start = time(NULL);
            time_t seconds = 20; // end loop after this time has elapsed

            endwait = start + seconds;
            while (start < endwait) {
                int account = people[acnt_dist(acnt_rng)];
                Txns op = static_cast<Txns>(dist(rng));
                // Txns op = static_cast<Txns>(0);
                if (op == View) {
                    TRANSACTION_E {
                        uintptr_t balance = db->lookup(account);
                    } RETRY_E(true);
                } else if (op == Deposit) {
                    TRANSACTION_E {
                        uintptr_t balance = db->lookup(account);
                        balance += 1;
                        db->insert(account, balance);
                    } RETRY_E(true);
                } else if (op == Transfer) {
                    int account2 = people[acnt_dist(acnt_rng)];
                    TRANSACTION_E {
                        uintptr_t balance1 = db->lookup(account);
                        uintptr_t balance2 = db->lookup(account2);

                        balance2 += 1;
                        balance1 -= 1;

                        db->insert(account, balance1);
                        db->insert(account2, balance2);
                    } RETRY_E(true);
                } else if (op == PayAll) {
                    TRANSACTION_E {
                        for (int i = 0; i < npeople; i++) {
                            uintptr_t balance = db->lookup(account);
                            balance += 1;
                            db->insert(people[i], balance);
                        }
                    } RETRY_E(true);
                } else {
                    printf("invalid op\n");
                    continue;
                }
                txns++;
                start = time(NULL);
            }
            total_txns += txns;
        }, i);
    }

    for (int i = 0; i < nthread; i++) {
        threads[i].join();
    }
    size_t txns = total_txns.load();
    printf("Transactions: %d\n", txns);
}

int main(int argc, char *argv[]) {
    int nthread = 1;
    int npeople = 10000000;
    if (argc > 1) {
        nthread = atoi(argv[1]);
    }
    if (argc > 2) {
        npeople = atoi(argv[2]);
    }
    bank_bench(nthread, npeople);
}
