#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "Vector.hh"
#include "PriorityQueue.hh"
#include "PriorityQueue1.hh"
#include "randgen.hh"

#define GLOBAL_SEED 0
#define MAX_VALUE  100000
#define NTRANS 1000
#define N_THREADS 4

typedef PriorityQueue<int> data_structure;
unsigned initial_seeds[128];


struct txn_record {
    // keeps track of pushes and pops for a single transaction
    std::vector<int> pushes;
    std::vector<int> tops;
    int pops;
};



template <typename T>
struct TesterPair {
    T* t;
    int me;
};

std::vector<std::map<uint64_t, txn_record *> > txn_list;
typedef TransactionTid::type Version;
Version lock;

void run_conc(data_structure* q, int me) {
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);

    for (int i = 0; i < NTRANS; ++i) {
        // so that retries of this transaction do the same thing
        int val1 = slotdist(transgen);
        int val2 = slotdist(transgen);
        int val3 = slotdist(transgen);
        q->push_nontrans(val1);
        q->push_nontrans(val2);
        q->push_nontrans(val3);
        //q->pop();
        //q->pop();
    }
}

template <typename T>
void run(T* q, int me) {
    TThread::set_id(me);

    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);

    for (int i = 0; i < NTRANS; ++i) {
        // so that retries of this transaction do the same thing
        Rand transgen_snap = transgen;
        txn_record *tr = new txn_record;
        while (1) {
        Sto::start_transaction();
        try {
            tr->pushes.clear();
            tr->pops = 0;
            tr->tops.clear();

            int val1 = slotdist(transgen);
            int val2 = slotdist(transgen);
            int val3 = slotdist(transgen);
            q->pop();
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] try to push " << val1 << std::endl;
            //TransactionTid::unlock(lock);
            q->push(val1);
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] pushed " << val1 << std::endl;
            //TransactionTid::unlock(lock);
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] try to push " << val2 << std::endl;
            //TransactionTid::unlock(lock);
            q->push(val2);
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] pushed " << val2 << std::endl;
            //TransactionTid::unlock(lock);
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] try to push " << val3 << std::endl;
            //TransactionTid::unlock(lock);
            //q->push(val3);
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] pushed " << val3 << std::endl;
            //TransactionTid::unlock(lock);
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] try to read "  << std::endl;
            //TransactionTid::unlock(lock);
            int rval = q->top();
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] read " << rval << std::endl;
            //TransactionTid::unlock(lock);
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] try to pop "  << std::endl;
            //TransactionTid::unlock(lock);
            //int pval = q->pop();
            q->push(val3);
            //TransactionTid::lock(lock);
            //std::cout << "[" << me << "] popped " << pval << std::endl;
            //TransactionTid::unlock(lock);
            //q->pop();

            tr->pushes.push_back(val1);
            tr->pushes.push_back(val2);
            tr->pushes.push_back(val3);
            tr->pops = 1;
            tr->tops.push_back(rval);

            if (Sto::try_commit()) {
                //TransactionTid::lock(lock);
                //std::cout << "[" << me << "] committed "  << std::endl;
                //TransactionTid::unlock(lock);
                txn_list[me][Sto::commit_tid()] = tr;
                break;
            }

        } catch (Transaction::Abort e) {
            //TransactionTid::lock(lock); std::cout << "[" << me << "] aborted "<< std::endl; TransactionTid::unlock(lock);
        }
        transgen = transgen_snap;
        }
    }
}

template <typename T>
void* runFunc(void* x) {
    TesterPair<T>* tp = (TesterPair<T>*) x;
    run(tp->t, tp->me);
    return nullptr;
}

void* runConcFunc(void* x) {
    TesterPair<data_structure>* tp = (TesterPair<data_structure>*) x;
    run_conc(tp->t, tp->me);
    return nullptr;
}

template <typename T>
void startAndWait(T* queue, bool parallel = true) {
    pthread_t tids[N_THREADS];
    TesterPair<T> testers[N_THREADS];
    for (int i = 0; i < N_THREADS; ++i) {
        testers[i].t = queue;
        testers[i].me = i;
        if (parallel)
            pthread_create(&tids[i], NULL, runFunc<T>, &testers[i]);
        else
            pthread_create(&tids[i], NULL, runConcFunc, &testers[i]);
    }

    for (int i = 0; i < N_THREADS; ++i) {
        pthread_join(tids[i], NULL);
    }
}

// These tests are adapted from the queue tests in single.cc
void queueTests() {
    data_structure q;
    
    // NONEMPTY TESTS
    {
        // ensure pops read pushes in FIFO order
        TransactionGuard t;
        // q is empty
        q.push(1);
        q.push(2);
        assert(q.top()  == 2);
        q.pop();
        assert(q.top() == 1);
        q.pop();
    }
    
    {
        TransactionGuard t;
        // q is empty
        q.push(1);
        q.push(2);
    }
    
    {
        // front with no pops
        TransactionGuard t;
        assert(q.top() == 2);
        assert(q.top() == 2);
    }
    
    {
        // pop until empty
        TransactionGuard t;
        q.pop();
        q.pop(); // After this, queue is empty
        
        // prepare pushes for next test
        q.push(1);
        q.push(2);
        q.push(3);
    }
    
    {
        // fronts intermixed with pops
        TransactionGuard t;
        assert(q.top() == 3);
        q.pop();
        assert(q.top() == 2);
        q.pop();
        assert(q.top() == 1);
        q.pop();
        //q.pop();
        
        // set up for next test
        q.push(1);
        q.push(2);
        q.push(3);
    }
    
    {
        // front intermixed with pushes on nonempty
        TransactionGuard t;
        assert(q.top() == 3);
        assert(q.top() == 3);
        q.push(4);
        assert(q.top() == 4);
    }
    
    {
        // pops intermixed with pushes and front on nonempty
        // q = [4 3 2 1]
        TransactionGuard t;
        q.pop();
        assert(q.top() == 3);
        q.push(5);
        // q = [5 3 2 1]
        q.pop();
        assert(q.top() == 3);
        q.push(6);
        // q = [6 3 2 1]
    }
    
    // EMPTY TESTS
    {
        // front with empty queue
        TransactionGuard t;
        // empty the queue
        q.pop();
        q.pop();
        q.pop();
        q.pop();
        //q.pop();
        
        //q.top();
        
        q.push(1);
        assert(q.top() == 1);
        assert(q.top() == 1);
    }
    
    {
        // pop with empty queue
        TransactionGuard t;
        // empty the queue
        q.pop();
        //q.pop();
        
        //q.top();
        
        q.push(1);
        q.pop();
        //q.pop();
    }
    
    {
        // pop and front with empty queue
        TransactionGuard t;
        //q.top();
        
        q.push(1);
        assert(q.top() == 1);
        q.pop();
        
        q.push(1);
        q.pop();
        //q.top();
        //q.pop();
        
        // add items for next test
        q.push(1);
        q.push(2);
    }
    
    // CONFLICTING TRANSACTIONS TEST
    {
        // test abortion due to pops
        TestTransaction t1(1);
        TestTransaction t2(2);
        // q has >1 element
        t1.use();
        q.pop();
        t2.use();
        bool aborted = false;
        try {
            q.pop();
        } catch (Transaction::Abort e) {
            aborted = true;
        }
        assert(t1.try_commit());
        assert(aborted);
    }
    
    {
        // test nonabortion T1 pops, T2 pushes on nonempty q
        TestTransaction t1(1);
        TestTransaction t2(2);
        // q has 1 element
        t1.use();
        
        q.pop();
        t2.use();
        bool aborted = false;
        try {
            q.push(3);
        } catch (Transaction::Abort e) {
            aborted = true;
        }
        assert(t1.try_commit());
        assert(aborted); // TODO: this also depends on queue implementation
    }
    {
        TransactionGuard t3;
        q.push(3);
    }

    {
        TransactionGuard t1;
        assert(q.top() == 3);
        q.pop(); // q is empty after this
    }
    
    {
        // test abortion due to empty q pops
        TestTransaction t1(1);
        TestTransaction t2(2);
        // q has 0 elements
        t1.use();
        q.pop();
        q.push(1);
        q.push(2);

        t2.use();
        q.push(3);
        q.push(4);
        q.push(5);
        q.pop();

        assert(!t1.try_commit()); // TODO: this can actually commit
        assert(t2.try_commit());
    }
    
    {
        TransactionGuard t;
        q.pop();
        q.pop();
        q.push(1);
        q.push(2);
    }
    
    {
        // test nonabortion T1 pops/fronts and pushes, T2 pushes on nonempty q
        TestTransaction t1(1);
        TestTransaction t2(2);
        
        // q has 2 elements [2, 1]
        t1.use();
        assert(q.top() == 2);
        q.push(4);
        
        // pop from non-empty q
        q.pop();
        assert(q.top() == 2);
        assert(t1.try_commit());

        t2.use();
        q.push(3);
        // order of pushes doesn't matter, commits succeed
        assert(t2.try_commit());
    }

    {
        // check if q is in order
        TransactionGuard t;
        assert(q.top() == 3);
        q.pop();
        assert(q.top() == 2);
        q.pop();
        assert(q.top() == 1);
        q.pop();
    }
    
    {
        TransactionGuard t;
        q.push(10);
        q.push(4);
        q.push(5);
    }

    {
        TestTransaction t1(1);
        q.pop();
        q.push(20);
        
        TestTransaction t2(2);
        bool aborted = false;
        try {
            q.push(12);
        } catch (Transaction::Abort e) {
            aborted = true;
        }
        
        assert(t1.try_commit());
        assert(aborted);
    }
    
    {
        TestTransaction t(1);
        q.top();
        
        TestTransaction t1(2);
        q.push(100);
        
        assert(t1.try_commit());
        assert(!t.try_commit());
    }
    
    {
        // prepare queue for next test
        TransactionGuard t;
        q.pop();
        q.pop();
        q.pop();
        q.pop();
        q.push(5);
    }
    
    {
        TestTransaction t(1);
        q.top(); // gets 5
        q.push(10);

        TestTransaction t1(2);
        q.push(7);
        assert(t1.try_commit());
        assert(!t.try_commit());
    }
    
    {
        // prepare queue for next test
        TransactionGuard t;
        q.pop();
        q.pop();
    }
    
    {
        TestTransaction t(1);
        q.pop(); // poping froming an empty queue

        TestTransaction t1(2);
        q.push(4);
        assert(t1.try_commit());
        assert(!t.try_commit());
    }
    
    {
        TestTransaction t(1);
        assert(q.top() == 4);

        TestTransaction t1(2);
        q.push(6);
        assert(t1.try_commit());

        TestTransaction t2(3);
        q.pop();

        assert(!t.try_commit());
        assert(t2.try_commit());
    }

    
    {
        TestTransaction t(1);
        assert(q.top() == 4);

        TestTransaction t1(2);
        q.push(6);
        assert(t1.try_commit());
        
        t.use();
        bool aborted = false;
        try {
            q.pop();
        } catch (Transaction::Abort e) {
            aborted = true;
        }
        assert(aborted);
    }
}

void print_time(struct timeval tv1, struct timeval tv2) {
    printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

int main() {
    queueTests();
    std::cout << "Done queue tests" << std::endl;
    lock = 0;
    // Run a parallel test with lots of transactions doing pushes and pops
    data_structure q;
    /*for (int i = 0; i < 10000; i++) {
        TRANSACTION {
            q.push(i);
        } RETRY(false);
    }
    for (int i = 0; i < 2000; i++) {
        TRANSACTION {
            q.pop();
        } RETRY(false);
    }*/

#if GLOBAL_SEED
    srandom(GLOBAL_SEED);
#else
    srandomdev();
#endif
    for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
        initial_seeds[i] = random();

    struct timeval tv1,tv2;
    gettimeofday(&tv1, NULL);
    
    for (int i = 0; i < N_THREADS; i++) {
        txn_list.emplace_back();
    }

    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);

    startAndWait(&q);
    
    gettimeofday(&tv2, NULL);
    printf("Parallel time: ");
    print_time(tv1, tv2);
    
#if STO_PROFILE_COUNTERS
    Transaction::print_stats();
    {
        txp_counters tc = Transaction::txp_counters_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif
    
    
    std::map<uint64_t, txn_record *> combined_txn_list;
    
    for (int i = 0; i < N_THREADS; i++) {
        combined_txn_list.insert(txn_list[i].begin(), txn_list[i].end());
    }
    
    std::cout << "Single thread replay" << std::endl;
    data_structure q1;
    gettimeofday(&tv1, NULL);
    
    std::map<uint64_t, txn_record *>::iterator it = combined_txn_list.begin();
    
    for(; it != combined_txn_list.end(); it++) {
        Sto::start_transaction();
        q1.pop();
        q1.push(it->second->pushes[0]);
        //std::cout << "Pushed " << it->second->pushes[0] << std::endl;
        q1.push(it->second->pushes[1]);
        //std::cout << "Pushed " << it->second->pushes[1] << std::endl;
        //std::cout << "Pushed " << it->second->pushes[2] << std::endl;
        assert(q1.top() == it->second->tops[0]);
        //assert(q1.pop() == it->second->tops[0]);
        q1.push(it->second->pushes[2]);
        //std::cout << "Popped " << it->second->tops[0] << std::endl;
        assert(Sto::try_commit());
    }
    
    //for (int i = 0; i < N_THREADS; i++) {
    //    run(&q1, i);
    //}
    
    gettimeofday(&tv2, NULL);
    printf("Serial time: ");
    print_time(tv1, tv2);
    
    
    //data_structure q2;
    //gettimeofday(&tv1, NULL);
    
    //startAndWait(&q2, false);
    
    //gettimeofday(&tv2, NULL);
    //printf("Concurrent time: ");
    //print_time(tv1, tv2);
    
    //PriorityQueue1<int> q3;
    //gettimeofday(&tv1, NULL);
    
    //startAndWait(&q3);
    
    //gettimeofday(&tv2, NULL);
    //printf("Vector rep time: ");
    //print_time(tv1, tv2);
    
/*#if STO_PROFILE_COUNTERS
    Transaction::print_stats();
    {
        txp_counters tc = Transaction::txp_counters_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif*/
    
    if (true) {
    TRANSACTION {
        //q.print();
        //q1.print();
    } RETRY(false);
    //std::cout << q.size() << " " << q1.size() << std::endl;
    //assert(q.size() == q1.size());
    assert(q1.unsafe_size() == N_THREADS* NTRANS*2 + 1);
    int size = q1.unsafe_size();
    for (int i = 0; i < size; i++) {
        TRANSACTION {
            int v1 = q.top();
            int v2 = q1.top();
            //std::cout << v1 << " " << v2 << std::endl;
            if (v1 != v2)
                q1.print();
            assert(v1 == v2);
            assert(q.pop() == v1);
            assert(q1.pop() == v2);
        } RETRY(false);
    }
        TRANSACTION {
            q.print();
            assert(q.top() == -1);
            //q1.print();
        } RETRY(false);
    }

	return 0;
}
