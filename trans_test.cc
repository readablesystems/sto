#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "PriorityQueue.hh"
#include "Hashtable.hh"

#define GLOBAL_SEED 10
#define MAX_VALUE  100000 // Max value of integers used in data structures
#define NTRANS 100000 // Number of transactions each thread should run.
#define N_THREADS 4 // Number of concurrent threads
#define MAX_OPS 3 // Maximum number of operations in a transaction.
#define PRINT_DEBUG 0 // Set this to 1 to print some debugging statements.

#define PRIORITY_QUEUE 0
#define HASHTABLE 1

#define DS HASHTABLE

#if DS == PRIORITY_QUEUE
typedef PriorityQueue<int> data_structure;
#endif
#if DS == HASHTABLE
typedef Hashtable<int, int, false, 1000000> data_structure;
#endif

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

struct op_record {
    // Keeps track of a transactional operation, arguments used, and any read data.
    int op;
    std::vector<int> args;
    std::vector<int> rdata;
};

struct txn_record {
    // keeps track of operations in a single transaction
    std::vector<op_record*> ops;
};

template <typename T>
struct TesterPair {
    T* t;
    int me;
};

std::vector<std::map<uint64_t, txn_record *> > txn_list;

typedef TransactionTid::type Version;
Version lock;

// Number of operations supported on the data structure.

#if DS == PRIORITY_QUEUE
static const int num_ops = 3;
#endif
#if DS == HASHTABLE
static const int num_ops = 3;
#endif

//Perform a particular operation on the data structure.
// Change this method inorder to test a different data structure.

#if DS == PRIORITY_QUEUE
template <typename T>
op_record* doOp(T* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) {
    if (op == 0) {
        int val = slotdist(transgen);
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] try to push " << val << std::endl;
        TransactionTid::unlock(lock);
#endif
        q->push(val);
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] pushed " << val << std::endl;
        TransactionTid::unlock(lock);
#endif
        op_record* rec = new op_record;
        rec->op = op;
        rec->args.push_back(val);
        return rec;
    } else if (op == 1) {
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] try to pop " << std::endl;
        TransactionTid::unlock(lock);
#endif
        int val = q->pop();
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] popped " << val << std::endl;
        TransactionTid::unlock(lock);
#endif
        op_record* rec = new op_record;
        rec->op = op;
        rec->rdata.push_back(val);
        return rec;
    } else {
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] try to read " << std::endl;
        TransactionTid::unlock(lock);
#endif
        int val = q->top();
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] read " << val << std::endl;
        TransactionTid::unlock(lock);
#endif
        op_record* rec = new op_record;
        rec->op = op;
        rec->rdata.push_back(val);
        return rec;
    }
}
#endif
#if DS == HASHTABLE
template <typename T>
op_record* doOp(T* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) {
    if (op == 0) {
        int key = slotdist(transgen);
        int val = slotdist(transgen);
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] try to put " << key << ", " << val << std::endl;
        TransactionTid::unlock(lock);
#endif
        bool present = q->transPut(key, val);
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] put " << key << "," << val << std::endl;
        TransactionTid::unlock(lock);
#endif
        op_record* rec = new op_record;
        rec->op = op;
        rec->args.push_back(key);
        rec->args.push_back(val);
        rec->rdata.push_back(present);
        return rec;
    } else if (op == 1) {
        int key = slotdist(transgen);
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] try to get " << key << std::endl;
        TransactionTid::unlock(lock);
#endif
        int val;
        bool present = q->transGet(key, val);
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] read " << key << ", " << val << std::endl;
        TransactionTid::unlock(lock);
#endif
        op_record* rec = new op_record;
        rec->op = op;
        rec->args.push_back(key);
        rec->rdata.push_back(val);
        rec->rdata.push_back(present);
        return rec;
    } else {
        int key = slotdist(transgen);
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] try to delete " << key << std::endl;
        TransactionTid::unlock(lock);
#endif
        bool success = q->transDelete(key);
#if PRINT_DEBUG
        TransactionTid::lock(lock);
        std::cout << "[" << me << "] deleted " << key << ", " << success << std::endl;
        TransactionTid::unlock(lock);
#endif
        op_record* rec = new op_record;
        rec->op = op;
        rec->args.push_back(key);
        rec->rdata.push_back(success);
        return rec;
    }
}

#endif


//Redo a operation. This is called during serial execution.
// Change this method to test a different data structure.
#if DS == PRIORITY_QUEUE
template <typename T>
void redoOp(T* q, op_record* op) {
    if (op->op == 0) {
        int val = op->args[0];
        q->push(val);
    } else if (op->op == 1) {
        int val = q->pop();
        assert(val == op->rdata[0]);
    } else {
        int val = q->top();
        assert(val == op->rdata[0]);
    }
}
#endif
#if DS == HASHTABLE
template <typename T>
void redoOp(T* q, op_record* op) {
    if (op->op == 0) {
        int key = op->args[0];
        int val = op->args[1];
        bool present = q->transPut(key, val);
        assert(present == op->rdata[0]);
    } else if (op->op == 1) {
        int key = op->args[0];
        int val;
        bool present = q->transGet(key, val);
        assert(present == op->rdata[1]);
        if (present)
            assert(val == op->rdata[0]);
    } else {
        int key = op->args[0];
        bool success = q->transDelete(key);
        assert(success == op->rdata[0]);
    }
}
#endif


template <typename T>
void run(T* q, int me) {
    Transaction::threadid = me;
    
    std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
    for (int i = 0; i < NTRANS; ++i) {
        // so that retries of this transaction do the same thing
        auto transseed = i;
        txn_record *tr = new txn_record;
        TRANSACTION {
            tr->ops.clear();
            
            uint32_t seed = transseed*3 + (uint32_t)me*NTRANS*7 + (uint32_t)GLOBAL_SEED*MAX_THREADS*NTRANS*11;
            auto seedlow = seed & 0xffff;
            auto seedhigh = seed >> 16;
            Rand transgen(seed, seedlow << 16 | seedhigh);
            
            int numOps = slotdist(transgen) % MAX_OPS + 1;
            
            for (int j = 0; j < numOps; j++) {
                int op = slotdist(transgen) % num_ops;
                tr->ops.push_back(doOp(q, op, me, slotdist, transgen));
            }
        }
        if (Sto::try_commit()) {
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] committed " << Sto::commit_tid() << std::endl;
            TransactionTid::unlock(lock);
#endif
            txn_list[me][Sto::commit_tid()] = tr;
            break;
        } else {
#if PRINT_DEBUG
            TransactionTid::lock(lock); std::cout << "[" << me << "] aborted "<< std::endl; TransactionTid::unlock(lock);
#endif
        }
        
    } catch (Transaction::Abort e) {
#if PRINT_DEBUG
        TransactionTid::lock(lock); std::cout << "[" << me << "] aborted "<< std::endl; TransactionTid::unlock(lock);
#endif
    }

    }
    }
}

template <typename T>
void* runFunc(void* x) {
    TesterPair<T>* tp = (TesterPair<T>*) x;
    run(tp->t, tp->me);
    return nullptr;
}


template <typename T>
void startAndWait(T* queue) {
    pthread_t tids[N_THREADS];
    TesterPair<T> testers[N_THREADS];
    for (int i = 0; i < N_THREADS; ++i) {
        testers[i].t = queue;
        testers[i].me = i;
        pthread_create(&tids[i], NULL, runFunc<T>, &testers[i]);
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

// Checks that final state of the two data structures are the same.
// Change this method to test a different data structure.

#if DS == PRIORITY_QUEUE
template <typename T>
void check (T* q, T* q1) {
    int size = q1->size();
    for (int i = 0; i < size; i++) {
        TRANSACTION {
            int v1 = q->top();
            int v2 = q1->top();
            //std::cout << v1 << " " << v2 << std::endl;
            if (v1 != v2)
                q1->print();
            assert(v1 == v2);
            assert(q->pop() == v1);
            assert(q1->pop() == v2);
        } RETRY(false)
    }
    TRANSACTION {
        q->print();
        assert(q->top() == -1);
        //q1.print();
    } RETRY(false);
}
#endif
#if DS == HASHTABLE
template <typename T>
void check (T* q, T* q1) {
    for (int i = 0; i < MAX_VALUE; i++) {
        TRANSACTION {
            int v1;
            bool p1 = q->transGet(i, v1);
            int v2;
            bool p2 = q1->transGet(i, v2);
            assert(p1 == p2);
            if (p1) {
                assert(v1 == v2);
            }
        } RETRY(false)
    }
}
#endif

// Initialize the data structure.
#if DS == PRIORITY_QUEUE
template <typename T>
void init(T* q) {
    for (int i = 0; i < 10000; i++) {
        TRANSACTION {
            q->push(i);
        } RETRY(false);
    }
}
#endif
#if DS == HASHTABLE
template <typename T>
void init(T* q) {
    for (int i = 0; i < 10000; i++) {
        TRANSACTION {
            q->transPut(i, i);
        } RETRY(false);
    }
}
#endif


int main() {
    lock = 0;
    data_structure q;
    
    init(&q);
    
    struct timeval tv1,tv2;
    gettimeofday(&tv1, NULL);
    
    for (int i = 0; i < N_THREADS; i++) {
        txn_list.emplace_back();
    }
    
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
    
    
    std::map<uint64_t, txn_record *> combined_txn_list;
    
    for (int i = 0; i < N_THREADS; i++) {
        combined_txn_list.insert(txn_list[i].begin(), txn_list[i].end());
    }
    
    std::cout << "Single thread replay" << std::endl;
    data_structure q1;
    init(&q1);
    gettimeofday(&tv1, NULL);
    
    std::map<uint64_t, txn_record *>::iterator it = combined_txn_list.begin();
    for(; it != combined_txn_list.end(); it++) {
        Sto::start_transaction();
        for (int i = 0; i < it->second->ops.size(); i++) {
            redoOp(&q1, it->second->ops[i]);
        }
        assert(Sto::try_commit());
    }
    
    gettimeofday(&tv2, NULL);
    printf("Serial time: ");
    print_time(tv1, tv2);
    
   
    check(&q, &q1);
	return 0;
}
