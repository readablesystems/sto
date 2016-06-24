#pragma once

#include <string>
#include <iostream>
#include <cstdarg>
#include <assert.h>
#include <vector>
#include <random>
#include <time.h>
#include <memory>

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
#include <cds/algo/elimination_opt.h> 
#include <cds/os/alloc_aligned.h> 
#include <cds/opt/options.h> 

#include "Transaction.hh"
#include "PriorityQueue.hh"
#include "PairingHeap.hh"
#include "Queue.hh"
#include "Queue2.hh"
#include "FCQueue.hh"
#include "randgen.hh"

#define GLOBAL_SEED 10

#define MAX_VALUE 20 
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
std::vector<int> nthreads_set = {1, 2, 4, 8, 12, 16, 20};//, 24};
int rand_vals[10000];

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

struct __attribute__((aligned(128))) cds_counters {
    txp_counter_type push;
    txp_counter_type pop;
    txp_counter_type skip;
};

cds_counters global_thread_ctrs[MAX_NUM_THREADS];

FILE *global_verbose_stats_file;
FILE *global_stats_file;

unsigned initial_seeds[64];

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

template <typename DS> struct CDSDatatypeHarness { typedef typename DS::value_type value_type;
public:
    CDSDatatypeHarness() {};
    CDSDatatypeHarness(size_t nCapacity) : v_(nCapacity) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
    size_t size() { return v_.size(); }
    int num_combines() { return v_.statistics().m_nCombiningCount; }
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
        if (v_.unsafe_size() > 0) {
            Sto::start_transaction();
            v_.pop();
            assert(Sto::try_commit());
            return true;
        } else return false;
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
        if (v_.unsafe_size() > 0) {
            Sto::start_transaction();
            v_.pop();
            assert(Sto::try_commit());
            return true;
        } else return false;
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
    DatatypeHarness() : v_(10000000) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
    size_t size() { return v_.size(); }
private:
    cds::container::MSPriorityQueue<T> v_;
};

template <typename T> struct DatatypeHarness<cds::container::FCPriorityQueue<T>> : 
    public CDSDatatypeHarness<cds::container::FCPriorityQueue<T>>{};

template <typename T> struct DatatypeHarness<cds::container::FCPriorityQueue<T, PairingHeap<T>>> : 
    public CDSDatatypeHarness<cds::container::FCPriorityQueue<T, PairingHeap<T>>>{};

/* 
 * Queue Templates
 */
template <typename T> struct DatatypeHarness<Queue<T>> {
    typedef T value_type;
public:
    bool pop() { return v_.pop(); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        bool ret = pop();
        assert(Sto::try_commit());
        return ret;
    }
    void push(T v) { v_.push(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_.push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return 0; }
private:
    Queue<T> v_;
};
template <typename T> struct DatatypeHarness<Queue2<T>> {
    typedef T value_type;
public:
    bool pop() { return v_.pop(); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        bool ret = pop();
        assert(Sto::try_commit());
        return ret;
    }
    void push(T v) { v_.push(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_.push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return 0; }
private:
    Queue2<T> v_;
};
template <typename T> struct DatatypeHarness<FCQueue<T>> {
    typedef T value_type;
public:
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        bool r = pop();
        assert(Sto::try_commit());
        return r;
    }
    void push(T v) { v_.push(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_.push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return v_.size(); }
private:
    FCQueue<T> v_;
};

template <typename T> struct DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, T>> : 
    public CDSDatatypeHarness<cds::container::BasketQueue<cds::gc::HP, T>>{};
#define FCQUEUE_TRAITS() cds::container::fcqueue::make_traits< \
    cds::opt::lock_type<cds::sync::spin>, \
    cds::opt::back_off<cds::backoff::delay_of<2>>, \
    cds::opt::allocator<CDS_DEFAULT_ALLOCATOR>, \
    cds::opt::stat<cds::container::fcqueue::stat<cds::atomicity::event_counter>>, \
    cds::opt::memory_model<cds::opt::v::relaxed_ordering>, \
    cds::opt::enable_elimination<false>>::type
template <typename T> struct DatatypeHarness<cds::container::FCQueue<T, std::queue<T>, FCQUEUE_TRAITS()>> : 
    public CDSDatatypeHarness<cds::container::FCQueue<T, std::queue<T>, FCQUEUE_TRAITS()>>{};
#define FCQUEUE_TRAITS_ELIM() cds::container::fcqueue::make_traits< \
    cds::opt::lock_type<cds::sync::spin>, \
    cds::opt::back_off<cds::backoff::delay_of<2>>, \
    cds::opt::allocator<CDS_DEFAULT_ALLOCATOR>, \
    cds::opt::stat<cds::container::fcqueue::empty_stat>, \
    cds::opt::memory_model<cds::opt::v::relaxed_ordering>, \
    cds::opt::enable_elimination<true>>::type
template <typename T> struct DatatypeHarness<cds::container::FCQueue<T, std::queue<T>, FCQUEUE_TRAITS_ELIM()>>  :
    public CDSDatatypeHarness<cds::container::FCQueue<T, std::queue<T>, FCQUEUE_TRAITS_ELIM()>>{};
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
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
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
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
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
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
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
        global_push_val = MAX_VALUE;
        // initialize with the maximum values, so that
        // at the beginning, pushes and pops will not conflict
        for (unsigned i = 0; i < init_sz; ++i) {
            v_.init_push(global_push_val--);
        }
    }
    
    void cleanup() {
        while (v_.cleanup_pop()){/*keep popping*/}
    }

    inline void do_op(op op, int val) {
        switch(val_type_) {
            case RANDOM_VALS:
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
            case push: global_thread_ctrs[me].push++; break;
            case pop: global_thread_ctrs[me].pop++; break;
            default: assert(0);
        }
    }
protected:
    DH v_;
    int val_type_;
};

template <typename DH> class SingleOpTest : public DHTest<DH> {
public:
    SingleOpTest(int ds_type, int val_type, op op) : DHTest<DH>(val_type), op_(op), ds_type_(ds_type) {};
    void run(int me) {
        Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        this->do_op(op_, rand_vals[(i*me + i) % arraysize(rand_vals)]);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(op_, me);
            } else {
                this->do_op(op_, rand_vals[(i*me + i)% arraysize(rand_vals)]);
                this->inc_ctrs(op_, me);
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
        if (me > 1) { sleep(1); return; }
        //Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        //std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                //Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        this->do_op(ops_array[me % arraysize(ops_array)], rand_vals[(i*me + i) % arraysize(rand_vals)]);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        //transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(ops_array[me % arraysize(ops_array)], me);
            } else {
                this->do_op(ops_array[me % arraysize(ops_array)], rand_vals[(i*me + i) % arraysize(rand_vals)]);
                this->inc_ctrs(ops_array[me % arraysize(ops_array)], me);
            }
        }
    }
    void print() {
        fprintf(stderr, "Num Combines %d\n", this->v_.num_combines());
    }
private:
    int ds_type_;
};

template <typename DH> class RandomSingleOpTest : public DHTest<DH> {
public:
    RandomSingleOpTest(int ds_type, int val_type) : DHTest<DH>(val_type), ds_type_(ds_type) {};
    void run(int me) {
        op my_op;
        //Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        //std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                //Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        my_op = ops_array[ops_array[i % 2]];
                        //my_op = ops_array[slotdist(transgen) % arraysize(ops_array)];
                        this->do_op(my_op, rand_vals[(i*me + i) % arraysize(rand_vals)]);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        //transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(my_op, me);
            } else {
                my_op = ops_array[ops_array[i%2]];
                //my_op = ops_array[slotdist(transgen) % arraysize(ops_array)];
                this->do_op(my_op, rand_vals[(i*me + i) % arraysize(rand_vals)]);
                this->inc_ctrs(my_op, me);
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
        Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        auto txn = txn_set_[slotdist(transgen) % txn_set_.size()];
                        for (unsigned j = 0; j < txn.size(); ++j) {
                            this->do_op(txn[j], rand_vals[(i*me + i) % arraysize(rand_vals)]);
                            this->inc_ctrs(txn[j], me); // XXX can lead to overcounting
                        }
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
            } else {
                auto txn = txn_set_[slotdist(transgen) % txn_set_.size()];
                for (unsigned j = 0; j < txn.size(); ++j) {
                    this->do_op(txn[j], rand_vals[(i*me + i) % arraysize(rand_vals)]);
                    this->inc_ctrs(txn[j], me);
                }
            }
        }
    }
private:
    int ds_type_;
    std::vector<std::vector<op>> txn_set_;
};

