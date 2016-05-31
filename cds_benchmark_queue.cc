#include <string>
#include <iostream>
#include <cstdarg>
#include <assert.h>
#include <vector>
#include <random>

#include <cds/init.h>
#include <cds/container/basket_queue.h>
#include <cds/container/fcqueue.h>
#include <cds/container/moir_queue.h>
#include <cds/container/msqueue.h>
#include <cds/container/rwqueue.h>
#include <cds/container/segmented_queue.h>
#include <cds/container/tsigas_cycle_queue.h>
#include <cds/container/vyukov_mpmc_cycle_queue.h>

#include "Transaction.hh"
#include "Queue.hh"
#include "cds_benchmarks.hh"
#include "randgen.hh"

// run the specified transaction for the particular benchmark
template <typename T>
void do_txn(Tester<T>* tp, Rand transgen) {
    int me = tp->me;
    int bm = tp->bm;
    size_t size = tp->size;
    T* q = tp->ds;
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
            case PUSHTHENPOP_RANDOM:
                val = slotdist(transgen);
                break;
            default: // NOABORTS, PUSHTHENPOP_D, or DECREASING
                val = --global_val;
                break;
        }
        // avoid shrinking/growing the queue too much
        // but not for the push-only test
        switch(txn[j]) {
            case push:
                if (bm != PUSHTHENPOP_RANDOM && bm != PUSHTHENPOP_DECREASING) {
                    if (q->size() > size*1.5) {
                        global_thread_skip_ctrs[me]++;
                        break;
                    }
                }
                q->push(val);
                global_thread_push_ctrs[me]++;
                break;
            case pop:
                if (q->size() < size*.5) break;
                q->pop();
                global_thread_pop_ctrs[me]++;
                break;
            default:
                break;
        }
    }
}

void run_benchmark(int bm, int size, std::vector<std::vector<op>> txn_set, int nthreads) {
    global_val = MAX_VALUE;
    
    // initialize all data structures
    STOQueue<int> sto_queue;
    BasketQueue<int> basket_queue;
    FCQueue<int> fc_queue;
    MoirQueue<int> moir_queue;
    MSQueue<int> ms_queue;
    OptimisticQueue<int> optimistic_queue;
    RWQueue<int> rw_queue;
    SegmentedQueue<int> segmented_queue;
    TsigasCycleQueue<int> tc_queue;
    VyukovMPMCCycleQueue<int> vm_queue;

    std::vector<CDSQueue> cds_queues = {
        basket_queue,
        fc_queue,
        moir_queue,
        ms_queue,
        optimistic_queue,
        rw_queue,
        segmented_queue,
        tc_queue,
        vm_queue,
    };

    for (int i = global_val; i < size; ++i) {
    Sto::start_transaction();
        sto_queue.push(global_val--);
    assert(Sto::try_commit());
        fc_queue.push(global_val--);
        ms_queue.push(global_val--);
    }

    struct timeval tv1,tv2;
    
    // benchmark FC queue
    gettimeofday(&tv1, NULL);
    startAndWait(&fc_queue, CDS, bm, txn_set, size, nthreads);
    gettimeofday(&tv2, NULL);
    //dualprint("CDS: FC Priority Queue \tFinal Size %lu", fc_queue.size());
    print_stats(tv1, tv2, bm);
    clear_balance_ctrs();

    // benchmark MS queue
    gettimeofday(&tv1, NULL);
    startAndWait(&ms_queue, CDS, bm, txn_set, size, nthreads);
    gettimeofday(&tv2, NULL);
    //dualprint("CDS: MS Priority Queue \tFinal Size %lu", ms_queue.size());
    print_stats(tv1, tv2, bm);
    clear_balance_ctrs();

    // benchmark STO
    gettimeofday(&tv1, NULL);
    startAndWait(&sto_queue, STO, bm, txn_set, size, nthreads);
    gettimeofday(&tv2, NULL);
    //dualprint("STO: Priority Queue \tFinal Size %d", sto_queue.unsafe_size());
    print_stats(tv1, tv2, bm);
    print_abort_stats();
    clear_balance_ctrs();
    
    // benchmark STO w/Opacity
    gettimeofday(&tv1, NULL);
    startAndWait(&sto_queue_opaque, STO, bm, txn_set, size, nthreads);
    gettimeofday(&tv2, NULL);
    //dualprint("STO: Priority Queue/O \tFinal Size %d", sto_queue_opaque.unsafe_size());
    print_stats(tv1, tv2, bm);
    print_abort_stats();
    clear_balance_ctrs();
}

int main() {
    dualprint("nthreads: %d, ntxns/thread: %d, max_value: %d, max_size: %d\n", N_THREADS, NTRANS, MAX_VALUE, MAX_SIZE);

    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh

    cds::Initialize();

    // create the epoch advancer thread
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);

    /*
    // run the different txn set workloads with different initial sizes
    // test both random and decreasing-only values
    for (auto txn_set = begin(q_txn_sets); txn_set != end(q_txn_sets); ++txn_set) {
        dualprint("\n****************TXN SET %ld*****************\n", txn_set-begin(q_txn_sets));
        for (auto size = begin(sizes); size != end(sizes); ++size) {
            dualprint("\nInit size: %d\n", *size);
            
            // benchmark with random values. pushes can conflict with pops
            dualprint("---------Benchmark: Random-----------\n");
            run_benchmark(RANDOM, *size, *txn_set, N_THREADS);
          
            // benchmark with decreasing values 
            // pushes and pops will never conflict (only pops conflict)
            fprintf(stderr, "---------Benchmark: Decreasing-----------\n");
            fprintf(stdout, "---------Benchmark: Decreasing-----------\n");
            run_benchmark(DECREASING, *size, *txn_set, N_THREADS);
        }
    }
   
    // run the two-thread test where one thread only pushes and the other only pops
    dualprint("\nBenchmark: No Aborts (2 threads: one pushing, one popping)\n");
    for (auto size = begin(sizes); size != end(sizes); ++size) {
        dualprint("Init size: %d\n", *size);
        run_benchmark(NOABORTS, *size, {}, 2);
    }
    */
   
    // run the push-only test (with single-thread all-pops at the end) 
    dualprint("\nBenchmark: Multithreaded Push, Singlethreaded Pops, Random Values\n");
    for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
        dualprint("nthreads: %d, ", *n);
        run_benchmark(PUSHTHENPOP_RANDOM, 100000, q_push_only_txn_set, *n);
	dualprint("\n");
    }
    dualprint("\nBenchmark: Multithreaded Push, Singlethreaded Pops, Decreasing Values\n");
    for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
        dualprint("nthreads: %d, ", *n);
        run_benchmark(PUSHTHENPOP_DECREASING, 100000, q_push_only_txn_set, *n);
	dualprint("\n");
    }

    // run single-operation txns with different nthreads
    dualprint("\nSingle-Op Txns, Random Values\n");
    for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
        dualprint("nthreads: %d, ", *n);
	    dualprint("50000\n");
        run_benchmark(RANDOM, 50000, q_single_op_txn_set, *n);
	    dualprint("100000\n");
        run_benchmark(RANDOM, 100000, q_single_op_txn_set, *n);
	    dualprint("150000\n");
        run_benchmark(RANDOM, 150000, q_single_op_txn_set, *n);
	dualprint("\n");
    }
    dualprint("\nSingle-Op Txns, Decreasing Values\n");
    for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
        dualprint("nthreads: %d, ", *n);
        run_benchmark(DECREASING, 50000, q_single_op_txn_set, *n);
	    dualprint("100000\n");
        run_benchmark(DECREASING, 100000, q_single_op_txn_set, *n);
	    dualprint("150000\n");
        run_benchmark(DECREASING, 150000, q_single_op_txn_set, *n);
	    dualprint("\n");
    }

    cds::Terminate();
    return 0;
}
