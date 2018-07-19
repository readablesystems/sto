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

class ordered_index {
public:
    virtual void insert(uint64_t int_key, uintptr_t val) = 0;
    virtual uintptr_t lookup(uint64_t int_key) = 0;
    virtual void erase(uint64_t int_key) = 0;
};

class tart_wrapper : public ordered_index {
public:
    TART* t;

    tart_wrapper() {
        t = new TART();
    }

    void insert(uint64_t int_key, uintptr_t val) override {
        TRANSACTION_E {
            t->insert(int_key, val);
        } RETRY_E(true);
    }

    uintptr_t lookup(uint64_t int_key) override {
        int ret;
        TRANSACTION_E {
            ret = t->lookup(int_key);
        } RETRY_E(true);
        return ret;
    }

    void erase(uint64_t int_key) override {
        TRANSACTION_E {
            t->erase(int_key);
        } RETRY_E(true);
    }
};

class art_wrapper : public ordered_index {
public:
    ART_OLC::Tree t;

    struct Element {
        const char* key;
        int len;
        uintptr_t val;
    };

    static void loadKey(TID tid, Key &key) {
        Element* e = (Element*) tid;
        key.setKeyLen(e->len);
        if (e->len > 8) {
            key.set(e->key, e->len);
        } else {
            memcpy(&key[0], &e->key, e->len);
        }
    }

    static void make_key(const char* tkey, Key &key, int len) {
        key.setKeyLen(len);
        if (len > 8) {
            key.set(tkey, len);
        } else {
            memcpy(&key[0], tkey, len);
        }
    }

    art_wrapper() {
        t.setLoadKey(art_wrapper::loadKey);
    }

    void insert(uint64_t int_key, uintptr_t val) override {
        Key art_key;
        make_key((const char*) &int_key, art_key, 8);

        t.insert(art_key, [int_key, val] {
            Element* e = new Element();
            e->key = (const char*) int_key;
            e->len = 8;
            e->val = val;
            return (TID) e;
        });
    }

    uintptr_t lookup(uint64_t int_key) override {
        Key art_key;
        make_key((const char*) &int_key, art_key, 8);
        auto r = t.lookup(art_key);
        Element* e = (Element*) r.first;
        if (e) {
            return e->val;
        }
        return 0;
    }

    void erase(uint64_t int_key) override {
        Key art_key;
        make_key((const char*) &int_key, art_key, 8);
        auto r = t.lookup(art_key);
        if (!r.first) {
            return;
        }
        t.remove(art_key, (TID) r.first);
    }
};

class masstree_wrapper : public ordered_index {
public:
};

void bench1(int nthread, int rand_keys, int nvals) {
    ordered_index* art = new tart_wrapper();
    uint64_t* keys = new uint64_t[nvals];

    std::mt19937 rng;
    rng.seed(std::random_device()());
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, (size_t) -1);

    for (int i = 0; i < nvals; i++) {
        if (rand_keys) {
            keys[i] = dist(rng);
        } else {
            keys[i] = i;
        }
    }

    // Build tree
    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[nthread];
        for (int i = 0; i < nthread; i++) {
            threads[i] = std::thread([&](int thread_id) {
                TThread::set_id(thread_id);

                for (int i = thread_id*(nvals/nthread); i < (thread_id+1)*nvals/nthread; i++) {
                    art->insert(keys[i], keys[i]);
                }
            }, i);
        }

        for (int i = 0; i < nthread; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("insert,%d,%f\n", nvals, (nvals * 1.0) / duration.count());
    }

    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[nthread];
        for (int i = 0; i < nthread; i++) {
            threads[i] = std::thread([&](int thread_id) {
                TThread::set_id(thread_id);

                for (int i = thread_id*(nvals/nthread); i < (thread_id+1)*nvals/nthread; i++) {
                    auto val = art->lookup(keys[i]);
                    assert(val == keys[i]);
                }
            }, i);
        }

        for (int i = 0; i < nthread; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("lookup,%d,%f\n", nvals, (nvals * 1.0) / duration.count());
    }

    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[nthread];
        for (int i = 0; i < nthread; i++) {
            threads[i] = std::thread([&](int thread_id) {
                TThread::set_id(thread_id);

                for (int i = thread_id*(nvals/nthread); i < (thread_id+1)*nvals/nthread; i++) {
                    art->erase(keys[i]);
                }
            }, i);
        }

        for (int i = 0; i < nthread; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("erase,%d,%f\n", nvals, (nvals * 1.0) / duration.count());
    }
}

void bench2(int nthread, int nvals) {

    ordered_index* art = new art_wrapper();
    uint64_t* keys = new uint64_t[nvals];

    std::mt19937 rng;
    rng.seed(std::random_device()());
    // std::uniform_int_distribution<std::mt19937::result_type> dist(0, (size_t) -1);
    std::normal_distribution<double> dist(nvals/2,nvals/4);

    for (int i = 0; i < nvals; i++) {
        keys[i] = dist(rng);
        art->insert(i, keys[i]);
    }
    printf("Prepopulation complete\n");

    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[nthread];
        for (int i = 0; i < nthread; i++) {
            threads[i] = std::thread([&](int thread_id) {
                TThread::set_id(thread_id);

                for (int i = thread_id*(nvals/nthread); i < (thread_id+1)*nvals/nthread; i++) {
                    uint64_t k = dist(rng);
                    auto val = art->lookup(k);
                    if (k >= (uint64_t) nvals) {
                        assert(val == 0);
                    } else {
                        assert(val == keys[k]);
                    }
                }
            }, i);
        }

        for (int i = 0; i < nthread; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("lookup,%d,%f\n", nvals, (nvals * 1.0) / duration.count());
    }
}

int main(int argc, char *argv[]) {
    int nthread = 1;
    int rand_keys = 0;
    int nvals = 10000;
    if (argc > 1) {
        nthread = atoi(argv[1]);
    }
    if (argc > 2) {
        nvals = atoi(argv[2]);
    }
    if (argc > 3) {
        rand_keys = atoi(argv[3]);
    }
    bench1(nthread, rand_keys, nvals);
    // bench2(nthread, nvals, 1);
}