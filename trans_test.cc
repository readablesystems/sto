#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "PriorityQueue.hh"

#define GLOBAL_SEED 10
#define MAX_VALUE  100000
#define NTRANS 1000
#define N_THREADS 4
#define MAX_OPS 1

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

struct op_record {
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


static const int num_ops = 3;

template <typename T>
op_record* doOp(T* q, int op, std::uniform_int_distribution<long> slotdist, Rand transgen) {
    if (op == 0) {
        int val = slotdist(transgen);
        q->push(val);
        op_record* rec = new op_record;
        rec->op = op;
        rec->args.push_back(val);
        return rec;
    } else if (op == 1) {
        int val = q->pop();
        op_record* rec = new op_record;
        rec->op = op;
        rec->rdata.push_back(val);
        return rec;
    } else {
        int val = q->top();
        op_record* rec = new op_record;
        rec->op = op;
        rec->rdata.push_back(val);
        return rec;
    }
}

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
            for (int i = 0; i < numOps; i++) {
                int op = slotdist(transgen) % num_ops;
                tr->ops.push_back(doOp(q, op, slotdist, transgen));
            }
        }
        if (Sto::try_commit()) {
            txn_list[me][Sto::commit_tid()] = tr;
            break;
        }
        
        } catch (Transaction::Abort e) {}
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

int main() {
    lock = 0;
    data_structure q;
    for (int i = 0; i < 10000; i++) {
        TRANSACTION {
            q.push(i);
        } RETRY(false);
    }
    
    
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
    for (int i = 0; i < 10000; i++) {
        TRANSACTION {
            q1.push(i);
        } RETRY(false);
    }
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
