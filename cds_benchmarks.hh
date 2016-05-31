#pragma once

#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <climits>

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
void* run_sto(void* x);
void* run_cds(void* x);
template <typename T>
void startAndWait(
        T* ds, 
        int ds_type, 
        int bm, 
        std::vector<std::vector<op>> txn_set, 
        size_t size, 
        int nthreads);

// txn sets
extern std::vector<std::vector<op>> q_single_op_txn_set;
extern std::vector<std::vector<op>> q_push_only_txn_set;
extern std::vector<std::vector<op>> q_pop_only_txn_set;
extern std::vector<std::vector<op>> q_txn_sets[];

template <typename T>
struct Tester {
    T* ds;          // pointer to the data structure
    int me;         // tid
    int ds_type;    // cds or sto
    int bm;         // which benchmark to run
    size_t size;    // initial size of the ds 
    std::vector<std::vector<op>> txn_set;
};

/* 
 * PQUEUE WRAPPERS
 */
template <typename T>
class WrappedMSPriorityQueue : cds::container::MSPriorityQueue<T> {
        typedef cds::container::MSPriorityQueue< T> base_class;
    
    public:
        WrappedMSPriorityQueue(size_t nCapacity) : base_class(nCapacity) {};
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};
template <typename T>
class WrappedFCPriorityQueue : cds::container::FCPriorityQueue<T> {
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
        void pop() {
            base_class::transpop();
        }
        void push(T v) { base_class::transpush(v); }
        size_t size() { return -1; } // no size of STO queue
};

// wrapper class for all the CDS queues
class CDSQueue {};

template <typename T>
class BasketQueue : cds::container::BasketQueue<cds::gc::HP, T>, CDSQueue {
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
class FCQueue : cds::container::FCQueue<cds::gc::HP, T>, CDSQueue {
    typedef cds::container::FCQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};
template <typename T>
class MoirQueue : cds::container::MoirQueue<cds::gc::HP,T>, CDSQueue {
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
class MSQueue : cds::container::MSQueue<cds::gc::HP,T>, CDSQueue {
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
class OptimisticQueue : cds::container::OptimisticQueue<cds::gc::HP, T>, CDSQueue {
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
class RWQueue : cds::container::RWQueue<cds::gc::HP,T>, CDSQueue {
    typedef cds::container::RWQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};
template <typename T>
class SegmentedQueue : cds::container::SegmentedQueue<cds::gc::HP,T>, CDSQueue {
    typedef cds::container::SegmentedQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};
template <typename T>
class TsigasCycleQueue : cds::container::TsigasCycleQueue<cds::gc::HP,T>, CDSQueue {
    typedef cds::container::TsigasCycleQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};
template <typename T>
class VyukovMPMCCycleQueue : cds::container::VyukovMPMCCycleQueue<cds::gc::HP,T>, CDSQueue {
    typedef cds::container::TsigasCycleQueue<cds::gc::HP, T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};
