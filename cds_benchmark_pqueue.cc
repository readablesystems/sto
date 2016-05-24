#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>

#include <cds/init.h>
#include <cds/container/fcpriority_queue.h>
#include <cds/container/mspriority_queue.h>

#include "Transaction.hh"
#include "PriorityQueue.hh"
#include "cds_benchmarks.hh"
#include "randgen.hh"

int global_thread_push_ctrs[N_THREADS];
int global_thread_pop_ctrs[N_THREADS];
void clear_balance_ctrs() {
    for(int i = 0; i < N_THREADS; ++i) {
        global_thread_pop_ctrs[i] = 0;
        global_thread_push_ctrs[i] = 0;
    }
}

// run the specified transaction for the particular benchmark
template <typename T>
void do_txn(Tester<T>* tp, Rand transgen) {
    int me = tp->me;
    int bm = tp->bm;
    size_t size = tp->size;
    T* pq = tp->ds;
    auto txn_set = tp->txn_set;

    // randomly select a transaction to run
    auto txn = txn_set[transgen() % txn_set.size()];

    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    int val = 0;
    for (unsigned j = 0; j < txn.size(); ++j) {
        // figure out what type of value to push into the queue
        // based on which benchmark we are running
        switch(bm) {
            case RANDOM:
                val = slotdist(transgen);
                break;
            case NOABORTS:
            case DECREASING:
                val = --global_val;
                break;
            default:
                break;
        }
        // avoid shrinking/growing the queue too much
        switch(txn[j]) {
            case push:
                if (pq->size() > size*1.5) break;
                pq->push(val);
                global_thread_push_ctrs[me]++;
                break;
            case pop:
                if (pq->size() < size*.5) break;
                pq->pop();
                global_thread_pop_ctrs[me]++;
                break;
            default:
                break;
        }
    }
}

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
void startAndWait(T* ds, int ds_type, int benchmark, std::vector<std::vector<op>> txn_set, size_t size) {
    int nthreads;
    // if we want to avoid all aborts, then only spawn two threads and 
    // have one only push and the other only pop
    if (benchmark == NOABORTS) nthreads = 2;
    else nthreads = N_THREADS;

    pthread_t tids[nthreads];
    Tester<T> testers[nthreads];
    for (int i = 0; i < nthreads; ++i) {
        // set the txn_set to be only pushes or pops if running the NOABORTS bm
        if (benchmark == NOABORTS) testers[i].txn_set = q_txn_sets[i];
        else testers[i].txn_set = txn_set;
        testers[i].ds = ds;
        testers[i].me = i;
        testers[i].bm = benchmark;
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
}

void print_time(struct timeval tv1, struct timeval tv2) {
    fprintf(stderr, "\t%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
    printf("\t%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

void print_abort_stats() {
#if STO_PROFILE_COUNTERS
    if (txp_count >= txp_total_aborts) {
        txp_counters tc = Transaction::txp_counters_combined();

        unsigned long long txc_total_starts = tc.p(txp_total_starts);
        unsigned long long txc_total_aborts = tc.p(txp_total_aborts);
        unsigned long long txc_commit_aborts = tc.p(txp_commit_time_aborts);
        unsigned long long txc_total_commits = txc_total_starts - txc_total_aborts;
        fprintf(stderr, "\t$ %llu starts, %llu max read set, %llu commits",
                txc_total_starts, tc.p(txp_max_set), txc_total_commits);
        if (txc_total_aborts) {
            fprintf(stderr, ", %llu (%.3f%%) aborts",
                    tc.p(txp_total_aborts),
                    100.0 * (double) tc.p(txp_total_aborts) / tc.p(txp_total_starts));
            if (tc.p(txp_commit_time_aborts))
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts at commit time",
                        tc.p(txp_commit_time_aborts),
                        100.0 * (double) tc.p(txp_commit_time_aborts) / tc.p(txp_total_aborts));
        }
        unsigned long long txc_commit_attempts = txc_total_starts - (txc_total_aborts - txc_commit_aborts);
        fprintf(stderr, "\n\t$ %llu commit attempts, %llu (%.3f%%) nonopaque\n",
                txc_commit_attempts, tc.p(txp_commit_time_nonopaque),
                100.0 * (double) tc.p(txp_commit_time_nonopaque) / txc_commit_attempts);
    }
    Transaction::clear_stats();
#endif
}

void print_balance_ctrs(int bm) {
    int nthreads;
    if (bm == NOABORTS) nthreads = 2;
    else nthreads = N_THREADS;
    for (int i = 0; i < nthreads; ++i) {
        fprintf(stderr, "Thread %d \tpushes: %d \tpops: %d\n", 
                i, global_thread_push_ctrs[i], global_thread_pop_ctrs[i]);
    }
}

void run_benchmark(int bm, int size, std::vector<std::vector<op>> txn_set) {
    global_val = INT_MAX;
    // initialize all data structures
    PriorityQueue<int> sto_pqueue;
    PriorityQueue<int, true> sto_pqueue_opaque;
    WrappedFCPriorityQueue<int> fc_pqueue;
    WrappedMSPriorityQueue<int> ms_pqueue(MAX_SIZE);
    Sto::start_transaction();
    for (int i = 0; i < size; i++) {
        sto_pqueue.push(i);
        fc_pqueue.push(i);
        ms_pqueue.push(i);
    }
    assert(Sto::try_commit());

    struct timeval tv1,tv2;
    
    // benchmark FC Pqueue
    gettimeofday(&tv1, NULL);
    startAndWait(&fc_pqueue, CDS, bm, txn_set, size);
    gettimeofday(&tv2, NULL);
    fprintf(stderr, "CDS: FC Priority Queue \tFinal Size %lu", fc_pqueue.size());
    fprintf(stdout, "CDS: FC Priority Queue \tFinal Size %lu", fc_pqueue.size());
    print_time(tv1, tv2);
    print_balance_ctrs(bm);
    clear_balance_ctrs();

    // benchmark MS Pqueue
    gettimeofday(&tv1, NULL);
    startAndWait(&ms_pqueue, CDS, bm, txn_set, size);
    gettimeofday(&tv2, NULL);
    fprintf(stderr, "CDS: MS Priority Queue \tFinal Size %lu", ms_pqueue.size());
    fprintf(stdout, "CDS: MS Priority Queue \tFinal Size %lu", ms_pqueue.size());
    print_time(tv1, tv2);
    print_balance_ctrs(bm);
    clear_balance_ctrs();

    // benchmark STO
    gettimeofday(&tv1, NULL);
    startAndWait(&sto_pqueue, STO, bm, txn_set, size);
    gettimeofday(&tv2, NULL);
    fprintf(stderr, "STO: Priority Queue \tFinal Size %d", sto_pqueue.unsafe_size());
    fprintf(stdout, "STO: Priority Queue \tFinal Size %d", sto_pqueue.unsafe_size());
    print_time(tv1, tv2);
    print_abort_stats();
    print_balance_ctrs(bm);
    clear_balance_ctrs();
    
    // benchmark STO w/Opacity
    gettimeofday(&tv1, NULL);
    startAndWait(&sto_pqueue_opaque, STO, bm, txn_set, size);
    gettimeofday(&tv2, NULL);
    fprintf(stderr, "STO: Priority Queue/O \tFinal Size %d", sto_pqueue_opaque.unsafe_size());
    fprintf(stdout, "STO: Priority Queue/O \tFinal Size %d", sto_pqueue_opaque.unsafe_size());
    print_time(tv1, tv2);
    print_abort_stats();
    print_balance_ctrs(bm);
    clear_balance_ctrs();
}

int main() {
    fprintf(stderr, "nthreads: %d, ntxns/thread: %d, max_value: %d, max_size: %d\n", N_THREADS, NTRANS, MAX_VALUE, MAX_SIZE);
    fprintf(stdout, "nthreads: %d, ntxns/thread: %d, max_value: %d, max_size: %d\n", N_THREADS, NTRANS, MAX_VALUE, MAX_SIZE);

    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh

    cds::Initialize();

    // create the epoch advancer thread
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);

    // iterate through the txn set for all different sizes
    for (auto txn_set = begin(q_txn_sets); txn_set != end(q_txn_sets); ++txn_set) {
        fprintf(stderr, "\n****************TXN SET %ld*****************\n", txn_set-begin(q_txn_sets));
        fprintf(stdout, "\n****************TXN SET %ld*****************\n", txn_set-begin(q_txn_sets));
        for (auto size = begin(sizes); size != end(sizes); ++size) {
            fprintf(stderr, "\nInit size: %d\n", *size);
            fprintf(stdout, "\nInit size: %d\n", *size);
            
            // benchmark with random values. pushes can conflict with pops
            fprintf(stderr, "---------Benchmark: Random-----------\n");
            fprintf(stdout, "---------Benchmark: Random-----------\n");
            run_benchmark(RANDOM, *size, *txn_set);
          
            // benchmark with decreasing values 
            // pushes and pops will never conflict (only pops conflict)
            fprintf(stderr, "---------Benchmark: Decreasing-----------\n");
            fprintf(stdout, "---------Benchmark: Decreasing-----------\n");
            run_benchmark(DECREASING, *size, *txn_set);
        }
    }

    fprintf(stderr, "\nBenchmark: No Aborts (2 threads: one pushing, one popping)\n");
    fprintf(stdout, "\nBenchmark: No Aborts (2 threads: one pushing, one popping)\n");
    for (auto size = begin(sizes); size != end(sizes); ++size) {
        fprintf(stderr, "-------------------------------------------------------------------\n");
        fprintf(stderr, "Init size: %d\n", *size);
        fprintf(stdout, "-------------------------------------------------------------------\n");
        fprintf(stdout, "Init size: %d\n", *size);
        run_benchmark(NOABORTS, *size, {});
    }

    cds::Terminate();
    return 0;
}
