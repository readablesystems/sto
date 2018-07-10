#include <iostream>
#include <random>
#include <vector>
#include <thread>
#include <byteswap.h>

#include <pthread.h>

#include "config.h"
#include "compiler.hh"

#include "masstree.hh"
#include "kvthread.hh"
#include "masstree_tcursor.hh"
#include "masstree_insert.hh"
#include "masstree_print.hh"
#include "masstree_remove.hh"
#include "masstree_scan.hh"
#include "string.hh"

#define NUM_THREADS 4
#define NTHREAD 20
#define NVALS 10000000

uint64_t* keys;

class MasstreeWrapper {
public:
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

    void insert(int int_key) {
        uint64_t key_buf;
        Str key = make_key(int_key, key_buf);

        cursor_type lp(table_, key);
        bool found = lp.find_insert(*ti);
        always_assert(!found, "keys should all be unique");

        lp.value() = int_key;

        fence();
        lp.finish(1, *ti);
    }

    int lookup(int int_key) {
        uint64_t key_buf;
        Str key = make_key(int_key, key_buf);

        unlocked_cursor_type lp(table_, key);
        bool found = lp.find_unlocked(*ti);
        always_assert(found, "keys must all exist");
        return lp.value();
    }

    void remove(int int_key) {
        uint64_t key_buf;
        Str key = make_key(int_key, key_buf);

        cursor_type lp(table_, key);
        bool found = lp.find_locked(*ti);
        always_assert(found, "keys must all exist");
        lp.finish(-1, *ti);
    }

private:
    table_type table_;
    uint64_t key_gen_;

    static inline Str make_key(uint64_t int_key, uint64_t& key_buf) {
        key_buf = __bswap_64(int_key);
        return Str((const char *)&key_buf, sizeof(key_buf));
    }
};

pthread_barrier_t barrier;
__thread typename MasstreeWrapper::table_params::threadinfo_type* MasstreeWrapper::ti = nullptr;

volatile mrcu_epoch_type active_epoch = 1;
volatile uint64_t globalepoch = 1;
volatile bool recovering = false;


void insertKey(MasstreeWrapper* mt, int thread_id) {
    mt->thread_init(thread_id);

    for (int i = thread_id*(NVALS/NTHREAD); i < (thread_id+1)*NVALS/NTHREAD; i++) {
        mt->insert(keys[i]);
    }
}

void lookupKey(MasstreeWrapper* mt, int thread_id) {
    mt->thread_init(thread_id);

    for (int i = thread_id*(NVALS/NTHREAD); i < (thread_id+1)*NVALS/NTHREAD; i++) {
        int v = mt->lookup(keys[i]);
        assert(v == keys[i]);
    }
}

int main() {
    pthread_barrier_init(&barrier, nullptr, NUM_THREADS);
    auto mt = new MasstreeWrapper();

    keys = new uint64_t[NVALS];
    for (uint64_t i = 0; i < NVALS; i++) {
        keys[i] = i;
    }

    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[NTHREAD];
        for (int i = 0; i < NTHREAD; i++) {
            threads[i] = std::thread(insertKey, mt, i);
        }

        for (int i = 0; i < NTHREAD; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("insert,%d,%f\n", NVALS, (NVALS * 1.0) / duration.count());
    }
    {
        auto starttime = std::chrono::system_clock::now();
        std::thread threads[NTHREAD];
        for (int i = 0; i < NTHREAD; i++) {
            threads[i] = std::thread(lookupKey, mt, i);
        }

        for (int i = 0; i < NTHREAD; i++) {
            threads[i].join();
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("lookup,%d,%f\n", NVALS, (NVALS * 1.0) / duration.count());
    }

    pthread_barrier_destroy(&barrier);
    return 0;
}
