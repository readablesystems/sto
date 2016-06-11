#include <string>
#include <iostream>
#include <cstdarg>
#include <assert.h>
#include <vector>
#include <random>
#include <time.h>
#include <condition_variable>
#include <mutex>

#include <cds/init.h>
#include <cds/container/fcpriority_queue.h>
#include <cds/container/mspriority_queue.h>
#include <cds/container/basket_queue.h>
#include <cds/container/fcqueue.h>
#include <cds/container/moir_queue.h>
#include <cds/container/msqueue.h>
#include <cds/container/rwqueue.h>
#include <cds/container/optimistic_queue.h>
#include <cds/container/segmented_queue.h>
#include <cds/container/tsigas_cycle_queue.h>
#include <cds/container/vyukov_mpmc_cycle_queue.h>
#include <cds/gc/hp.h> 

#include "Transaction.hh"
#include "PriorityQueue.hh"
#include "Queue.hh"
#include "randgen.hh"

#define GLOBAL_SEED 10

#define MAX_VALUE INT_MAX
#define MAX_SIZE 1000000
#define NTRANS 100000 // Number of transactions each thread should run.
#define MAX_NUM_THREADS 24 // Maximum number of concurrent threads
#define INITIAL_THREAD 0 // tid of the first thread spawned

// type of data structure to be used
#define STO 0
#define CDS 1

// value types
#define RANDOM_VALS 10
#define DECREASING_VALS 11

// types of operations available
enum op {push, pop};
std::array<op, 2> ops_array = {push, pop};

// types of queues
enum q_type { basket, fc, moir, ms, optimistic, rw, segmented, tc, vm };

// globals
std::atomic_int global_push_val(MAX_VALUE);
std::vector<int> init_sizes = {1000, 10000, 50000, 100000, 150000};
std::vector<int> nthreads_set = {1, 2, 4, 8, 12, 16, 20, 24};
// txn sets
std::vector<std::vector<std::vector<op>>> q_txn_sets = {
    // 0. short txns
    {{push, push, push}, {pop, pop, pop}, {pop}, {pop}, {pop}, {push}, {push}, {push}},
    // 1. longer txns
    {{push, push, push, push, push},{pop, pop, pop, pop, pop}}, 
    // 2. 100% include both pushes and ptps
    {{push, push, pop},{pop, pop, push}},
    // 3. 50% include both pushes and pops
    {{push, push, pop},{pop, pop, push},{pop}, {push}},
    // 4. 33% include both pushes and pops
    {{push, push, pop},{pop, pop, push},{pop}, {pop},{push}, {push}},
    // 5. 33%: longer push + pop txns
    {{push, pop, push, pop, push, pop},{pop}, {push}}
};

std::atomic_int spawned_barrier(0);

txp_counter_type global_thread_push_ctrs[MAX_NUM_THREADS];
txp_counter_type global_thread_pop_ctrs[MAX_NUM_THREADS];
txp_counter_type global_thread_skip_ctrs[MAX_NUM_THREADS];

FILE *global_verbose_stats_file;
FILE *global_stats_file;

// This thread is run after all threads modifying the data structure have been spawned. 
// It then iterates until a thread finishes and measures the max rate of ops/ms on the data structure. 
void* record_perf_thread(void* x);
// Takes a Tester (Test + params) struct and runs the requested test.
void* test_thread(void* data);

// Helper functions for printing
void dualprintf(const char* fmt,...); // prints to both the verbose and csv files
void print_abort_stats(void);        // prints how many aborts/commits, etc. for STO ds

/*
 * Datatype Harnesses
 */
template <typename DS> struct DatatypeHarness{};

template <typename DS> struct CDSDatatypeHarness {
    typedef typename DS::value_type value_type;
public:
    CDSDatatypeHarness() {};
    CDSDatatypeHarness(size_t nCapacity) : v_(nCapacity) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { v_.push(v); }
    void init_push(value_type v) { v_.push(v); }
    size_t size() { return v_.size(); }
private:
    DS v_;
};

/* 
 * Priority Queue Templates
 */
template <typename T> struct DatatypeHarness<PriorityQueue<T>> {
    typedef T value_type;
public:
    DatatypeHarness() {};
    bool pop() { return v_.pop(); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        return pop();
        assert(Sto::try_commit());
    }
    void push(value_type v) { v_.push(v); }
    void init_push(value_type v) {
        Sto::start_transaction();
        v_.push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return v_.unsafe_size(); }
private:
    PriorityQueue<T> v_;
};

template <typename T> struct DatatypeHarness<PriorityQueue<T, true>> {
    typedef T value_type;
public:
    DatatypeHarness() {};
    bool pop() { return v_.pop(); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        return pop();
        assert(Sto::try_commit());
    }
    void push(value_type v) { v_.push(v); }
    void init_push(value_type v) { 
        Sto::start_transaction();
        v_.push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return v_.unsafe_size(); }
private:
    PriorityQueue<T, true> v_;
};

template <typename T> struct DatatypeHarness<cds::container::MSPriorityQueue<T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(1000000) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { v_.push(v); }
    void init_push(value_type v) { v_.push(v); }
    size_t size() { return v_.size(); }
private:
    cds::container::MSPriorityQueue<T> v_;
};

template <typename T> struct DatatypeHarness<cds::container::FCPriorityQueue<T>> : 
    public CDSDatatypeHarness<cds::container::FCPriorityQueue<T>>{};

/* 
 * Queue Templates
 */
template <typename T> struct DatatypeHarness<Queue<T>> {
    typedef T value_type;
public:
    bool pop() { return v_.transPop(); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        return pop();
        assert(Sto::try_commit());
    }
    void push(T v) { v_.transPush(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_.transPush(v);
        assert(Sto::try_commit());
    }
    size_t size() { return 0; }
private:
    Queue<T> v_;
};

template <typename T> struct DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, T>> : 
    public CDSDatatypeHarness<cds::container::BasketQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::FCQueue<T>> : 
    public CDSDatatypeHarness<cds::container::FCQueue<T>>{};
template <typename T> struct DatatypeHarness<cds::container::MoirQueue<cds::gc::HP, T>> : 
    public CDSDatatypeHarness<cds::container::MoirQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::MSQueue<cds::gc::HP, T>> : 
    public CDSDatatypeHarness<cds::container::MSQueue<cds::gc::HP, T>> {};
template <typename T> struct DatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, T>> : 
    public CDSDatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::RWQueue<T>> : 
    public CDSDatatypeHarness<cds::container::RWQueue<T>>{};

template <typename T> struct DatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(32) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { v_.push(v); }
    void init_push(value_type v) { v_.push(v); }
    size_t size() { return v_.size(); }
private:
    cds::container::SegmentedQueue<cds::gc::HP, T> v_;
};
template <typename T> struct DatatypeHarness<cds::container::TsigasCycleQueue<T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(1000000) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { v_.push(v); }
    void init_push(value_type v) { v_.push(v); }
    size_t size() { return v_.size(); }
private:
    cds::container::TsigasCycleQueue<T> v_;
};
template <typename T> struct DatatypeHarness<cds::container::VyukovMPMCCycleQueue<T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(1000000) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { v_.push(v); }
    void init_push(value_type v) { v_.push(v); }
    size_t size() { return v_.size(); }
private:
    cds::container::VyukovMPMCCycleQueue<T> v_;
};
/*
 * Test interfaces and templates.
 */
class GenericTest {
public:
    virtual void initialize(size_t init_sz) = 0;
    virtual void run(int me) = 0;
    virtual void cleanup() = 0;
};

template <typename DH> class DHTest : public GenericTest {
public:
    DHTest(int val_type) : val_type_(val_type){};
    void initialize(size_t init_sz) {
        for (unsigned i = 0; i < init_sz; ++i) {
            v_.init_push(i);
        }
    }
    
    void cleanup() {
        while (v_.cleanup_pop()){/*keep popping*/}
    }

    void do_op(op op, Rand transgen) {
        std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);

        int val;
        switch(val_type_) {
            case RANDOM_VALS:
                val = slotdist(transgen);
                break;
            case DECREASING_VALS:
                val = --global_push_val;
                break;
            default: assert(0);
        }
        switch (op) {
            case pop: v_.pop(); break;
            case push: v_.push(val); break;
            default: assert(0);
        }
    }
    inline void inc_ctrs(op op, int me) {
        switch(op) {
            case push: global_thread_push_ctrs[me]++; break;
            case pop: global_thread_pop_ctrs[me]++; break;
            default: assert(0);
        }
    }
private:
    DH v_;
    int val_type_;
};

template <typename DH> class SingleOpTest : public DHTest<DH> {
public:
    SingleOpTest(int ds_type, int val_type, op op) : DHTest<DH>(val_type), op_(op), ds_type_(ds_type) {};
    void run(int me) {
        for (int i = NTRANS; i > 0; --i) {
            auto transseed = i;
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_NUM_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);

            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        DHTest<DH>::do_op(op_, transgen);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
                DHTest<DH>::inc_ctrs(op_, me);
            } else {
                DHTest<DH>::do_op(op_, transgen);
                DHTest<DH>::inc_ctrs(op_, me);
            }
        }
    }
private:
    op op_;
    int ds_type_;
};

template <typename DH> class PushPopTest : public DHTest<DH> {
public:
    PushPopTest(int ds_type, int val_type) : DHTest<DH>(val_type), ds_type_(ds_type) {};
    void run(int me) {
        if (me > 1) { return; }
        for (int i = NTRANS; i > 0; --i) {
            auto transseed = i;
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_NUM_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        DHTest<DH>::do_op(ops_array[me % arraysize(ops_array)], transgen);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
                DHTest<DH>::inc_ctrs(ops_array[me % arraysize(ops_array)], me);
            } else {
                DHTest<DH>::do_op(ops_array[me % arraysize(ops_array)], transgen);
                DHTest<DH>::inc_ctrs(ops_array[me % arraysize(ops_array)], me);
            }
        }
    }
private:
    int ds_type_;
};

template <typename DH> class RandomSingleOpTest : public DHTest<DH> {
public:
    RandomSingleOpTest(int ds_type, int val_type) : DHTest<DH>(val_type), ds_type_(ds_type) {};
    void run(int me) {
        op my_op;
        for (int i = NTRANS; i > 0; --i) {
            auto transseed = i;
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_NUM_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        my_op = ops_array[transgen() % arraysize(ops_array)];
                        DHTest<DH>::do_op(my_op, transgen);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
                DHTest<DH>::inc_ctrs(my_op, me);
            } else {
                my_op = ops_array[transgen() % arraysize(ops_array)];
                DHTest<DH>::do_op(my_op, transgen);
                DHTest<DH>::inc_ctrs(my_op, me);
            } 
        }
    }
private:
    int ds_type_;
};

template <typename DH> class GeneralTxnsTest : public DHTest<DH> {
public:
    GeneralTxnsTest(int ds_type, int val_type, std::vector<std::vector<op>> txn_set) : 
        DHTest<DH>(val_type), ds_type_(ds_type), txn_set_(txn_set) {};
    void run(int me) {
        for (int i = NTRANS; i > 0; --i) {
            auto transseed = i;
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_NUM_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        auto txn = txn_set_[transgen() % txn_set_.size()];
                        for (unsigned j = 0; j < txn.size(); ++j) {
                            DHTest<DH>::do_op(txn[j], transgen);
                            DHTest<DH>::inc_ctrs(txn[j], me); // XXX can lead to overcounting
                        }
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
            } else {
                auto txn = txn_set_[transgen() % txn_set_.size()];
                for (unsigned j = 0; j < txn.size(); ++j) {
                    DHTest<DH>::do_op(txn[j], transgen);
                    DHTest<DH>::inc_ctrs(txn[j], me);
                }
            }
        }
    }
private:
    int ds_type_;
    std::vector<std::vector<op>> txn_set_;
};

/*
 * Tester defines the test parameters 
 * and the Test object to run the test
 */

struct Tester {
    int me; // tid
    int nthreads;
    size_t init_size;
    GenericTest* test;   
};

/*
 * Threads to run the tests and record performance
 */
void* test_thread(void *data) {
    cds::threading::Manager::attachThread();

    GenericTest *gt = ((Tester*)data)->test;
    int me = ((Tester*)data)->me;
    int nthreads = ((Tester*)data)->nthreads;
    size_t init_size = ((Tester*)data)->init_size;

    if (me == INITIAL_THREAD) {
        gt->initialize(init_size); 
    }
    
    spawned_barrier++;
    while (spawned_barrier != nthreads) {
        sched_yield();
    }
   
    gt->run(me);
    gt->cleanup();

    cds::threading::Manager::detachThread();
    return nullptr;
}

void* record_perf_thread(void* x) {
    int nthreads = *(int*)x;
    int total1, total2;
    struct timeval tv1, tv2;
    float ops_per_ms = 0; 

    while (spawned_barrier != nthreads) {
        sched_yield();
    }

    // benchmark until the first thread finishes
    struct timespec ts = {0, 10000};
    while (spawned_barrier == nthreads) {
        total1 = total2 = 0;
        gettimeofday(&tv1, NULL);
        for (int i = 0; i < MAX_NUM_THREADS; ++i) {
            total1 += (global_thread_push_ctrs[i] + global_thread_pop_ctrs[i]);
        }
        nanosleep(&ts, NULL);
        for (int i = 0; i < MAX_NUM_THREADS; ++i) {
            total2 += (global_thread_push_ctrs[i] + global_thread_pop_ctrs[i]);
        }
        gettimeofday(&tv2, NULL);
        float microseconds = ((tv2.tv_sec-tv1.tv_sec)*1000000.0) + ((tv2.tv_usec-tv1.tv_usec)/1000.0);
        ops_per_ms = (total2-total1)/microseconds > ops_per_ms ? (total2-total1)/microseconds : ops_per_ms;
    }
    dualprintf("%f, ", ops_per_ms);
    return nullptr;
}

void startAndWait(GenericTest* test, size_t size, int nthreads) {
    // create performance recording thread
    pthread_t recorder;
    pthread_create(&recorder, NULL, record_perf_thread, &nthreads);
    // create threads to run the test
    pthread_t tids[nthreads];
    Tester testers[nthreads];
    for (int i = 0; i < nthreads; ++i) {
        testers[i].me = i;
        testers[i].test = test;
        testers[i].init_size = size;
        testers[i].nthreads = nthreads;
        pthread_create(&tids[i], NULL, test_thread, &testers[i]);
    }
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(tids[i], NULL);
    }
    spawned_barrier = 0;
    pthread_join(recorder, NULL);

    fprintf(global_verbose_stats_file, "\n");
    for (int i = 0; i < nthreads; ++i) {
        // prints the number of pushes, pops, and skips
        fprintf(global_verbose_stats_file, "Thread %d \tpushes: %ld \tpops: %ld, \tskips: %ld\n", i, 
                global_thread_push_ctrs[i], 
                global_thread_pop_ctrs[i], 
                global_thread_skip_ctrs[i]);
        global_thread_pop_ctrs[i] = 0;
        global_thread_push_ctrs[i] = 0;
        global_thread_skip_ctrs[i] = 0;
    }
    print_abort_stats();
}

void dualprintf(const char* fmt,...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_start(args2, fmt);
    vfprintf(global_verbose_stats_file, fmt, args1);
    vfprintf(global_stats_file, fmt, args2);
    va_end(args1);
    va_end(args2);
}

void print_abort_stats() {
#if STO_PROFILE_COUNTERS
    if (txp_count >= txp_total_aborts) {
        txp_counters tc = Transaction::txp_counters_combined();

        unsigned long long txc_total_starts = tc.p(txp_total_starts);
        unsigned long long txc_total_aborts = tc.p(txp_total_aborts);
        unsigned long long txc_commit_aborts = tc.p(txp_commit_time_aborts);
        unsigned long long txc_total_commits = txc_total_starts - txc_total_aborts;
        fprintf(global_verbose_stats_file, "\t$ %llu starts, %llu max read set, %llu commits",
                txc_total_starts, tc.p(txp_max_set), txc_total_commits);
        if (txc_total_aborts) {
            fprintf(global_verbose_stats_file, ", %llu (%.3f%%) aborts",
                    tc.p(txp_total_aborts),
                    100.0 * (double) tc.p(txp_total_aborts) / tc.p(txp_total_starts));
            if (tc.p(txp_commit_time_aborts))
                fprintf(global_verbose_stats_file, "\n$ %llu (%.3f%%) of aborts at commit time",
                        tc.p(txp_commit_time_aborts),
                        100.0 * (double) tc.p(txp_commit_time_aborts) / tc.p(txp_total_aborts));
        }
        unsigned long long txc_commit_attempts = txc_total_starts - (txc_total_aborts - txc_commit_aborts);
        fprintf(global_verbose_stats_file, "\n\t$ %llu commit attempts, %llu (%.3f%%) nonopaque\n",
                txc_commit_attempts, tc.p(txp_commit_time_nonopaque),
                100.0 * (double) tc.p(txp_commit_time_nonopaque) / txc_commit_attempts);
    }
    Transaction::clear_stats();
#endif
}

#define MAKE_PQUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO pqueue", new test<DatatypeHarness<PriorityQueue<type>>>(STO, ## __VA_ARGS__)},      \
    {desc, "STO pqueue opaque", new test<DatatypeHarness<PriorityQueue<type, true>>>(STO, ## __VA_ARGS__)},\
    {desc, "MS pqueue", new test<DatatypeHarness<cds::container::MSPriorityQueue<type>>>(CDS, ## __VA_ARGS__)},\
    {desc, "FC pqueue", new test<DatatypeHarness<cds::container::FCPriorityQueue<type>>>(CDS, ## __VA_ARGS__)}
struct Test {
    const char* desc;
    const char* ds;
    GenericTest* test;
} pqueue_tests[] = {
    MAKE_PQUEUE_TESTS("Push+Pop with Random Vals", PushPopTest, int, RANDOM_VALS),
    MAKE_PQUEUE_TESTS("Push+Pop with Decreasing Vals", PushPopTest, int, DECREASING_VALS),
    MAKE_PQUEUE_TESTS("Random Single Operations with Random Vals", RandomSingleOpTest, int, RANDOM_VALS),
    MAKE_PQUEUE_TESTS("Random Single Operations with Decreasing Vals", RandomSingleOpTest, int, DECREASING_VALS),
    MAKE_PQUEUE_TESTS("General Txns Test with Random Vals", GeneralTxnsTest, int, RANDOM_VALS, q_txn_sets[0]),
    MAKE_PQUEUE_TESTS("General Txns Test with Decreasing Vals", GeneralTxnsTest, int, DECREASING_VALS, q_txn_sets[0]),
    MAKE_PQUEUE_TESTS("Push-Only with Random Vals", SingleOpTest, int, RANDOM_VALS, push),
    MAKE_PQUEUE_TESTS("Push-Only with Random Vals", SingleOpTest, int, DECREASING_VALS, push),
};
int num_pqueues = 4;

#define MAKE_QUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO queue", new test<DatatypeHarness<Queue<type>>>(STO, ## __VA_ARGS__)},                                  \
    {desc, "Basket queue", new test<DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},         \
    {desc, "FC queue", new test<DatatypeHarness<cds::container::FCQueue<type>>>(CDS, ## __VA_ARGS__)},                 \
    {desc, "Moir queue", new test<DatatypeHarness<cds::container::MoirQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "MS queue", new test<DatatypeHarness<cds::container::MSQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},   \
    {desc, "Opt queue", new test<DatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},   \
    {desc, "RW queue", new test<DatatypeHarness<cds::container::RWQueue<type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "Segmented queue", new test<DatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "TC queue", new test<DatatypeHarness<cds::container::TsigasCycleQueue<type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "VyukovMPMC queue", new test<DatatypeHarness<cds::container::VyukovMPMCCycleQueue<type>>>(CDS, ## __VA_ARGS__)}
Test queue_tests[] = {
    MAKE_QUEUE_TESTS("Push+Pop with Random Vals", PushPopTest, int, RANDOM_VALS),
    MAKE_QUEUE_TESTS("Push+Pop with Decreasing Vals", PushPopTest, int, DECREASING_VALS),
    MAKE_QUEUE_TESTS("Random Single Operations with Random Vals", RandomSingleOpTest, int, RANDOM_VALS),
    MAKE_QUEUE_TESTS("Random Single Operations with Decreasing Vals", RandomSingleOpTest, int, DECREASING_VALS),
    MAKE_QUEUE_TESTS("General Txns Test with Random Vals", GeneralTxnsTest, int, RANDOM_VALS, q_txn_sets[0]),
    MAKE_QUEUE_TESTS("General Txns Test with Decreasing Vals", GeneralTxnsTest, int, DECREASING_VALS, q_txn_sets[0]),
};
int num_queues = 10;

int main() {
    global_verbose_stats_file = fopen("cds_benchmarks_stats_verbose.txt", "a");
    global_stats_file = fopen("cds_benchmarks_stats.txt", "a");
    if ( !global_stats_file || !global_verbose_stats_file ) {
        fprintf(stderr, "Could not open file to write stats");
        return 1;
    }

    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh

    cds::Initialize();
    cds::gc::HP hpGC;
    cds::threading::Manager::attachThread();

    // create epoch advancer thread
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);

    dualprintf("\nRUNNING PQUEUE TESTS\n");
    for (unsigned i = 0; i < arraysize(pqueue_tests); i+=num_pqueues) {
        dualprintf("\nNew Pqueue Test\n");
        for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
            for (auto nthreads = begin(nthreads_set); nthreads != end(nthreads_set); ++nthreads) {
                for (int j = 0; j < num_pqueues; ++j) {
                    fprintf(global_verbose_stats_file, "\nRunning Test %s on %s\t size: %d, nthreads: %d\n", 
                            pqueue_tests[i+j].desc, pqueue_tests[i+j].ds, *size, *nthreads);
                    startAndWait(pqueue_tests[i+j].test, *size, *nthreads);
                    fprintf(stderr, "\nRan Test %s on %s\t size: %d, nthreads: %d\n", 
                            pqueue_tests[i+j].desc, pqueue_tests[i+j].ds, *size, *nthreads);
                }
                dualprintf("\n");
            }
            dualprintf("\n");
        }
    }
    dualprintf("\nRUNNING QUEUE TESTS\n");
    for (unsigned i = 0; i < arraysize(queue_tests); i+=num_queues) {
        dualprintf("\nNew Queue Test\n");
        for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
            for (auto nthreads = begin(nthreads_set); nthreads != end(nthreads_set); ++nthreads) {
                for (int j = 0; j < num_queues; ++j) {
                    fprintf(global_verbose_stats_file, "\nRunning Test %s on %s\t size: %d, nthreads: %d\n", 
                            queue_tests[i+j].desc, queue_tests[i+j].ds, *size, *nthreads);
                    startAndWait(queue_tests[i+j].test, *size, *nthreads);
                    fprintf(stderr, "\nRan Test %s on %s\t size: %d, nthreads: %d\n", 
                            queue_tests[i+j].desc, queue_tests[i+j].ds, *size, *nthreads);
                }
                dualprintf("\n");
            }
            dualprintf("\n");
        }
    }

    cds::Terminate();
    return 0;
}


