#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <map>
#include "Transaction.hh"
#include "PriorityQueue.hh"
#include "Hashtable.hh"
#include "RBTree.hh"
#include "Vector.hh"

#define MAX_VALUE 10 // Max value of integers used in data structures
#define PRINT_DEBUG 1 // Set this to 1 to print some debugging statements.

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

template <typename T>
class Tester {
public: 
    virtual ~Tester() {}
    // initialize the data structure
    virtual void init(T* q) = 0; 
    // Perform a particular operation on the data structure.    
    virtual op_record* doOp(T* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) = 0 ;
    // Redo a operation. This is called during serial execution.
    virtual void redoOp(T* q, op_record *op) = 0;
    // Checks that final state of the two data structures are the same.
    virtual void check(T* q, T* q1) = 0;
#if PRINT_DEBUG
    // Print stats for each data structure
    void print_stats(T* q) {};
#endif
};

template <typename T>
class RBTreeTester: Tester<T> {
public:
    void init(T* q) {
        for (int i = 0; i < MAX_VALUE; i++) {
            TRANSACTION {
                (*q)[i] = i;
            } RETRY(false);
        }
    }

    op_record* doOp(T* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) {
        int key = slotdist(transgen);
        if (op == 0) {
            int val = slotdist(transgen);
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to operator[] key " << key << " and val " << val << std::endl;
            TransactionTid::unlock(lock);
#endif
            (*q)[key] = val;
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] insert key " << key << " and val " << val << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(key);
            rec->args.push_back(val);
            return rec;
        } else if (op == 1) {
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to erase " << key<< std::endl;
            TransactionTid::unlock(lock);
#endif
            int num = q->erase(key);
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] erased " << num << " items with key " << key << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(key);
            rec->rdata.push_back(num);
            return rec;
        } else {
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to count " << key << std::endl;
            TransactionTid::unlock(lock);
#endif
            int num = q->count(key);
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] counted " << num << " items with key " << key << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(key);
            rec->rdata.push_back(num);
            return rec;
        }
    }

    void redoOp(T* q, op_record *op) {
        if (op->op == 0) {
            int key = op->args[0];
            int val = op->args[1];
            (*q)[key] = val;
#if PRINT_DEBUG
            std::cout << "inserting: " << key << ", " << val << std::endl;
#endif
        } else if (op->op == 1) {
            int key = op->args[0];
            auto erased = q->erase(key);
#if PRINT_DEBUG
            std::cout << "erasing: " << key << std::endl;
            std::cout << "erase actual: " << erased << std::endl;
            std::cout << "erase expected: " << op->rdata[0] << std::endl;
#endif
            assert (erased == op->rdata[0]);
        } else {
            int key = op->args[0];
            auto counted = q->count(key);
#if PRINT_DEBUG
            std::cout << "counting: " << key << std::endl;
            std::cout << "count actual: " << counted << std::endl;
            std::cout << "count expected: " << op->rdata[0] << std::endl;
#endif
            assert(q->count(key) == op->rdata[0]);
        }
    }

    void check(T* q, T*q1) {
        for (int i = 0; i < MAX_VALUE; i++) {
#if PRINT_DEBUG
            std::cout << "i is: " << i << std::endl;
#endif
            TRANSACTION {
                int c = q->count(i);
                int c1 = q1->count(i);
#if PRINT_DEBUG
                std::cout << "q count: " << c << std::endl;
                std::cout << "q1 count: " << c1 << std::endl;
#endif
                assert(c == c1);
                int v1 = (*q)[i];
                int v2 = (*q1)[i];
#if PRINT_DEBUG
                std::cout << "v1: " << v1 << std::endl;
                std::cout << "v2: " << v2 << std::endl;
#endif
                assert(v1 == v2);
                // this should always return 1 because we inserted an empty element if it
                // did not exist before
                int e = q->erase(i);
                int e1 = q1->erase(i);
#if PRINT_DEBUG
                std::cout << "q erased: " << e << std::endl;
                std::cout << "q1 erased: " << e1 << std::endl;
#endif
                assert(e == e1);
            } RETRY(false);
        }
        TRANSACTION {
            for (int i = 0; i < MAX_VALUE; i++) {
                assert(q->count(i) == 0);
            }
        } RETRY(false);
    }

#if PRINT_DEBUG
    void print_stats(T* q) {
        q->print_absent_reads();
    }
#endif

    static const int num_ops_ = 3;
};

template <typename T>
class PqueueTester : Tester<T> {
public:
    void init(T* q) {
        for (int i = 0; i < 10000; i++) {
            TRANSACTION {
                q->push(i);
            } RETRY(false);
        }
    }

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

    void redoOp(T* q, op_record *op) {
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

    void check(T* q, T*q1) {
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
            } RETRY(false);
        }
        TRANSACTION {
            q->print();
            assert(q->top() == -1);
            //q1.print();
        } RETRY(false);
    }

    static const int num_ops_ = 3;
};

template <typename T>
class HashtableTester : Tester<T> {
public:
    void init(T* q) {
        for (int i = 0; i < 10000; i++) {
            TRANSACTION {
                q->transPut(i, i);
            } RETRY(false);
        }
    }

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

    void check(T* q, T*q1) {
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
            } RETRY(false);
        }
    }
    
#if PRINT_DEBUG
    void print_stats(T* q) {
       
    }
#endif

    static const int num_ops_ = 3;
};

template <typename T>
class VectorTester : Tester<T> {
public:
    void init(T* q) {
        for (int i = 0; i < 1000; i++) {
            TRANSACTION {
                q->push_back(i);
            } RETRY(false);
        }
    }
    
    op_record* doOp(T* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) {
        if (op == 0) {
            int key = slotdist(transgen);
            int val = slotdist(transgen);
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to update " << key << ", " << val << std::endl;
            TransactionTid::unlock(lock);
#endif
            bool outOfBounds = false;
            try {
            q->transUpdate(key, val);
            } catch (OutOfBoundsException e) {
                outOfBounds = true;
            }
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] update " << !outOfBounds << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(key);
            rec->args.push_back(val);
            rec->rdata.push_back(outOfBounds);
            return rec;
        } else if (op == 1) {
            int key = slotdist(transgen);
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to read " << key << std::endl;
            TransactionTid::unlock(lock);
#endif
            int val;
            bool outOfBounds = false;
            try {
            val = q->transGet(key);
            } catch (OutOfBoundsException e) {
                outOfBounds = true;
            }
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] read (" << !outOfBounds << ") " << key << ", " << val << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(key);
            rec->rdata.push_back(val);
            rec->rdata.push_back(outOfBounds);
            return rec;
        } else if (op == 2) {
            int val = slotdist(transgen);
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to push " << val << std::endl;
            TransactionTid::unlock(lock);
#endif
            q->push_back(val);
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] pushed " << val  << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(val);
            return rec;
        } else if (op == 3) {
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to pop " << std::endl;
            TransactionTid::unlock(lock);
#endif
            int val;
            int sz = q->transSize();
            if (sz > 0) {
              val = q->transGet(sz - 1);
              q->pop_back();
            }
            
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] popped "  << sz-1 << " " << val << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->rdata.push_back(val);
            rec->rdata.push_back(sz > 0);
            return rec;

        } else {
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try size " << std::endl;
            TransactionTid::unlock(lock);
#endif
            int sz = q->transSize();
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] size "  << sz << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->rdata.push_back(sz);
            return rec;

        }
    }
    
    void redoOp(T* q, op_record* op) {
        if (op->op == 0) {
            int key = op->args[0];
            int val = op->args[1];
            int size = q->transSize();
            if (op->rdata[0]) { assert(key >= size); return; }
            assert(key < size);
            q->transUpdate(key, val);
        } else if (op->op == 1) {
            int key = op->args[0];
            int size = q->transSize();
            if (op->rdata[1]) { assert(key >= size); return; }
            assert(key < size);
            int val = q->transGet(key);
            assert(val == op->rdata[0]);
        } else if (op->op == 2){
            int val = op->args[0];
            q->push_back(val);
        } else if (op->op == 3) {
            int size = q->transSize();
            if (!op->rdata[1]) { assert(size == 0); return;}
            assert(size > 0);
            assert(q->transGet(size - 1) == op->rdata[0]);
            q->pop_back();
        } else {
            assert(q->transSize() == op->rdata[0]);
        }
    }
    
    void check(T* q, T*q1) {
        int size;
        TRANSACTION {
            size = q->transSize();
            assert(size == q1->transSize());
        } RETRY(false);
        for (int i = 0; i < size; i++) {
            TRANSACTION {
                assert(q->transGet(i) == q1->transGet(i));
            } RETRY(false);
        }
    }
    
#if PRINT_DEBUG
    void print_stats(T* q) {
        
    }
#endif
    
    static const int num_ops_ = 5;
};

