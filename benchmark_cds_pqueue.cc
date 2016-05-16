#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>

#include <cds/init.h>
#include <cds/container/fcpriority_queue.h>
#include <cds/container/mspriority_queue.h>

#include "Transaction.hh"

#define GLOBAL_SEED 10

#define MAX_VALUE  100000
#define INIT_SIZE 100
#define NTXN 200 // Number of transactions each thread should run.
#define N_THREADS 10 // Number of concurrent threads

// type of data structure to be used
#define STO 0
#define CDS 1

enum op {push, pop}

// set of transactions to choose from
// approximately equivalent pushes and pops
// no transaction that includes both pushes and pops
std::vector<op> txns1[] = {
    {push, push, push},
    {pop, pop, pop},
    {pop}, {pop}, {pop},
    {push}, {push}, {push}
};

template <typename T>
struct Tester {
    T* ds;
    int ds_type;
    int me;
};

template <typename T>
void run_txn(T* pq, Rand transgen) {
    // randomly select a transaction to run
    auto txn = txns1[transgen() % sizeof(txns1)/sizeof(*txns1)];
    // run the txn with random values
    for (int j = 0; j < txn.size(); ++j) {
        int val = slotdist(transgen);
        switch(op) {
            case push:
                pq->push(val);
                break;
            case pop:
                pq->pop();
                break;
            default:
        }
    }
}

template <typename T>
void run(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    int me = tp->me;
    T* pq = tp->ds;
    int ds_type = tp->ds_type;

    // set up random operation and value distributions
    auto transseed = i;
    uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
    auto seedlow = seed & 0xffff;
    auto seedhigh = seed >> 16;
    Rand transgen(seed, seedlow << 16 | seedhigh);
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);

    // do the proper initialization depending on which library we're using
    if (ds_type == CDS) {
        cds::thread::Manager::attachThread();
    } else {
        TThread::set_id(me);
    }

    // generate all transactions the thread will run
    for (int i = 0; i < NTRANS; ++i) {
        // we're using the CDS pqueue, so we don't need to wrap in txn try-commit logic
        if (ds_type == CDS) {
            run_txn(pq, transgen);
        }
        else {
            // so that retries of this transaction do the same thing
            Rand transgen_snap = transgen;
            while (1) {
                Sto::start_transaction();
                try {
                    run_txn(pq, transgen);
                    if (Sto::try_commit()) {
                        break;
                    }
                } catch (Transaction::Abort e) {
                }
                transgen = transgen_snap;
            }
        }
    }

    if (ds_type == CDS) {
        cds::thread::Manager::detachThread();
    }
}

template <typename T>
void startAndWait(T* ds, int ds_type /*STO or CDS*/) {
    pthread_t tids[N_THREADS];
    Tester<T> testers[N_THREADS];
    for (int i = 0; i < N_THREADS; ++i) {
        testers[i].ds = ds;
        testers[i].me = i;
        testers[i].t = ds_type;
        pthread_create(&tids[i], NULL, run<T>, &testers[i]);
    }
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
    
    for (int i = 0; i < N_THREADS; ++i) {
        pthread_join(tids[i], NULL);
    }
}

void print_time(struct timeval tv1, struct timeval tv2) {
    printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

int main() {
    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh
    lock = 0;

    // initialize all data structures
    PriorityQueue<int> sto_pqueue = PriorityQueue<int>();
    FCPriorityQueue<int> cds::container::FCPriorityQueue fc_pqueue = cds::container::FCPriorityQueue<int>();
    MSPriorityQueue<int> cds::container::MSPriorityQueue ms_pqueue = cds::container::MSPriorityQueue<int>();
    
    for (int i = 0; i < INIT_SIZE; i++) {
        sto_pqueue->push(i);
        fc_pqueue->push(i);
        ms_pqueue->push(i);
    }

    // benchmark STO
    struct timeval tv1,tv2;
    gettimeofday(&tv1, NULL);
    
    startAndWait(&sto_pqueue, STO);
    
    gettimeofday(&tv2, NULL);
    printf("STO: Priority Queue, init size %d: ", INIT_SIZE);
    print_time(tv1, tv2);
    
    cds::Initialize();
    {
        // benchmark FC Pqueue
        gettimeofday(&tv1, NULL);
        
        startAndWait(&fc_pqueue, CDS);
        
        gettimeofday(&tv2, NULL);
        printf("CDS: FC Priority Queue, init size %d: ", INIT_SIZE);
        print_time(tv1, tv2);
        
        // benchmark  
        gettimeofday(&tv1, NULL);
        
        startAndWait(&ms_pqueue, CDS);
        
        gettimeofday(&tv2, NULL);
        printf("CDS: MS Priority Queue, init size %d: ", INIT_SIZE);
        print_time(tv1, tv2);
    }
    cds::Terminate();
    return 0;
}
