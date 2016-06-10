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
#define INITIAL_THREAD 0 // Maximum number of concurrent threads

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
std::atomic_int global_push_val(MAX_VALUE);
std::vector<int> init_sizes = {1000, 10000, 50000, 100000, 150000};
std::vector<int> nthreads_set = {1, 2, 4, 8, 12, 16, 20, 24};

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
void dualprint(const char* fmt,...); // prints to both the verbose and csv files
void print_abort_stats(void);        // prints how many aborts/commits, etc. for STO ds

template <typename T>
void startAndWait(Test* test, size_t size, int nthreads);

/*
 * Datatype Harnesses
 */
template <typename DS> DatatypeHarness{};

template <typename DS> CDSDatatypeHarness {
    typedef typename value_type DS::value_type;
public:
    CDSDatatypeHarness() {};
    CDSDatatypeHarness(size_t nCapacity) {};
    void pop() { int ret; v_::pop(ret); }
    void push(T v) { v_::push(v); }
    void init_push(T v) { v_::push(v); }
    size_t size() { return v_::size(); }
private:
    DS v_;
}

/* 
 * Priority Queue Templates
 */
template <T> DatatypeHarness<cds::container::MSPriorityQueue<T>> : public CDSDatatypeHarness<cds::container::MSPriorityQueue<T>>;
template <T> DatatypeHarness<cds::container::FCPriorityQueue<T>> : public CDSDatatypeHarness<cds::container::FCPriorityQueue<T>>;
template <T> DatatypeHarness<PriorityQueue<T>> {
    typedef typename value_type T;
public:
    DatatypeHarness() {};
    void pop() { v_::pop(); }
    void push(value_type v) { v_::push(v); }
    void init_push(value_type v) { v_::push(v); }
    size_t size() { return v_::unsafe_size(); }
private:
    PriorityQueue<T> v_;
}
template <T> DatatypeHarness<PriorityQueue<T, true>> {
    typedef typename value_type T;
public:
    DatatypeHarness() {};
    void pop() { v_::pop(); }
    void push(value_type v) { v_::push(v); }
    void init_push(value_type v) { v_::push(v); }
    size_t size() { return v_::unsafe_size(); }
private:
    PriorityQueue<T, true> v_;
}
/* 
 * Queue Templates
 */
template <T> DatatypeHarness<Queue<T>> {
    typedef typename value_type T;
public:
    void pop() { v_.transPop(); }
    void push(T v) { v_.transPush(v); }
    size_t size() { return 0; }
private:
    Queue<T> v_;
}
template <T> DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, T>> : public CDSDatatypeHarness<cds::container::BasketQueue<cds::gc::HP, T>>;
template <T> DatatypeHarnes<cds::container::FCQueue<T>> : public CDSDatatypeHarness<cds::container::FCQueue<T>>;
template <T> DatatypeHarness<cds::container::MoirQueue<cds::gc::HP, T>> : public CDSDatatypeHarness<cds::container::MoirQueue<cds::gc::HP, T>>;
template <T> DatatypeHarness<cds::container::MSQueue<cds::gc::HP, T>> : public CDSDatatypeHarness<cds::container::MSQueue<cds::gc::HP, T>>;
template <T> DatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, T>> : public CDSDatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, T>>:
template <T> DatatypeHarness<cds::container::RWQueue<T>> : public CDSDatatypeHarness<RWQueue<T>>:
template <T> DatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, T>> : public CDSDatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, T>
template <T> DatatypeHarness<cds::container::TsigasCycleQueue<T>> : public CDSDatatypeHarness<cds::container::TsigasCycleQueue<T>>;
template <T> DatatypeHarness<cds::container::VyukovMPMCCycleQueue<T>> : public CDSDatatypeHarness<cds::container::VyukovMPMCCycleQueue<T>>;

Tester {
    int me; // tid
    int nthreads;
    size_t size;
    GenericTest* test;   
}

void* test_thread(void *data) {
    GenericTest *gt = (Tester*)data->test;
    int me = (Tester*)data->me;
    int nthreads = (Tester*)data->nthreads;
    size_t size = (Tester*)data->size;

    if (me == INITIAL_THREAD) {
        gt->initialize(size); 
    }
    
    spawned_barrier++;
    while (spawned_barrier != nthreads) {
        sched_yield();
    }
   
    gt->run();

    spawned_barrier--;
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
    while (spawned == nthreads) {
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
    fprintf(global_verbose_stats_file, "\n");
    for (int i = 0; i < MAX_NUM_THREADS; ++i) {
        // prints the number of pushes, pops, and skips
        fprintf(global_verbose_stats_file, "Thread %d \tpushes: %ld \tpops: %ld, \tskips: %ld\n", i, 
                global_thread_push_ctrs[i], 
                global_thread_pop_ctrs[i], 
                global_thread_skip_ctrs[i]);
    }
    dualprint("%f, ", ops_per_ms);
    return nullptr;
}

void print_benchmark(int bm, int val_type, int size, int nthreads) {
    char* val;
    switch (val_type) {
        case RANDOM_VALS: val = (char*)"random"; break;
        case DECREASING_VALS: val = (char*)"decreasing"; break;
        default: assert(0);
    }
    switch(bm) {
        case PUSHPOP: 
            fprintf(global_verbose_stats_file, "-------2 THREAD PUSH/POP TEST, %s VALUES, INIT SIZE %d, NTHREADS %d--------\n", 
                    val, size, nthreads);
        break;
        case PUSHONLY:
            fprintf(global_verbose_stats_file, "-------PUSH-ONLY TEST, %s VALUES, INIT SIZE %d, NTHREADS %d--------\n", 
                    val, size, nthreads);
        break;
        case RANDOMOP:
            fprintf(global_verbose_stats_file, "-------RANDOM OP TEST, %s VALUES, INIT SIZE %d, NTHREADS %d--------\n", 
                    val, size, nthreads);
        break;
        case GENERAL:
            fprintf(global_verbose_stats_file, "-------GENERAL TXNS TEST, %s VALUES, INIT SIZE %d, NTHREADS %d--------\n", 
                    val, size, nthreads);
        break;
        default: assert(0);
    }
}

void run_queue_benchmark(int bm, int val_type, int size, std::vector<std::vector<op>> txn_set, int nthreads) {
    print_benchmark(bm, val_type, size, nthreads);
    global_push_val = MAX_VALUE;
   
    // initialize all data structures
    STOQueue<int>sto_queue;
    CDSQueue<cds::container::BasketQueue<cds::gc::HP, int>> q1;
    CDSQueue<cds::container::FCQueue<int>> q2;
    CDSQueue<cds::container::MoirQueue<cds::gc::HP, int>> q3;
    CDSQueue<cds::container::MSQueue<cds::gc::HP, int>> q4;
    CDSQueue<cds::container::OptimisticQueue<cds::gc::HP, int>> q5;
    CDSQueue<cds::container::RWQueue<int>> q6;
    CDSQueue<cds::container::SegmentedQueue<cds::gc::HP, int>> q7(32);
    CDSQueue<cds::container::TsigasCycleQueue<int>> q8(1000000);
    CDSQueue<cds::container::VyukovMPMCCycleQueue<int>> q9(1000000);

    // XXX std::vector<CDSQueue<int>*> cds_queues;
    for (int i = 0; i < size; ++i) {
        Sto::start_transaction();
        sto_queue.push(i);
        assert(Sto::try_commit());
        q1.push(i); q2.push(i); q3.push(i);
        q4.push(i); q5.push(i); q6.push(i);
        q7.push(i); q8.push(i); q9.push(i);
    }
    startAndWait(&sto_queue, STO, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q1, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q2, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q3, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q4, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q5, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q6, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q7, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q8, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&q9, CDS, bm, val_type, txn_set, size, nthreads);
    dualprint("\n");
}

void run_pqueue_benchmark(int bm, int val_type, int size, std::vector<std::vector<op>> txn_set, int nthreads) {
    global_push_val = MAX_VALUE;
    print_benchmark(bm, val_type, size, nthreads);

    // initialize all data structures
    PriorityQueue<int> sto_pqueue;
    PriorityQueue<int, true> sto_pqueue_opaque;
    FCPriorityQueue<int> fc_pqueue;
    MSPriorityQueue<int> ms_pqueue(MAX_SIZE);
    for (int i = global_push_val; i < size; ++i) {
        Sto::start_transaction();
        sto_pqueue.push(global_push_val);
        assert(Sto::try_commit());
        fc_pqueue.push(global_push_val);
        ms_pqueue.push(global_push_val);
        global_push_val--;
    }
   
    startAndWait(&fc_pqueue, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&ms_pqueue, CDS, bm, val_type, txn_set, size, nthreads);
    startAndWait(&sto_pqueue, STO, bm, val_type, txn_set, size, nthreads);
    startAndWait(&sto_pqueue_opaque, STO, bm, val_type, txn_set, size, nthreads);
    dualprint("\n");
}

void run_all_queue_benchmarks() {
    dualprint("\nQUEUE BENCHMARKS: STO, Basket, FC, Moir, MS, Optimistic, RW, Segmented, TsigasCycle, VyukovMPMCCycle\n");
    // run the different txn set workloads with different initial init_sizes
    // test both random and decreasing-only values
    /*
    for (auto txn_set = begin(q_txn_sets); txn_set != end(q_txn_sets); ++txn_set) {
        for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
            run_queue_benchmark(GENERAL, RANDOM_VALS, *size, *txn_set, MAX_NUM_THREADS);
            run_queue_benchmark(GENERAL, DECREASING_VALS, *size, *txn_set, MAX_NUM_THREADS);
        }
    }
    */
    // run the two-thread test where one thread only pushes and the other only pops
    dualprint("\nQUEUE PUSHPOP\n");
    for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
	    run_queue_benchmark(PUSHPOP, RANDOM_VALS, *size, {}, 2);
        fprintf(stderr, "Done running pushpop with size %d\n", *size);
    }
    // run single-operation txns with different nthreads
    for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
    	for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
            run_queue_benchmark(RANDOMOP, RANDOM_VALS, *size, {}, *n);
            fprintf(stderr, "Done running randomop random with size %d nthreads %d\n", *size, *n);
        }
    }
}

void run_all_pqueue_benchmarks() {
    dualprint("\nPQUEUE BENCHMARKS: FC, MS, STO, STO(O)\n");
    // run the different txn set workloads with different initial init_sizes
    // test both random and decreasing-only values
    /*
    for (auto txn_set = begin(q_txn_sets); txn_set != end(q_txn_sets); ++txn_set) {
        for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
            run_pqueue_benchmark(GENERAL, RANDOM_VALS, *size, *txn_set, MAX_NUM_THREADS);
            run_pqueue_benchmark(GENERAL, DECREASING_VALS, *size, *txn_set, MAX_NUM_THREADS);
            fprintf(stderr, "Done running general txn set with size %d nthreads %d", *size, MAX_NUM_THREADS);
        }
    }
    // run the two-thread test where one thread only pushes and the other only pops
    dualprint("\nPQUEUE PUSHPOP\n");
    for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
	    run_pqueue_benchmark(PUSHPOP, RANDOM_VALS, *size, {}, 2);
        fprintf(stderr, "Done running pushpop with size %d\n", *size);
    }
    // run the push-only test
    dualprint("\nPQUEUE PUSHONLY\n");
    for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
        run_pqueue_benchmark(PUSHONLY, RANDOM_VALS, 100000, {}, *n);
        fprintf(stderr, "Done running pushonly random with nthreads %d\n", *n);
    }
    for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
        run_pqueue_benchmark(PUSHONLY, DECREASING_VALS, 100000, {}, *n);
        fprintf(stderr, "Done running pushonly decreasing with nthreads %d\n", *n);
    }*/
    // run single-operation txns with different nthreads
    dualprint("\nPQUEUE RANDOMOP RANDOM\n");
    for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
    	for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
            run_pqueue_benchmark(RANDOMOP, RANDOM_VALS, *size, {}, *n);
            fprintf(stderr, "Done running randomop random with size %d nthreads %d\n", *size, *n);
        }
    }
    dualprint("\nPQUEUE RANDOMOP DECREASING\n");
    for (auto size = begin(init_sizes); size != end(init_sizes); ++size) {
    	for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
            run_pqueue_benchmark(RANDOMOP,DECREASING_VALS, *size, {}, *n);
            fprintf(stderr, "Done running randomop decreasing with size %d nthreads %d\n", *size, *n);
        }
    }
}

void dualprint(const char* fmt,...) {
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

int main() {
    global_verbose_stats_file = fopen("cds_benchmarks_stats_verbose.txt", "a");
    global_stats_file = fopen("cds_benchmarks_stats.txt", "a");
    if ( !global_stats_file || !global_verbose_stats_file ) {
        fprintf(stderr, "Could not open file to write stats");
        return 1;
    }
    dualprint("\n************NEW CDS BENCHMARK RUN************\n");

    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh

    cds::Initialize();
    cds::gc::HP hpGC;
    cds::threading::Manager::attachThread();

    // create the epoch advancer thread
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);

    //run_all_queue_benchmarks();
    run_all_pqueue_benchmarks();

    cds::threading::Manager::detachThread();
    cds::Terminate();
    return 0;
}

// txn sets
std::vector<std::vector<std::vector<op>>> q_txn_sets = 
{
    // 0. short txns
    {
        {push, push, push},
        {pop, pop, pop},
        {pop}, {pop}, {pop},
        {push}, {push}, {push}
    },
    // 1. longer txns
    {
        {push, push, push, push, push},
        {pop, pop, pop, pop, pop},
        {pop}, {pop}, {pop}, {pop}, {pop}, 
        {push}, {push}, {push}, {push}, {push}
    },
    // 2. 100% include both pushes and ptps
    {
        {push, push, pop},
        {pop, pop, push},
    },
    // 3. 50% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
        {pop}, {push}
    },
    // 4. 33% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
        {pop}, {pop},
        {push}, {push}
    },
    // 5. 33%: longer push + pop txns
    {
        {push, pop, push, pop, push, pop},
        {pop}, 
        {push}
    }
};

