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
#include "randgen.hh"

#define GLOBAL_SEED 10

#define MAX_VALUE  100000
#define MAX_SIZE 100000
#define NTRANS 200 // Number of transactions each thread should run.
#define N_THREADS 10 // Number of concurrent threads

// type of data structure to be used
#define STO 0
#define CDS 1

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
        typedef cds::container::FCPriorityQueue< T> base_class;

    public:
        void pop() {
            int ret;
            base_class::pop(ret);
        }
        void push(T v) { base_class::push(v); }
        size_t size() { return base_class::size(); }
};

enum op {push, pop};

// set of transactions to choose from
// approximately equivalent pushes and pops

std::vector<std::vector<op>> all_txns[] = 
{
    // short txns
    {
        {push, push, push},
        {pop, pop, pop},
        {pop}, {pop}, {pop},
        {push}, {push}, {push}
    },
    // longer txns
    {
        {push, push, push, push, push},
        {pop, pop, pop, pop, pop},
        {pop}, {pop}, {pop}, {pop}, {pop}, 
        {push}, {push}, {push}, {push}, {push}
    },
    // 100% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
    },
    // 50% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
        {pop}, {push}
    },
    // 33% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
        {pop}, {pop},
        {push}, {push}
    },
    // 33%: longer push + pop txns
    {
        {push, pop, push, pop, push, pop},
        {pop}, 
        {push}
    }
};

std::vector<int> sizes = {100, 200, 300, 400, 500};

template <typename T>
struct Tester {
    T* ds;
    int ds_type;
    int me;
    std::vector<std::vector<op>> txn_set;
};

template <typename T>
void do_txn(T* pq, Rand transgen, std::vector<op> txn) {
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    for (unsigned j = 0; j < txn.size(); ++j) {
        int val = slotdist(transgen);
        switch(txn[j]) {
            case push:
                pq->push(val);
                break;
            case pop:
                pq->pop();
                break;
            default:
                break;
        }
    }
}

template <typename T>
void* run_sto(void* x) {
    Tester<T>* tp = (Tester<T>*) x;
    int me = tp->me;
    T* pq = tp->ds;
    auto txn_set = tp->txn_set;
    
    TThread::set_id(me);

    // generate all transactions the thread will run
    for (int i = 0; i < NTRANS; ++i) {
        auto transseed = i;
        uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);

        // randomly select a transaction to run
        auto txn = txn_set[transgen() % txn_set.size()];

        // so that retries of this transaction do the same thing
        Rand transgen_snap = transgen;
        while (1) {
            Sto::start_transaction();
            try {
                do_txn(pq, transgen, txn);
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
    int me = tp->me;
    T* pq = tp->ds;
    auto txn_set = tp->txn_set;
    
    cds::threading::Manager::attachThread();

    // generate all transactions the thread will run
    for (int i = 0; i < NTRANS; ++i) {
        auto transseed = i;
        uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
        auto seedlow = seed & 0xffff;
        auto seedhigh = seed >> 16;
        Rand transgen(seed, seedlow << 16 | seedhigh);

        // randomly select a transaction to run
        auto txn = txn_set[transgen() % txn_set.size()];
        do_txn(pq, transgen, txn);
    }
    cds::threading::Manager::detachThread();
    return nullptr;
}

template <typename T>
void startAndWait(T* ds, int ds_type, std::vector<std::vector<op>> txn_set) {
    pthread_t tids[N_THREADS];
    Tester<T> testers[N_THREADS];
    for (int i = 0; i < N_THREADS; ++i) {
        testers[i].ds = ds;
        testers[i].me = i;
        testers[i].txn_set = txn_set;
        if (ds_type == CDS) {
            pthread_create(&tids[i], NULL, run_cds<T>, &testers[i]);
        } else {
            pthread_create(&tids[i], NULL, run_sto<T>, &testers[i]);
        }
    }
    
    for (int i = 0; i < N_THREADS; ++i) {
        pthread_join(tids[i], NULL);
    }
}

void print_time(struct timeval tv1, struct timeval tv2) {
    printf("\t%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

int main() {
    std::ios_base::sync_with_stdio(true);
    assert(CONSISTENCY_CHECK); // set CONSISTENCY_CHECK in Transaction.hh

    cds::Initialize();

    // create the epoch advancer thread
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);

    // iterate through the txn set for all different sizes
    for (auto size = begin(sizes); size != end(sizes); ++size) {
        printf("Init size %d\n", size);
        for (auto it = begin(all_txns); it != end(all_txns); ++it) {
            printf("Running Txn Set %ld\n", it-begin(all_txns));

            // initialize all data structures
            PriorityQueue<int> sto_pqueue;
            WrappedFCPriorityQueue<int> fc_pqueue;
            WrappedMSPriorityQueue<int> ms_pqueue(MAX_SIZE);
            Sto::start_transaction();
            for (int i = 0; i < *size; i++) {
                sto_pqueue.push(i);
                fc_pqueue.push(i);
                ms_pqueue.push(i);
            }
            assert(Sto::try_commit());

            // benchmark STO
            struct timeval tv1,tv2;
            gettimeofday(&tv1, NULL);
            
            startAndWait(&sto_pqueue, STO, *it);
            
            gettimeofday(&tv2, NULL);
            printf("STO: Priority Queue \tFinal Size %d", sto_pqueue.unsafe_size());
            print_time(tv1, tv2);
            
            // benchmark FC Pqueue
            gettimeofday(&tv1, NULL);
            startAndWait(&fc_pqueue, CDS, *it);
            gettimeofday(&tv2, NULL);
            printf("CDS: FC Priority Queue \tFinal Size %lu", fc_pqueue.size());
            print_time(tv1, tv2);
       
            // benchmark MS Pqueue
            gettimeofday(&tv1, NULL);
            startAndWait(&ms_pqueue, CDS, *it);
            gettimeofday(&tv2, NULL);
            printf("CDS: MS Priority Queue \tFinal Size %lu", ms_pqueue.size());
            print_time(tv1, tv2);
        }
    }
    cds::Terminate();
    return 0;
}
