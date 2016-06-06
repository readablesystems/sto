#include <memory>
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

// run the specified transactions for the particular benchmark
template <typename T>
inline void do_txn(T* q, int me, std::vector<op> txn_ops, int txn_val, size_t size) {
    (void)size;
    for (unsigned j = 0; j < txn_ops.size(); ++j) {
        switch(txn_ops[j]) {
            case push:
                q->push(txn_val);
                global_thread_push_ctrs[me]++;
                break;
            case pop:
                q->pop();
                global_thread_pop_ctrs[me]++;
                break;
            default:
                assert(0);
                break;
        }
    }
}

void run_benchmark(int bm, int size, std::vector<std::vector<op>> txn_set, int nthreads) {
    global_val = MAX_VALUE;
   
    // initialize all data structures
    STOQueue<int>sto_queue;
    CDSQueue<int>q1(basket); 
    CDSQueue<int>q2(fc); 
    CDSQueue<int>q3(moir);
    CDSQueue<int>q4(ms);
    CDSQueue<int>q5(optimistic);
    CDSQueue<int>q6(rw);
    //CDSQueue<int>q7(segmented);
    CDSQueue<int>q8(tc);
    CDSQueue<int>q9(vm);

    std::vector<CDSQueue<int>*> cds_queues = {&q1, &q2, &q3, &q4, &q5, &q6, &q8, &q9};

    for (int i = global_val; i < size; ++i) {
        Sto::start_transaction();
        sto_queue.push(global_val);
        assert(Sto::try_commit());
        for (auto q = begin(cds_queues); q != end(cds_queues); ++q) {
            (*q)->push(global_val);
        }
        global_val--;
    }

    for (auto q = begin(cds_queues); q != end(cds_queues); ++q) {
        startAndWait(*q, CDS, bm, txn_set, size, nthreads);
    }
    startAndWait(&sto_queue, STO, bm, txn_set, size, nthreads);
}

int main() {
    dualprint("nthreads: %d, ntxns/thread: %d, max_value: %d, max_size: %d\n", N_THREADS, NTRANS, MAX_VALUE, MAX_SIZE);
    dualprint("\nSTO, Basket, FC, Moir, MS, Optimistic, RW, TsigasCycle, VyukovMPMCCycle\n");

    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh

    cds::Initialize();
    cds::gc::HP hpGC;
    cds::threading::Manager::attachThread();

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
            
            dualprint("---------Benchmark: Random-----------\n");
            run_benchmark(RANDOM, *size, *txn_set, N_THREADS);
        }
    }
    */
    /* 
    // run the push-only test (with single-thread all-pops at the end) 
    dualprint("\nBenchmark: Multithreaded Push, Singlethreaded Pops, Random Values\n");
    for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
        dualprint("nthreads: %d, ", *n);
        run_benchmark(RANDOM, 100000, q_push_only_txn_set, *n);
        dualprint("\n");
    }
    */
   
    // run the two-thread test where one thread only pushes and the other only pops
    dualprint("\nBenchmark: No Aborts (2 threads: one pushing, one popping)\n");
    for (auto size = begin(sizes); size != end(sizes); ++size) {
        fprintf(stderr, "Init size: %d\n", *size);
        run_benchmark(NOABORTS, *size, {}, 2);
        dualprint("\n");
    }
    // run single-operation txns with different nthreads
    dualprint("\nSingle-Op Txns, Random Values\n");
    for (auto size = begin(sizes); size != end(sizes); ++size) {
        fprintf(stderr, "Init size: %d\n", *size);
    	for (auto n = begin(nthreads_set); n != end(nthreads_set); ++n) {
        fprintf(stderr, "nthreads: %d\n", *n);
            run_benchmark(RANDOM, *size, q_single_op_txn_set, *n);
    	    dualprint("\n");
        }
    	dualprint("\n");
    }
    
    cds::threading::Manager::detachThread();
    cds::Terminate();
    return 0;
}
