#include <iostream>
#include <random>
#include <vector>
#include <thread>
#include <byteswap.h>

#include <pthread.h>

#include "config.h"
#include "compiler.hh"
#include "sampling.hh"

#include "masstree.hh"
#include "kvthread.hh"
#include "masstree_tcursor.hh"
#include "masstree_insert.hh"
#include "masstree_print.hh"
#include "masstree_remove.hh"
#include "masstree_scan.hh"
#include "string.hh"

#define NUM_THREADS 64

using sampling::StoRandomDistribution;
using sampling::StoZipfDistribution;

struct MultiPartKey {
    uint64_t key_part_0;
    uint64_t key_part_1;
    uint64_t key_part_2;

    MultiPartKey(uint64_t k0, uint64_t k1, uint64_t k2)
        : key_part_0(__bswap_64(k0)), key_part_1(__bswap_64(k1)), key_part_2(__bswap_64(k2)) {}
};

std::ostream& operator<<(std::ostream& os, const MultiPartKey& k) {
    os << "(" << __bswap_64(k.key_part_0) << "," << __bswap_64(k.key_part_1)
       << "," << __bswap_64(k.key_part_2) << ")";
    return os;
}

class MasstreeWrapper {
public:
    static constexpr uint64_t insert_bound = 0xfffffff;
    struct table_params : public Masstree::nodeparams<15,15> {
        typedef uint64_t value_type;
        typedef Masstree::value_print<value_type> value_print_type;
        typedef threadinfo threadinfo_type;
    };

    typedef Masstree::Str Str;
    typedef Masstree::basic_table<table_params> table_type;
    typedef Masstree::unlocked_tcursor<table_params> unlocked_cursor_type;
    typedef Masstree::tcursor<table_params> cursor_type;
    typedef Masstree::leaf<table_params> leaf_type;

    typedef typename table_type::node_type node_type;
    typedef typename unlocked_cursor_type::nodeversion_value_type nodeversion_value_type;

    static __thread typename table_params::threadinfo_type *ti;

    MasstreeWrapper() {
        this->table_init();
    }

    void table_init() {
        if (ti == nullptr)
            ti = threadinfo::make(threadinfo::TI_MAIN, -1);
        table_.initialize(*ti);
        key_gen_ = 0;
    }

    void keygen_reset() {
        key_gen_ = 0;
    }

    static void thread_init(int thread_id) {
        if (ti == nullptr)
            ti = threadinfo::make(threadinfo::TI_PROCESS, thread_id);
    }

    void insert_test(int thread_id) {
        thread_init(thread_id);
        while (1) {
            uint64_t key_buf = fetch_and_add(&key_gen_, 1);
            if (key_buf > insert_bound)
                break;
            Str key = make_key(&key_buf);

            cursor_type lp(table_, key);
            bool found = lp.find_insert(*ti);
            always_assert(!found, "keys should all be unique");

            lp.value() = key_buf;

            fence();
            lp.finish(1, *ti);
        }
    }

    void remove_test(int thread_id) {
        thread_init(thread_id);
        while (1) {
            uint64_t key_buf = fetch_and_add(&key_gen_, 1);
            if (key_buf > insert_bound)
                break;
            Str key = make_key(&key_buf);

            cursor_type lp(table_, key);
            bool found = lp.find_locked(*ti);
            always_assert(found, "keys must all exist");
            lp.finish(-1, *ti);
        }
    }

    void insert_remove_test(int thread_id) {
        thread_init(thread_id);
        std::mt19937 gen(thread_id);
        std::uniform_int_distribution<int> dist(1, 6);
        StoRandomDistribution<>::rng_type rng(thread_id/*seed*/);
        StoRandomDistribution<> *dist_0 = new StoZipfDistribution<>(rng /*generator*/, 1 /*low*/, 100000 /*high*/, 0.8 /*skew*/);
	StoRandomDistribution<> *dist_1 = new StoZipfDistribution<>(rng /*generator*/, 1 /*low*/, 200000 /*high*/, 0.2 /*skew*/);
        while (1) {
            uint64_t k0 = dist_0->sample();
	    uint64_t k1 = dist_1->sample();
            uint64_t k2 = fetch_and_add(&key_gen_, 1);
            MultiPartKey key_buf(k0, k1, k2);
            if (k2 > insert_bound)
                break;
            Str key = make_key(&key_buf);

            cursor_type lp(table_, key);
            bool found = lp.find_insert(*ti);
            always_assert(!found, "keys should all be unique 1");

            lp.value() = k2;
            fence();
            lp.finish(1, *ti);

            if (dist(gen) <= 2) {
                cursor_type lp1(table_, key);
                bool found1 = lp1.find_insert(*ti);
                if (!found1) {
                    std::cerr << "failed at key: " << key_buf << std::endl;
                    always_assert(found1, "this is my key!");
                }
                lp1.finish(-1, *ti);
            }
        }
        delete dist_0;
        delete dist_1;
    }

private:
    table_type table_;
    uint64_t key_gen_;

    template <typename K>
    static inline Str make_key(const K* key_buf) {
        return Str((const char*)key_buf, sizeof(K));
    }
};

pthread_barrier_t barrier;
__thread typename MasstreeWrapper::table_params::threadinfo_type* MasstreeWrapper::ti = nullptr;

volatile mrcu_epoch_type active_epoch = 1;
volatile uint64_t globalepoch = 1;
volatile bool recovering = false;

void test_thread(MasstreeWrapper* mt, int thread_id) {
    mt->thread_init(thread_id);
    mt->insert_remove_test(thread_id);
}

int main() {
    pthread_barrier_init(&barrier, nullptr, NUM_THREADS);
    auto mt = new MasstreeWrapper();
    std::vector<std::thread> ths;
    mt->keygen_reset();
    std::cout << "insert test..." << std::endl;
    for (int i = 0; i < NUM_THREADS; ++i)
        ths.emplace_back(&MasstreeWrapper::insert_test, mt, i);
    for (auto& t : ths)
        t.join();
    ths.clear();
    mt->keygen_reset();

    std::cout << "remove test..." << std::endl;
    for (int i = 0; i < NUM_THREADS; ++i)
        ths.emplace_back(&MasstreeWrapper::remove_test, mt, i);
    for (auto& t : ths)
        t.join();
    ths.clear();
    mt->keygen_reset();

    std::cout << "insert_remove test..." << std::endl;
    for (int i = 0; i < NUM_THREADS; ++i)
        ths.emplace_back(&MasstreeWrapper::insert_remove_test, mt, i);
    for (auto& t : ths)
        t.join();

    std::cout << "test pass." << std::endl;

    pthread_barrier_destroy(&barrier);
    return 0;
}
