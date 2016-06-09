#pragma once

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
#define MAX_NUM_THREADS 22 // Maximum number of concurrent threads

// type of data structure to be used
#define STO 0
#define CDS 1

// value types
#define RANDOM_VALS 10
#define DECREASING_VALS 11

// benchmarks types
#define PUSHPOP 12      // two threads, one pushing one popping
#define POPONLY 13     // multiple threads, all only pushing
#define PUSHONLY 14     // multiple threads, all only pushing
#define RANDOMOP 15  // choose one operation at random per txn
#define GENERAL 16      // run arbitrary txns of arbitrary length

// types of operations available
enum op {push, pop};
// types of queues
enum q_type { basket, fc, moir, ms, optimistic, rw, segmented, tc, vm };

// globals
extern std::atomic_int global_val;              // used to insert decreasing vals into ds
extern std::vector<int> init_sizes;             // initial sizes upon to which to run the test 
extern std::vector<int> nthreads_set;           // nthreads with which to run the benchmark

// counters of operations done by each thread
extern txp_counter_type global_thread_push_ctrs[MAX_NUM_THREADS];
extern txp_counter_type global_thread_pop_ctrs[MAX_NUM_THREADS];
extern txp_counter_type global_thread_skip_ctrs[MAX_NUM_THREADS];

// condition variables for synchronizing benchmark starts
extern std::mutex run_mtx;                      // mutex for the global_run_cv
extern std::condition_variable global_run_cv;   // broadcasts to the threads to begin running the benchmark
extern std::atomic_int spawned;                 // count of how many threads have successfully spawned
extern std::mutex spawned_mtx;                  // mutex for the global_spawned_cv
extern std::condition_variable global_spawned_cv; // each thread signals on this cv when it has spawned
                                                  // the main thread waits on this cv until spawned=nthreads
                                                  
// this thread is run after all threads modifying the data structure 
// have been spawned. It then iterates for 300 cycles of 0.1ms (a total of 30 ms) 
// measuring the maximum rate at which the threads are operating on the data structure.
void* record_perf_thread(void*);

// helper functions for printing
void dualprint(const char* fmt,...); // prints to both the verbose and csv files
void print_abort_stats(void);        // prints how many aborts/commits, etc. for STO ds

// txn sets for the general benchmark
extern std::vector<std::vector<std::vector<op>>> q_txn_sets;

template <typename T>
struct Tester {
    T* ds;          // pointer to the data structure
    int me;         // tid
    int bm;         // benchmark 
    size_t size;    // initial size of the ds 
    std::vector<std::vector<op>> txns_to_run;
    std::vector<int> vals_to_insert;
};

template <typename T>
void* run_sto_thread(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    auto q = tp->ds;
    int bm = tp->bm;
    TThread::set_id(tp->me);

    spawned++; 
    global_spawned_cv.notify_one();
    
    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();

    auto txns_to_run = tp->txns_to_run;
    auto vals_to_insert = tp->vals_to_insert;
    assert(vals_to_insert.size() == NTRANS);

    switch(bm) {
        case POPONLY:
            for (int i = NTRANS; i > 0; --i) {
                while (1) {
                    Sto::start_transaction();
                    try {
                        q->pop();
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {}
                }
                global_thread_pop_ctrs[tp->me]++;
            }
        break;
        case PUSHONLY:
            for (int i = NTRANS; i > 0; --i) {
                while (1) {
                    Sto::start_transaction();
                    try {
                        q->push(vals_to_insert[i]);
                        if (Sto::try_commit()) {
                            break;
                        }
                    } catch (Transaction::Abort e) {}
                }
                global_thread_push_ctrs[tp->me]++;
            }
        break;
        case RANDOMOP:
        {
            for (int i = NTRANS; i > 0; --i) {
                auto transseed = i;
                uint32_t seed = transseed*3 + (uint32_t)tp->me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_NUM_THREADS*NTRANS*11;
                auto seedlow = seed & 0xffff;
                auto seedhigh = seed >> 16;
                Rand transgen(seed, seedlow << 16 | seedhigh);
                auto transgen_snap = transgen;
            
                while (1) {
                    Sto::start_transaction();
                    try {
                        if (transgen() % 2 == 0) {
                            q->push(vals_to_insert[i]);
                            global_thread_push_ctrs[tp->me]++;
                        } else {
                            q->pop();
                            global_thread_pop_ctrs[tp->me]++;
                        }
                        if (Sto::try_commit()) break;
                    }
                    catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
            }
            break;
        }
        case GENERAL:
        {
            assert(txns_to_run.size() == vals_to_insert.size());
            auto val = begin(vals_to_insert);
            for (auto txn = begin(txns_to_run); txn != end(txns_to_run); ++txn) {
                std::vector<op> txn_ops = *txn;
                int txn_val = *val;
                val++;        
                
                while (1) {
                    Sto::start_transaction();
                    try {
                        do_txn(tp->ds, tp->me, txn_ops, txn_val, tp->size);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {}
                }
            }
            break;
        }
        default: assert(0);
    }
    return nullptr;
}

template <typename T>
void* run_cds_thread(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    auto q = tp->ds;
    int bm = tp->bm;
    cds::threading::Manager::attachThread();

    spawned++; 
    global_spawned_cv.notify_one();
    
    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();

    auto txns_to_run = tp->txns_to_run;
    auto vals_to_insert = tp->vals_to_insert;
    assert(vals_to_insert.size() == NTRANS);

    switch(bm) {
        case POPONLY:
            for (int i = NTRANS; i > 0; --i) {
                q->pop();
                global_thread_pop_ctrs[tp->me]++;
            }
        break;
        case PUSHONLY:
            for (int i = NTRANS; i > 0; --i) {
                q->push(vals_to_insert[i]);
                global_thread_push_ctrs[tp->me]++;
            }
        break;
        case RANDOMOP:
        {
            for (int i = NTRANS; i > 0; --i) {
                auto transseed = i;
                uint32_t seed = transseed*3 + (uint32_t)tp->me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_NUM_THREADS*NTRANS*11;
                auto seedlow = seed & 0xffff;
                auto seedhigh = seed >> 16;
                Rand transgen(seed, seedlow << 16 | seedhigh);
                if (transgen() % 2 == 0) {
                    q->push(vals_to_insert[i]);
                    global_thread_push_ctrs[tp->me]++;
                } else {
                    q->pop();
                    global_thread_pop_ctrs[tp->me]++;
                }
            }
            break;
        }
        case GENERAL:
        {
            assert(txns_to_run.size() == vals_to_insert.size());
            auto val = begin(vals_to_insert);
            for (auto txn = begin(txns_to_run); txn != end(txns_to_run); ++txn) {
                std::vector<op> txn_ops = *txn;
                int txn_val = *val;
                val++;        
                do_txn(tp->ds, tp->me, txn_ops, txn_val, tp->size);
            }
            break;
        }
        default: assert(0);
    }
    cds::threading::Manager::detachThread();
    return nullptr;
}

template <typename T>
void startAndWait(T* ds, int ds_type, 
        int bm, int val_type,
        std::vector<std::vector<op>> txn_set, 
        size_t size, int nthreads) {
    
    pthread_t tids[nthreads+1];
    Tester<T> testers[nthreads];
    int val;
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    
    /* 
     * Set up the testers with which to the threads will run the benchmark.
     * For GENERAL benchmark, preallocate the operations and values for all threads to run.
     * All other benchmarks will not have a "txns_to_run" field, as their operations are predefined
     */
    for (int me = 0; me < nthreads; ++me) {
        for (int j = 0; j < NTRANS; ++j) {
            auto transseed = j;
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_NUM_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            
            // randomly select txns to run if we're doing the general benchmark
            if (bm == GENERAL) {
                auto txn = txn_set[transgen() % txn_set.size()];
                testers[me].txns_to_run.push_back(txn);
            }

            switch(val_type) {
                case RANDOM_VALS:
                    val = slotdist(transgen);
                    break;
                case DECREASING_VALS:
                    val = --global_val;
                    break;
                default: assert(0);
            }
            testers[me].vals_to_insert.push_back(val);
        }
    }
    for (int i = 0; i < nthreads; ++i) {
        testers[i].ds = ds;
        testers[i].bm = bm;
        testers[i].me = i;
        testers[i].size = size;
    }
    
    /* 
     * Start the threads that do the operations.
     */
    if (bm == PUSHPOP) {
        testers[0].bm = POPONLY;
        testers[1].bm = PUSHONLY;
    }
    for (int i = 0; i < nthreads; ++i) {
        if (ds_type == CDS) pthread_create(&tids[i], NULL, run_cds_thread<T>, &testers[i]);
        else pthread_create(&tids[i], NULL, run_sto_thread<T>, &testers[i]);
    }
  
    /* 
     * Wait until all threads are ready, then tell all threads to begin execution.
    */ 
    std::unique_lock<std::mutex> spawned_lk(spawned_mtx);
    global_spawned_cv.wait(spawned_lk, [&nthreads]{return spawned==nthreads;});
    spawned_lk.unlock();
  
    sleep(0.5*nthreads); // sleep so that the threads will wait before the notification
    global_run_cv.notify_all();
 
    /* 
     * Create the thread to track performance and print stats
     */
    pthread_create(&tids[nthreads], NULL, record_perf_thread, NULL);

    sleep(0.1);
    global_run_cv.notify_all(); // notify again just to be paranoid

    for (int i = 0; i < nthreads+1; ++i) {
        pthread_join(tids[i], NULL);
    }
    if (ds_type == STO) print_abort_stats();

    /* 
     * Reset the counters so that the next benchmark run has accurate counts
     */
    for(int i = 0; i < MAX_NUM_THREADS; ++i) {
        global_thread_pop_ctrs[i] = 0;
        global_thread_push_ctrs[i] = 0;
        global_thread_skip_ctrs[i] = 0;
    }
    spawned = 0;
}

template <typename T>
inline void do_txn(T* q, int me, std::vector<op> txn_ops, int txn_val, size_t size) {
    for (unsigned j = 0; j < txn_ops.size(); ++j) {
        switch(txn_ops[j]) {
            case push:
                q->push(txn_val);
                global_thread_push_ctrs[me]++;
                break;
            case pop:
                if (size && q->size() < size*.25) {
                    global_thread_skip_ctrs[me]++;
                    break;
                }
                q->pop();
                global_thread_pop_ctrs[me]++;
                break;
            default:
                assert(0);
                break;
        }
    }
}


/* 
 * PQUEUE WRAPPERS
 */
template <typename T>
class MSPriorityQueue : cds::container::MSPriorityQueue<T> {
        typedef cds::container::MSPriorityQueue< T> base_class;
    
    public:
        MSPriorityQueue(size_t nCapacity) : base_class(nCapacity) {};
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};
template <typename T>
class FCPriorityQueue : cds::container::FCPriorityQueue<T> {
        typedef cds::container::FCPriorityQueue<T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};

/* 
 * QUEUE WRAPPERS
 */
template <typename T>
class STOQueue : Queue<T> {
    typedef Queue< T> base_class;
    
    public:
        void pop() { base_class::transPop(); }
        void push(T v) { base_class::transPush(v); }
        size_t size() { return 0; }
};

template <typename T> class CDSQueue{};

template <typename T>
class CDSQueue<cds::container::BasketQueue<cds::gc::HP, T>> {
    public:
        typedef cds::container::BasketQueue<cds::gc::HP, T> base_class;
    private:
        base_class v_;
    public:
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
template <typename T>
class CDSQueue<cds::container::FCQueue<T>> {
    public:
        typedef cds::container::FCQueue<T> base_class;
    private:
        base_class v_;
    public:
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
template <typename T>
class CDSQueue<cds::container::MoirQueue<cds::gc::HP, T>> {
    public:
        typedef cds::container::MoirQueue<cds::gc::HP, T> base_class;
    private:
        base_class v_;
    public:
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
template <typename T>
class CDSQueue<cds::container::MSQueue<cds::gc::HP, T>> {
    public:
        typedef cds::container::MSQueue<cds::gc::HP, T> base_class;
    private:
        base_class v_;
    public:
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
template <typename T>
class CDSQueue<cds::container::OptimisticQueue<cds::gc::HP, T>> {
    public:
        typedef cds::container::OptimisticQueue<cds::gc::HP, T> base_class;
    private:
        base_class v_;
    public:
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
template <typename T> class CDSQueue<cds::container::RWQueue<T>> {
    public:
        typedef cds::container::RWQueue<T> base_class;
    private:
        base_class v_;
    public:
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
template <typename T>
class CDSQueue<cds::container::SegmentedQueue<cds::gc::HP, T>> {
    public:
        typedef cds::container::SegmentedQueue<cds::gc::HP, T> base_class;
    private:
        base_class v_;
    public:
        CDSQueue(size_t nQuasiFactor) : v_(nQuasiFactor) {};
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
template <typename T>
class CDSQueue<cds::container::TsigasCycleQueue<T>> {
    public:
        typedef cds::container::TsigasCycleQueue<T> base_class;
    private:
        base_class v_;
    public:
        CDSQueue(size_t nCapacity) : v_(nCapacity) {};
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
template <typename T>
class CDSQueue<cds::container::VyukovMPMCCycleQueue<T>> {
    public:
        typedef cds::container::VyukovMPMCCycleQueue<T> base_class;
    private:
        base_class v_;
    public:
        CDSQueue(size_t nCapacity) : v_(nCapacity) {};
        void pop() { int ret; v_.pop(ret); }
        void push(T v) { v_.push(v); }
        size_t size() { return v_.size(); }
};
