#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include "Transaction.hh"
#include "Vector.hh"
#include "PriorityQueue.hh"

#define GLOBAL_SEED 0
#define MAX_VALUE  100000
#define NTRANS 100000
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


struct TesterPair {
    data_structure* t;
    int me;
};

void run(data_structure* q, int me) {
    Transaction::threadid = me;
    
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
            
            q->pop();
            q->pop();
        } RETRY(true)
    }
}

void* runFunc(void* x) {
    TesterPair* tp = (TesterPair*) x;
    run(tp->t, tp->me);
    return nullptr;
}
    
void startAndWait(int n, data_structure* queue) {
    pthread_t tids[n];
    TesterPair testers[n];
    for (int i = 0; i < n; ++i) {
        testers[i].t = queue;
        testers[i].me = i;
        pthread_create(&tids[i], NULL, runFunc, &testers[i]);
    }
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer);
    
    for (int i = 0; i < n; ++i) {
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
        Sto::set_transaction(&t1);
        q.pop();
        Sto::set_transaction(&t2);
        q.pop();
        assert(t1.try_commit());
        bool com = t2.try_commit();// TODO: this depends on the queue implementation
        assert(com);
        if (com) {
            Transaction t3;
            Sto::set_transaction(&t3);
            q.push(1);
            t3.try_commit();
        }
    }
    
    {
        // test nonabortion T1 pops, T2 pushes on nonempty q
        Transaction t1;
        Transaction t2;
        // q has 1 element
        Sto::set_transaction(&t1);
        q.pop();
        Sto::set_transaction(&t2);
        q.push(3);
        assert(t1.try_commit());
        assert(t2.try_commit()); // TODO: this also depends on queue implementation
        
        Transaction t3;
        Sto::set_transaction(&t3);
        q.push(3);
        assert(t3.try_commit());
        
    }
    // Nate: this used to be part of the above block but that doesn't make sense
    {
        Transaction t1;
        Sto::set_transaction(&t1);
        assert(q.top() == 3);
        q.pop();
        assert(q.top() == 3);
        q.pop(); // q is empty after this
        assert(t1.try_commit());
    }
    
    {
        // test abortion due to empty q pops
        Transaction t1;
        Transaction t2;
        // q has 0 elements
        Sto::set_transaction(&t1);
        q.pop(); // TODO: should support pop from empty queue in PriorityQueue
        q.push(1);
        q.push(2);
        Sto::set_transaction(&t2);
        q.push(3);
        q.push(4);
        q.push(5);
        
        q.pop();
        
        assert(t1.try_commit());
        assert(!t2.try_commit());
    }
    
    {
        // test nonabortion T1 pops/fronts and pushes, T2 pushes on nonempty q
        Transaction t1;
        Transaction t2;
        
        // q has 2 elements [2, 1]
        Sto::set_transaction(&t1);
        q.print();
        assert(q.top() == 2);
        q.push(4);
        
        // pop from non-empty q
        q.pop();
        assert(q.top() == 2);
        
        Sto::set_transaction(&t2);
        q.push(3);
        // order of pushes doesn't matter, commits succeed
        assert(t2.try_commit());
        assert(!t1.try_commit());
        
        // check if q is in order
        Transaction t;
        Sto::set_transaction(&t);
        q.print();
        assert(q.top() == 3);
        q.pop();
        assert(q.top() == 2);
        q.pop();
        assert(q.top() == 1);
        q.pop();
        assert(t.try_commit());
    }
}

int main() {
    queueTests();
    
    // Run a parallel test with lots of transactions doing pushes and pops
    data_structure q;
    startAndWait(N_THREADS, &q);
    
#if PERF_LOGGING
    Transaction::print_stats();
    {
        using thd = threadinfo_t;
        thd tc = Transaction::tinfo_combined();
        printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
    }
#endif

    
    if (false) {
    // check with sequential execution
    data_structure q1;
    for (int i = 0; i < N_THREADS; i++) {
        run(&q1, i);
    }
    std::cout << "Done " << std::endl;
    int size = q.size();
    
    std::cout << "size = " << size << std::endl;
    assert(size == q1.size());
    TRANSACTION {
        //q.print();
        //q1.print();
    } RETRY(false);
    for (int i = 0; i < size; i++) {
        TRANSACTION {
            assert(q.top() == q1.top());
            q.pop();
            q1.pop();
        } RETRY(false)
    }
    }
	return 0;
}
