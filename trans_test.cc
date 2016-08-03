#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "Testers.hh"

#define GLOBAL_SEED 10
#define NTRANS 1000 // Number of transactions each thread should run.
#define N_THREADS 8 // Number of concurrent threads
#define MAX_OPS 5 // Maximum number of operations in a transaction.

#define PRIORITY_QUEUE 0
#define HASHTABLE 1
#define RBTREE 2
#define VECTOR 3
#define CUCKOOHASHMAP 4
//#define DS HASHTABLE
#define DS CUCKOOHASHMAP

#if DS == PRIORITY_QUEUE
PqueueTester<PriorityQueue<int>> tester = PqueueTester<PriorityQueue<int>>();
#elif DS == HASHTABLE
HashtableTester<Hashtable<int, int, false, 1000000>> tester = HashtableTester<Hashtable<int, int, false, 1000000>>();
#elif DS == RBTREE
RBTreeTester<RBTree<int, int, true>, std::map<int, int>> tester = RBTreeTester<RBTree<int, int, true>, std::map<int, int>>();
#elif DS == VECTOR
VectorTester<Vector<int>> tester = VectorTester<Vector<int>>();
#elif DS == CUCKOOHASHMAP 
CuckooHashMapTester<CuckooHashMap<int, int>, CuckooHashMap<int,int>> tester = CuckooHashMapTester<CuckooHashMap<int, int>, CuckooHashMap<int,int>>();
#endif

unsigned initial_seeds[64];
std::atomic<int> spawned_barrier(0);
txp_counter_type global_thread_ctrs[N_THREADS];

void* record_perf_thread(void* x) {
    int nthreads = *(int*)x;
    int total1, total2;
    struct timeval tv1, tv2;
    double ops_per_s = 0; 

    while (spawned_barrier != nthreads) {
        sched_yield();
    }

    // benchmark until the first thread finishes
    gettimeofday(&tv1, NULL);
    total1 = total2 = 0;
    for (int i = 0; i < N_THREADS; ++i) {
        total1 += global_thread_ctrs[i];
    }
    while (spawned_barrier != 0) {
        sched_yield();
    }
    for (int i = 0; i < N_THREADS; ++i) {
        total2 += global_thread_ctrs[i];
    }
    gettimeofday(&tv2, NULL);
    double seconds = ((tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
    ops_per_s = (total2-total1)/seconds > ops_per_s ? (total2-total1)/seconds : ops_per_s;
    fprintf(stderr, "%d threads speed: %f ops/s\n", nthreads, ops_per_s);
    return nullptr;
}

template <typename T>
void run(T* q, int me) {
    TThread::set_id(me);
    
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
   
    spawned_barrier++;
    while (spawned_barrier != N_THREADS) {
        sched_yield();
    }

    for (int i = 0; i < NTRANS; ++i) {
#if CONSISTENCY_CHECK
        txn_record *tr = new txn_record;
#endif
        // so that retries of this transaction do the same thing
        Rand transgen_snap = transgen;
        while (1) {
        Sto::start_transaction();
        try {
#if CONSISTENCY_CHECK
            tr->ops.clear();
#endif
            int numOps = slotdist(transgen) % MAX_OPS + 1;
            
            for (int j = 0; j < numOps; j++) {
                int op = slotdist(transgen) % tester.num_ops_;
#if CONSISTENCY_CHECK
                tr->ops.push_back(tester.doOp(q, op, me, slotdist, transgen));
#else
                tester.doOp(q, op, me, slotdist, transgen);
#endif
            }

            if (Sto::try_commit()) {
#if PRINT_DEBUG
                TransactionTid::lock(lock);
                std::cerr << "[" << me << "] committed " << Sto::commit_tid() << std::endl;
                TransactionTid::unlock(lock);
#endif
#if CONSISTENCY_CHECK
                txn_list[me][Sto::commit_tid()] = tr;
#endif
                global_thread_ctrs[me] += numOps;
                break;
            } else {
#if PRINT_DEBUG
                TransactionTid::lock(lock); std::cerr << "[" << me << "] aborted "<< std::endl; TransactionTid::unlock(lock);
#endif
                transgen = transgen_snap;
            }

        } catch (Transaction::Abort e) {
#if PRINT_DEBUG
            TransactionTid::lock(lock); std::cerr << "[" << me << "] aborted "<< std::endl; TransactionTid::unlock(lock);
#endif
        }
        }
    }

    spawned_barrier--;
}

template <typename T>
void* runFunc(void* x) {
    TesterPair<T>* tp = (TesterPair<T>*) x;
    run(tp->t, tp->me);
#if PRINT_DEBUG 
    TransactionTid::lock(lock); tester.print_stats(tp->t); TransactionTid::unlock(lock);
#endif
    return nullptr;
}

template <typename T>
void startAndWait(T* ds) {
    // create performance recording thread
    pthread_t recorder;
    int nthreads = N_THREADS;
    pthread_create(&recorder, NULL, record_perf_thread, &nthreads);

    pthread_t tids[N_THREADS];
    TesterPair<T> testers[N_THREADS];
    for (int i = 0; i < N_THREADS; ++i) {
        testers[i].t = ds;
        testers[i].me = i;
        pthread_create(&tids[i], NULL, runFunc<T>, &testers[i]);
    }
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
   
    for (int i = 0; i < N_THREADS; ++i) {
        pthread_join(tids[i], NULL);
    }
    pthread_join(recorder, NULL);

    int total_ops = 0;
    for (int i = 0; i < N_THREADS; ++i) {
        total_ops += global_thread_ctrs[i];
        global_thread_ctrs[i] = 0;
    }
}


int main() {
    std::ios_base::sync_with_stdio(true);
    lock = 0;

    srandomdev();
    for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
        initial_seeds[i] = random();

#if DS == PRIORITY_QUEUE 
    PriorityQueue<int> q;
    PriorityQueue<int> q1;
#elif DS == HASHTABLE
    Hashtable<int, int, false, 1000000> q;
    Hashtable<int, int, false, 1000000> q1;
#elif DS == RBTREE
    RBTree<int, int, true> q;
    std::map<int, int> q1;
#elif DS == VECTOR
    Vector<int> q;
    Vector<int> q1;
#elif DS == CUCKOOHASHMAP
    CuckooHashMap<int, int> q;
    CuckooHashMap<int, int> q1;
#endif  

    tester.init(&q);
    tester.init(&q1);

#if CONSISTENCY_CHECK
    fprintf(stderr, "Multi-then-single thread execution with CONSISTENCY_CHECK\n");
    for (int i = 0; i < N_THREADS; i++) {
        txn_list.emplace_back();
    }

    spawned_barrier = 0;
    startAndWait(&q);
#if PRINT_DEBUG
#if STO_PROFILE_COUNTERS
    Transaction::print_stats();
    {
        txp_counters tc = Transaction::txp_counters_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif
#endif
    
    std::map<uint64_t, txn_record *> combined_txn_list;
    
    for (int i = 0; i < N_THREADS; i++) {
        combined_txn_list.insert(txn_list[i].begin(), txn_list[i].end());
    }
    
    int nthreads = 1;
    spawned_barrier = 0;
    pthread_t recorder;
    pthread_create(&recorder, NULL, record_perf_thread, &nthreads);
   
    spawned_barrier++;
    std::map<uint64_t, txn_record *>::iterator it = combined_txn_list.begin();
    for(; it != combined_txn_list.end(); it++) {
#if PRINT_DEBUG
        std::cerr << "BEGIN txn " << it->first << std::endl;
#endif
        Sto::start_transaction();
        for (unsigned i = 0; i < it->second->ops.size(); i++) {
            global_thread_ctrs[0]++;
            tester.redoOp(&q1, it->second->ops[i]);
        }
        assert(Sto::try_commit());
#if PRINT_DEBUG
        std::cerr << "COMMITTED" << std::endl;
#endif
    }
    spawned_barrier--;
    pthread_join(recorder, NULL);
   
    tester.check(&q, &q1);
#else 
    fprintf(stderr, "Multi-then-single thread execution without CONSISTENCY_CHECK\n");
    // no consistency check, so let's just see how fast our transactions run compared to running the same operations (maybe not 
    // in the same order) sequentially

    spawned_barrier = 0;
    startAndWait(&q);
#if PRINT_DEBUG
#if STO_PROFILE_COUNTERS
    Transaction::print_stats();
    {
        txp_counters tc = Transaction::txp_counters_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif
#endif
    
    int nthreads = 1;
    spawned_barrier = 0;
    pthread_t recorder;
    pthread_create(&recorder, NULL, record_perf_thread, &nthreads);

    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    Rand transgen(initial_seeds[0], initial_seeds[1]);
   
    spawned_barrier++;
    for(int i = 0; i < N_THREADS*NTRANS; i++) {
#if PRINT_DEBUG
        std::cerr << "BEGIN txn " << it->first << std::endl;
#endif
        Sto::start_transaction();
        int op = slotdist(transgen) % tester.num_ops_;
        tester.doOp(&q1, op, 0, slotdist, transgen);
        global_thread_ctrs[0]++;
        assert(Sto::try_commit());
#if PRINT_DEBUG
        std::cerr << "COMMITTED" << std::endl;
#endif
    }
    spawned_barrier--;
    pthread_join(recorder, NULL);
#endif 
	return 0;
}
