#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include "Transaction.hh"
#include "Vector.hh"
#include "PriorityQueue.hh"
#include "PriorityQueue1.hh"

#define GLOBAL_SEED 10
#define MAX_VALUE  100000
#define NTRANS 1000000
#define N_THREADS 4

typedef PriorityQueue<int> data_structure;

struct Rand {
    typedef uint32_t result_type;
    
    result_type u, v;
    Rand(result_type u, result_type v) : u(u|1), v(v|1) {}
    
    inline result_type operator()() {
        v = 36969*(v & 65535) + (v >> 16);
        u = 18000*(u & 65535) + (u >> 16);
        return (v << 16) + u;
    }
    
    static constexpr result_type max() {
        return (uint32_t)-1;
    }
    
    static constexpr result_type min() {
        return 0;
    }
};


template <typename T>
struct TesterPair {
    T* t;
    int me;
};


void run_conc(data_structure* q, int me) {
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    
    for (int i = 0; i < NTRANS; ++i) {
        // so that retries of this transaction do the same thing
        auto transseed = i;
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            
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
    Transaction::threadid = me + 1;
    
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    
    for (int i = 0; i < NTRANS; ++i) {
        // so that retries of this transaction do the same thing
        auto transseed = i;
        TRANSACTION {
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            
            int val1 = slotdist(transgen);
            int val2 = slotdist(transgen);
            int val3 = slotdist(transgen);
            q->push(val1);
            q->push(val2);
            q->push(val3);
            
            //q->pop();
            //q->pop();
        } RETRY(true)
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
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
    
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
        Transaction t;
        Sto::set_transaction(&t);
        // q is empty
        q.push(1);
        q.push(2);
        assert(q.top()  == 2);
        q.pop();
        assert(q.top() == 1);
        q.pop();
        assert(t.try_commit());
    }
    
    {
        Transaction t;
        Sto::set_transaction(&t);
        // q is empty
        q.push(1);
        q.push(2);
        assert(t.try_commit());
    }
    
    {
        // front with no pops
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.top() == 2);
        assert(q.top() == 2);
        assert(t.try_commit());
    }
    
    {
        // pop until empty
        Transaction t;
        Sto::set_transaction(&t);
        q.pop();
        q.pop(); // After this, queue is empty
        
        // prepare pushes for next test
        q.push(1);
        q.push(2);
        q.push(3);
        assert(t.try_commit());
    }
    
    {
        // fronts intermixed with pops
        Transaction t;
        Sto::set_transaction(&t);
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
        assert(t.try_commit());
    }
    
    {
        // front intermixed with pushes on nonempty
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.top() == 3);
        assert(q.top() == 3);
        q.push(4);
        assert(q.top() == 4);
        assert(t.try_commit());
    }
    
    {
        // pops intermixed with pushes and front on nonempty
        // q = [4 3 2 1]
        Transaction t;
        Sto::set_transaction(&t);
        q.pop();
        assert(q.top() == 3);
        q.push(5);
        // q = [5 3 2 1]
        q.pop();
        assert(q.top() == 3);
        q.push(6);
        // q = [6 3 2 1]
        assert(t.try_commit());
    }
    
    // EMPTY TESTS
    {
        // front with empty queue
        Transaction t;
        Sto::set_transaction(&t);
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
        assert(t.try_commit());
    }
    
    {
        // pop with empty queue
        Transaction t;
        Sto::set_transaction(&t);
        // empty the queue
        q.pop();
        //q.pop();
        
        //q.top();
        
        q.push(1);
        q.pop();
        //q.pop();
        assert(t.try_commit());
    }
    
    {
        // pop and front with empty queue
        Transaction t;
        Sto::set_transaction(&t);
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
        
        assert(t.try_commit());
    }
    
    // CONFLICTING TRANSACTIONS TEST
    {
        // test abortion due to pops
        Transaction t1;
        Transaction t2;
        // q has >1 element
        Transaction::threadid = 0;
        Sto::set_transaction(&t1);
        q.pop();
        Transaction::threadid = 1;
        Sto::set_transaction(&t2);
        bool aborted = false;
        try {
            q.pop();
        } catch (Transaction::Abort e) {
            aborted = true;
        }
        Transaction::threadid = 0;
        assert(t1.try_commit());
        Transaction::threadid = 1;
        assert(aborted);
    }
    
    {
        // test nonabortion T1 pops, T2 pushes on nonempty q
        Transaction t1;
        Transaction t2;
        // q has 1 element
        Transaction::threadid = 0;
        Sto::set_transaction(&t1);
        
        q.pop();
        Transaction::threadid = 1;
        Sto::set_transaction(&t2);
        bool aborted = false;
        try {
            q.push(3);
        } catch (Transaction::Abort e) {
            aborted = true;
        }
        Transaction::threadid = 0;
        assert(t1.try_commit());
        Transaction::threadid = 1;
        assert(aborted); // TODO: this also depends on queue implementation
        
        Transaction t3;
        Sto::set_transaction(&t3);
        q.push(3);
        assert(t3.try_commit());
        
    }

    {
        Transaction t1;
        Sto::set_transaction(&t1);
        assert(q.top() == 3);
        q.pop(); // q is empty after this
        assert(t1.try_commit());
    }
    
    {
        // test abortion due to empty q pops
        Transaction t1;
        Transaction t2;
        // q has 0 elements
        Transaction::threadid = 0;
        Sto::set_transaction(&t1);
        
        q.pop(); // TODO: should support pop from empty queue in PriorityQueue
        q.push(1);
        q.push(2);
        Transaction::threadid = 1;
        Sto::set_transaction(&t2);
        
        q.push(3);
        q.push(4);
        q.push(5);
        
        q.pop();
        
        Transaction::threadid = 0;
        assert(t1.try_commit());
        Transaction::threadid = 1;
        assert(t2.try_commit());
    }
    
    {
        Transaction t;
        Sto::set_transaction(&t);
        q.pop();
        q.pop();
        assert(t.try_commit());
    }
    
    {
        // test nonabortion T1 pops/fronts and pushes, T2 pushes on nonempty q
        Transaction t1;
        Transaction t2;
        
        // q has 2 elements [2, 1]
        Sto::set_transaction(&t1);
        assert(q.top() == 2);
        q.push(4);
        
        // pop from non-empty q
        q.pop();
        assert(q.top() == 2);
        assert(t1.try_commit());
        Sto::set_transaction(&t2);
        
        q.push(3);
        // order of pushes doesn't matter, commits succeed
        assert(t2.try_commit());
        
        // check if q is in order
        Transaction t;
        Sto::set_transaction(&t);
        assert(q.top() == 3);
        q.pop();
        assert(q.top() == 2);
        q.pop();
        assert(q.top() == 1);
        q.pop();
        assert(t.try_commit());
    }
    
    {
        Transaction t;
        Transaction::threadid = 0;
        Sto::set_transaction(&t);
        q.push(10);
        q.push(4);
        q.push(5);
        assert(t.try_commit());
        
        Transaction t1;
        Sto::set_transaction(&t1);
        q.pop();
        q.push(20);
        
        Transaction t2;
        Transaction::threadid = 1;
        Sto::set_transaction(&t2);
        bool aborted = false;
        try {
            q.push(12);
        } catch (Transaction::Abort e) {
            aborted = true;
        }
        
        Transaction::threadid = 0;
        assert(t1.try_commit());
        assert(aborted);
    }
    
    {
        Transaction t;
        Transaction::threadid = 0;
        Sto::set_transaction(&t);
        q.top();
        
        Transaction t1;
        Transaction::threadid = 1;
        Sto::set_transaction(&t1);
        q.push(100);
        
        assert(t1.try_commit());
        Transaction::threadid = 0;
        Sto::set_transaction(&t);
        assert(!t.try_commit());
        
    }
}

void print_time(struct timeval tv1, struct timeval tv2) {
    printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

int main() {
    queueTests();
    std::cout << "Done queue tests" << std::endl;
    
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
    
    struct timeval tv1,tv2;
    gettimeofday(&tv1, NULL);
    
    startAndWait(&q);
    
    gettimeofday(&tv2, NULL);
    printf("Parallel time: ");
    print_time(tv1, tv2);
    
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif
    
    data_structure q1;
    gettimeofday(&tv1, NULL);
    
    for (int i = 0; i < N_THREADS; i++) {
        run(&q1, i);
    }
    
    gettimeofday(&tv2, NULL);
    printf("Serial time: ");
    print_time(tv1, tv2);
    
    
    data_structure q2;
    gettimeofday(&tv1, NULL);
    
    startAndWait(&q2, false);
    
    gettimeofday(&tv2, NULL);
    printf("Concurrent time: ");
    print_time(tv1, tv2);
    
    PriorityQueue1<int> q3;
    gettimeofday(&tv1, NULL);
    
    startAndWait(&q3);
    
    gettimeofday(&tv2, NULL);
    printf("Vector rep time: ");
    print_time(tv1, tv2);
    
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif
    
   
    if (false) {
    TRANSACTION {
        //q.print();
        //q1.print();
    } RETRY(false);
    for (int i = 0; i < q1.size(); i++) {
        TRANSACTION {
            assert(q.top() == q1.top());
            q.pop();
            q1.pop();
        } RETRY(false)
    }
    }

	return 0;
}
