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

#define MAX_VALUE  INT_MAX
#define MAX_SIZE 1000000
#define NTRANS 20000 // Number of transactions each thread should run.
#define N_THREADS 24 // Number of concurrent threads

// type of data structure to be used
#define STO 0
#define CDS 1

// type of benchmark to use
#define RANDOM 10
#define DECREASING 11
#define PUSHPOP 12
#define PUSHONLY 13
#define GENERAL 14

enum op {push, pop};
enum q_type { basket, fc, moir, ms, optimistic, rw, segmented, tc, vm };

// globals
extern std::atomic_int global_val;
extern std::vector<int> sizes;
extern std::vector<int> nthreads_set;
extern txp_counter_type global_thread_push_ctrs[N_THREADS];
extern txp_counter_type global_thread_pop_ctrs[N_THREADS];
extern txp_counter_type global_thread_skip_ctrs[N_THREADS];

extern std::atomic_int spawned;
extern std::mutex run_mtx;
extern std::mutex spawned_mtx;
extern std::condition_variable global_run_cv;
extern std::condition_variable global_spawned_cv;

// helper functions
void* record_perf_thread(void*);
void clear_op_ctrs(void);
void dualprint(const char* fmt,...);
void print_abort_stats(void);

// txn sets
extern std::vector<std::vector<op>> q_single_op_txn_set;
extern std::vector<std::vector<op>> q_push_only_txn_set;
extern std::vector<std::vector<op>> q_pop_only_txn_set;
extern std::vector<std::vector<std::vector<op>>> q_txn_sets;

template <typename T>
struct Tester {
    T* ds;          // pointer to the data structure
    int me;         // tid
    int ds_type;    // cds or sto
    size_t size;    // initial size of the ds 
    std::vector<std::pair<std::vector<op>, int>> txns_to_run;
};

template <typename T>
void* run_sto(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    TThread::set_id(tp->me);

    auto txns_to_run = tp->txns_to_run;
    for (auto txn = begin(txns_to_run); txn != end(txns_to_run); ++txn) {
        std::vector<op> txn_ops = txn->first;
        int txn_val = txn->second;
        while (1) {
            Sto::start_transaction();
            try {
                do_txn(tp->ds, tp->me, txn_ops, txn_val, tp->size);
                if (Sto::try_commit()) {
                    break;
                }
            } catch (Transaction::Abort e) {}
        }
    }

    return nullptr;
}
template <typename T>
void* run_cds(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    cds::threading::Manager::attachThread();
    auto txns_to_run = tp->txns_to_run;
    for (auto txn = begin(txns_to_run); txn != end(txns_to_run); ++txn) {
        std::vector<op> txn_ops = txn->first;
        int txn_val = txn->second;
        do_txn(tp->ds, tp->me, txn_ops, txn_val, tp->size);
    }
    cds::threading::Manager::detachThread();
    return nullptr;
}
template <typename T>
void* run_sto_random_op_thread(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    auto q = tp->ds;
    TThread::set_id(tp->me);
    
    spawned++; 
    global_spawned_cv.notify_one();
    
    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();

    for (int i = NTRANS; i > 0; --i) {
        auto transseed = i;
        uint32_t seed = transseed*3 + (uint32_t)tp->me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);
        auto transgen_snap = transgen;
    
        while (1) {
            Sto::start_transaction();
            try {
                if (transgen() % 2 == 0) {
                    q->push(i);
                    global_thread_push_ctrs[tp->me]++;
                } else {
                    q->pop();
                    global_thread_pop_ctrs[tp->me]++;
                }
                if (Sto::try_commit()) {
                    break;
                }
            }
            catch (Transaction::Abort e) {
                transgen = transgen_snap;
            }
        }
    }

    return nullptr;
}
template <typename T>
void* run_cds_random_op_thread(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    auto q = tp->ds;
    cds::threading::Manager::attachThread();
    
    spawned++; 
    global_spawned_cv.notify_one();

    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();
    
    for (int i = NTRANS; i > 0; --i) {
        auto transseed = i;
        uint32_t seed = transseed*3 + (uint32_t)tp->me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);

        if (transgen() % 2 == 0) {
            q->push(i);
            global_thread_push_ctrs[tp->me]++;
        } else {
            q->pop();
            global_thread_pop_ctrs[tp->me]++;
        }
    }

    cds::threading::Manager::detachThread();
    return nullptr;
}
template <typename T>
void* run_sto_push_thread(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    auto q = tp->ds;
    TThread::set_id(tp->me);
  
    spawned++; 
    global_spawned_cv.notify_one();
 
    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();
    

    for (int i = NTRANS; i > 0; --i) {
        while (1) {
            Sto::start_transaction();
            try {
                q->push(i);
                if (Sto::try_commit()) {
                    break;
                }
            } catch (Transaction::Abort e) {}
        }
        global_thread_push_ctrs[tp->me]++;
    }

    return nullptr;
}
template <typename T>
void* run_sto_pop_thread(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    auto q = tp->ds;
    TThread::set_id(tp->me);
  
    spawned++; 
    global_spawned_cv.notify_one();
  
    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();

    for (int i = NTRANS; i > 0; --i) {
        while (1) {
            Sto::start_transaction();
            try {
                q->pop();
                if (Sto::try_commit()) {
                    break;
                }
            } catch (Transaction::Abort e) {}
        }
        global_thread_pop_ctrs[tp->me]++;
    }
    
    return nullptr;
}
template <typename T>
void* run_cds_push_thread(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    auto q = tp->ds;
    cds::threading::Manager::attachThread();
  
    spawned++; 
    global_spawned_cv.notify_one();
  
    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();

    for (int i = NTRANS; i > 0; --i) {
        q->push(i);
        global_thread_push_ctrs[tp->me]++;
    }

    cds::threading::Manager::detachThread();
    return nullptr;
}
template <typename T>
void* run_cds_pop_thread(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    auto q = tp->ds;
    cds::threading::Manager::attachThread();
  
    spawned++; 
    global_spawned_cv.notify_one();
  
    std::unique_lock<std::mutex> run_lk(run_mtx);
    global_run_cv.wait(run_lk);
    run_lk.unlock();

    for (int i = NTRANS; i > 0; --i) {
        q->pop();
        global_thread_pop_ctrs[tp->me]++;
    }

    cds::threading::Manager::detachThread();
    return nullptr;
}

template <typename T>
void startAndWait(T* ds, int ds_type, int bm, 
        std::vector<std::vector<op>> txn_set, 
        size_t size, int nthreads) {

    pthread_t tids[nthreads+1];
    Tester<T> testers[nthreads];
    int val;
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    
    /* 
     * Allocate the operations and values for all threads to run.
     * Only allow pushes or pops if running the PUSHPOP bm.
     */
    for (int me = 0; me < nthreads; ++me) {
        for (int j = 0; j < NTRANS; ++j) {
            auto transseed = j;
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);

            switch(bm) {
                case RANDOM:
                    val = slotdist(transgen);
                    break;
                default: // PUSHPOP, DECREASING
                    val = --global_val;
                    break;
            }
            // randomly select an operation to run unless we're doing the push/pop noaborts test
            if (bm == GENERAL) {
                auto txn = txn_set[transgen() % txn_set.size()];
                testers[me].txns_to_run.push_back(std::make_pair(txn, val));
            }
        }
    }
    for (int i = 0; i < nthreads; ++i) {
        testers[i].ds = ds;
        testers[i].me = i;
        testers[i].size = size;
    }
  
    /* 
     * Create the thread to track performance and print stats
     */
    pthread_create(&tids[nthreads], NULL, record_perf_thread, NULL);
      
    /* 
     * Run the threads that do the operations.
     */
    if (bm == PUSHPOP) {
        if (ds_type == CDS) {
            pthread_create(&tids[0], NULL, run_cds_pop_thread<T>, &testers[0]);
            pthread_create(&tids[1], NULL, run_cds_push_thread<T>, &testers[1]);
        } else {
            pthread_create(&tids[0], NULL, run_sto_pop_thread<T>, &testers[0]);
            pthread_create(&tids[1], NULL, run_sto_push_thread<T>, &testers[1]);
        }
    } else if (bm == PUSHONLY) {
        for (int i = 0; i < nthreads; ++i) {
            if (ds_type == CDS) pthread_create(&tids[i], NULL, run_cds_push_thread<T>, &testers[i]);
            else pthread_create(&tids[i], NULL, run_sto_push_thread<T>, &testers[i]);
        }
    } else {
        for (int i = 0; i < nthreads; ++i) {
            if (ds_type == CDS) pthread_create(&tids[i], NULL, run_cds_random_op_thread<T>, &testers[i]);
            else pthread_create(&tids[i], NULL, run_sto_random_op_thread<T>, &testers[i]);
        }
    }
  
    /* 
     * Wait until all threads are ready, then tell all threads to begin execution.
    */ 
    std::unique_lock<std::mutex> spawned_lk(spawned_mtx);
    global_spawned_cv.wait(spawned_lk, [&nthreads]{return spawned==nthreads;});
    spawned_lk.unlock();
    sleep(0.01*nthreads);
    global_run_cv.notify_all();
    
    for (int i = 0; i < nthreads+1; ++i) {
        pthread_join(tids[i], NULL);
    }
    if (ds_type == STO) print_abort_stats();

    clear_op_ctrs();
    spawned = 0;
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
};

template <typename T>
class BasketQueue : cds::container::BasketQueue<cds::gc::HP, T> {
   typedef cds::container::BasketQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};
template <typename T>
class FCQueue : cds::container::FCQueue<T> {
    typedef cds::container::FCQueue<T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};
template <typename T>
class MoirQueue : cds::container::MoirQueue<cds::gc::HP,T> {
    typedef cds::container::MoirQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};
template <typename T>
class MSQueue : cds::container::MSQueue<cds::gc::HP,T> {
    typedef cds::container::MSQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};
template <typename T>
class OptimisticQueue : cds::container::OptimisticQueue<cds::gc::HP, T> {
    typedef cds::container::OptimisticQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};
template <typename T>
class RWQueue : cds::container::RWQueue<T> {
    typedef cds::container::RWQueue<T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};
template <typename T>
class SegmentedQueue : cds::container::SegmentedQueue<cds::gc::HP,T> {
    typedef cds::container::SegmentedQueue<cds::gc::HP, T> base_class;

    public:
        SegmentedQueue(size_t nQuasiFactor) : base_class(nQuasiFactor) {};
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};
template <typename T>
class TsigasCycleQueue : cds::container::TsigasCycleQueue<T> {
    typedef cds::container::TsigasCycleQueue<T> base_class;

    public:
        TsigasCycleQueue(size_t nCapacity) : base_class(nCapacity) {};
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};
template <typename T>
class VyukovMPMCCycleQueue : cds::container::VyukovMPMCCycleQueue<T> {
    typedef cds::container::VyukovMPMCCycleQueue<T> base_class;

    public:
        VyukovMPMCCycleQueue(size_t nCapacity) : base_class(nCapacity) {};
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
};

// wrapper class for all the CDS queues
template <typename T>
class CDSQueue {
    private:
        int _qtype;
        BasketQueue<T> basket_queue;
        FCQueue<T> fc_queue;
        MoirQueue<T> moir_queue;
        MSQueue<T> ms_queue;
        OptimisticQueue<T> optimistic_queue;
        RWQueue<T> rw_queue;
        SegmentedQueue<T> segmented_queue;
        TsigasCycleQueue<T> tc_queue;
        VyukovMPMCCycleQueue<T> vm_queue;    
    
    public:
        CDSQueue(q_type qtype) : _qtype(qtype), segmented_queue(MAX_SIZE), tc_queue(4096), vm_queue(4096) {};
        void pop() {
            switch(_qtype) {
                case basket: basket_queue.pop(); break;
                case fc: fc_queue.pop(); break;
                case moir: moir_queue.pop(); break;
                case ms:  ms_queue.pop(); break;
                case optimistic: optimistic_queue.pop(); break;
                case rw: rw_queue.pop(); break;
                case segmented: segmented_queue.pop(); break;
                case tc: tc_queue.pop(); break;
                case vm: vm_queue.pop(); break;
                default: break;
            };
        };
        void push(T v) {
            switch(_qtype) {
                case basket: basket_queue.push(v); break;
                case fc: fc_queue.push(v); break;
                case moir: moir_queue.push(v); break;
                case ms:  ms_queue.push(v); break;
                case optimistic: optimistic_queue.push(v); break;
                case rw: rw_queue.push(v); break;
                case segmented: segmented_queue.push(v); break;
                case tc: tc_queue.push(v); break;
                case vm: vm_queue.push(v); break;
                default: break;
            };
        };
};


