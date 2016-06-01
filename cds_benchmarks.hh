#pragma once

#include <string>
#include <iostream>
#include <cstdarg>
#include <assert.h>
#include <vector>
#include <random>

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
#define N_THREADS 30 // Number of concurrent threads

// type of data structure to be used
#define STO 0
#define CDS 1

// type of benchmark to use
#define RANDOM 10
#define DECREASING 11
#define NOABORTS 12
#define PUSHTHENPOP_RANDOM 13
#define PUSHTHENPOP_DECREASING 14

enum op {push, pop};
enum q_type { basket, fc, moir, ms, optimistic, rw, segmented, tc, vm };

// globals
extern std::atomic_int global_val;
extern std::vector<int> sizes;
extern std::vector<int> nthreads_set;
extern txp_counter_type global_thread_push_ctrs[N_THREADS];
extern txp_counter_type global_thread_pop_ctrs[N_THREADS];
extern txp_counter_type global_thread_skip_ctrs[N_THREADS];

// helper functions
void clear_balance_ctrs(void);
void dualprint(const char* fmt,...);
void print_stats(struct timeval tv1, struct timeval tv2, int bm);
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
    int bm;         // which benchmark to run
    size_t size;    // initial size of the ds 
    std::vector<std::vector<op>> txn_set;
};

template <typename T>
void* run_sto(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    TThread::set_id(tp->me);

    for (int i = 0; i < NTRANS; ++i) {
        auto transseed = i;
        uint32_t seed = transseed*3 + (uint32_t)tp->me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);

        // so that retries of this transaction do the same thing
        Rand transgen_snap = transgen;
        while (1) {
            Sto::start_transaction();
            try {
                do_txn(tp, transgen);
                if (Sto::try_commit()) {
                    break;
                }
            } catch (Transaction::Abort e) {}
            transgen = transgen_snap;
        }
    }
    return nullptr;
}

template <typename T>
void* run_cds(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    cds::threading::Manager::attachThread();

    // generate all transactions the thread will run
    for (int i = 0; i < NTRANS; ++i) {
        auto transseed = i;
        uint32_t seed = transseed*3 + (uint32_t)tp->me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);
        do_txn(tp, transgen);
    }
    cds::threading::Manager::detachThread();
    return nullptr;
}
template <typename T>
void startAndWait(T* ds, int ds_type, 
        int bm, 
        std::vector<std::vector<op>> txn_set, 
        size_t size,
        int nthreads) {

    pthread_t tids[nthreads];
    Tester<T> testers[nthreads];
    for (int i = 0; i < nthreads; ++i) {
        // set the txn_set to be only pushes or pops if running the NOABORTS bm
        if (bm == NOABORTS) {
            testers[0].txn_set = q_push_only_txn_set;
            testers[1].txn_set = q_pop_only_txn_set;
        }
        else testers[i].txn_set = txn_set;
        testers[i].ds = ds;
        testers[i].me = i;
        testers[i].bm = bm;
        testers[i].size = size;
        if (ds_type == CDS) {
            pthread_create(&tids[i], NULL, run_cds<T>, &testers[i]);
        } else {
            pthread_create(&tids[i], NULL, run_sto<T>, &testers[i]);
        }
    }
    for (int i = 0; i < nthreads; ++i) {
        pthread_join(tids[i], NULL);
    }

    if (bm == PUSHTHENPOP_RANDOM || bm == PUSHTHENPOP_DECREASING) { 
        while (ds->size() != 0) {
    	    Sto::start_transaction();
            ds->pop();
	        // so that totals are correct
            global_thread_pop_ctrs[0]++;
            assert(Sto::try_commit());
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
        size_t size() { return -1; } // no size of STO queue
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
        size_t size() { return base_class::size(); }
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
        size_t size() { return base_class::size(); }
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
        size_t size() { return base_class::size(); }
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
        size_t size() { return base_class::size(); }
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
        size_t size() { return base_class::size(); }
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
        size_t size() { return base_class::size(); }
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
        size_t size() { return base_class::size(); }
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
        size_t size() { return base_class::size(); }
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
        size_t size() { return base_class::size(); }
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
        size_t size() {
            switch(_qtype) {
                case basket: return basket_queue.size(); 
                case fc: return fc_queue.size(); 
                case moir: return moir_queue.size(); 
                case ms: return  ms_queue.size(); 
                case optimistic: return optimistic_queue.size(); 
                case rw: return rw_queue.size(); 
                case segmented: segmented_queue.size(); 
                case tc: return tc_queue.size(); 
                case vm: return vm_queue.size(); 
                default: return -1;
            };
        };
};


