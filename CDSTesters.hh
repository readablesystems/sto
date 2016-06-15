#include <string>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include <mutex>
#include "Transaction.hh"

#include "PriorityQueue.hh"
#include "Queue.hh"
#include "randgen.hh"
#include <cds/container/vyukov_mpmc_cycle_queue.h>
#include <cds/container/fcpriority_queue.h>

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

std::atomic<int> op_id(0);
std::mutex op_id_mtx;
std::vector<std::map<uint64_t, txn_record *> > txn_list;

typedef TransactionTid::type Version;
Version lock;

template <typename DT, typename RT>
class Tester {
public: 
    virtual ~Tester() {}
    // initialize the data structures
    // structure under test
    virtual void init_sut(DT* q) = 0;
    // reference structure
    virtual void init_ref(RT* q) = 0;
    // Perform a particular operation on the data structure.    
    virtual op_record* doOp(DT* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) = 0 ;
    // Redo a operation. This is called during serial execution.
    virtual void redoOp(RT* q, op_record *op) = 0;
    // Checks that final state of the two data structures are the same.
    virtual void check(DT* q, RT* q1) = 0;
};

template <typename DT, typename RT>
class QueueTester : Tester<DT, RT> {
public:
    void init_sut(DT* q) {
        for (int i = 0; i < 10000; i++) {
            TRANSACTION {
                q->push(i);
            } RETRY(false);
        }
    }
    void init_ref(RT* q) {
        for (int i = 0; i < 10000; i++) {
            TRANSACTION {
                q->transPush(i);
            } RETRY(false);
        }
    }

    op_record* doOp(DT* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) {
        int val;
        if (op == 0) {
            val = slotdist(transgen);
            q->push(val);
            std::cout << "[" << me << "] pushed " << val << std::endl;
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(val);
            return rec;
        } else {
            q->pop(val);
            std::cout << "[" << me << "] popped " << val << std::endl;
            op_record* rec = new op_record;
            rec->op = op;
            rec->rdata.push_back(val);
            return rec;
        }
    }

    void redoOp(RT* q, op_record *op) {
        int val = 0;
        if (op->op == 0) {
            val = op->args[0];
            q->transPush(val);
        } else if (op->op == 1) {
            q->transFront(val);
            assert(q->transPop());
            std::cout << "[" << val << "] [" << op->rdata[0] << "]" << std::endl;
            assert(val == op->rdata[0]);
        }
    }

    void check(DT* q, RT*q1) {
        TRANSACTION {
            int v1;
            if (q->pop(v1)) {
                int v2;
                q1->transFront(v2);
                q1->transPop();
                std::cout << "[" << v1 << "] [" << v2 << "]" << std::endl;
                assert(v1 == v2);
            } else {
                assert(!q1->transPop());
            }
        }
        RETRY(false);
    }

    static const int num_ops_ = 2;
};
template <typename DT, typename RT>
class PqueueTester : Tester<DT, RT> {
public:
    void init_sut(DT* q) {
        for (int i = 0; i < 10000; i++) {
            TRANSACTION {
                q->push(i);
            } RETRY(false);
        }
        assert(q->size() == 10000);
    }
    void init_ref(RT* q) {
        for (int i = 0; i < 10000; i++) {
            TRANSACTION {
                q->push(i);
            } RETRY(false);
        }
        assert(q->unsafe_size() == 10000);
    }

    op_record* doOp(DT* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) {
        if (op == 0) {
            int val = slotdist(transgen);
            q->push(val);
            std::cout << "[" << me << "] pushed " << val << std::endl;
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(val);
            return rec;
        } else {
            int val;
            q->pop(val);
            std::cout << "[" << me << "] popped " << val << std::endl;
            op_record* rec = new op_record;
            rec->op = op;
            rec->rdata.push_back(val);
            return rec;
        }
    }

    void redoOp(RT* q, op_record *op) {
        if (op->op == 0) {
            int val = op->args[0];
            q->push(val);
            std::cout << "pushed " << val << std::endl;
        } else if (op->op == 1) {
            int val = q->pop();
            std::cout << "popped " << val << std::endl;
            assert(val == op->rdata[0]);
        }
    }

    void check(DT* q, RT*q1) {
        int size = q1->unsafe_size();
        int size1 = q->size();
        std::cout << "[" << size << "] [" << size1 << "]" << std::endl;
        assert(size == size1);
        for (int i = 0; i < size; i++) {
            TRANSACTION {
                int v1;
                q->pop(v1);
                int v2 = q1->pop();
                std::cout << "[" << v1 << "] [" << v2 << "]" << std::endl;
                assert(v1 == v2);
            } RETRY(false);
        }
        TRANSACTION {
            assert((size_t)q->size() == (size_t)q1->unsafe_size());
        } RETRY(false);
    }

    static const int num_ops_ = 2;
};
