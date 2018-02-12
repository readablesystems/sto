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
#define PRINT_DEBUG 1 // Set this to 1 to print some debugging statements

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
#if PRINT_DEBUG
    // Print stats for each data structure
    //void print_stats(T* q) {};
#endif
};

template <typename DT, typename RT>
class RBTreeTester: Tester<DT, RT> {
public:
    template <typename T>
    void init(T* q) {
        for (int i = 0; i < MAX_VALUE; i++) {
            TRANSACTION {
                (*q)[i] = i;
            } RETRY(false);
        }
    }

    void init_sut(DT* q) {init<DT>(q);}
    void init_ref(RT* q) {init<RT>(q);}

    op_record* doOp(DT* q, int op, int me, std::uniform_int_distribution<long> slotdist, Rand transgen) {
#if !PRINT_DEBUG
        (void)me;
#endif
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
        } else if (op == 2) {
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
        } else if (op == 3) {
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to size" << std::endl;
            TransactionTid::unlock(lock);
#endif
            size_t size = q->size();
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] size " << size << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->rdata.push_back(size);
            return rec;
        } /*else if (op == 4) {
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to iterator* start" << std::endl;
            TransactionTid::unlock(lock);
#endif
            if (q->size() == 0) {
#if PRINT_DEBUG
                TransactionTid::lock(lock);
                std::cout << "[" << me << "] tried to *start empty tree" << std::endl;
                TransactionTid::unlock(lock);
#endif
                op_record* rec = new op_record;
                rec->op = op;
                rec->rdata.push_back(-1);
                return rec;
            }
            auto it = q->begin();
            int val = it->second;
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] found value " << val << " at start" << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->rdata.push_back(val);
            return rec;
        } else if (op == 5) {
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to iterator* --end" << std::endl;
            TransactionTid::unlock(lock);
#endif
            if (q->size() == 0) {
#if PRINT_DEBUG
                TransactionTid::lock(lock);
                std::cout << "[" << me << "] tried to -- empty tree" << std::endl;
                TransactionTid::unlock(lock);
#endif
                op_record* rec = new op_record;
                rec->op = op;
                rec->rdata.push_back(-1);
                return rec;
            }
            auto it = --(q->end());
            int val = it->second;
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] found value " << val << " at end" << std::endl;
            TransactionTid::unlock(lock);
#endif
            op_record* rec = new op_record;
            rec->op = op;
            rec->rdata.push_back(val);
            return rec;
        } else if (op == 6) {
            int forward = slotdist(transgen);
            int backward = slotdist(transgen);
            backward = (forward < backward) ? forward : backward;
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] try to * " << forward << " forward and " << backward << " backward" << std::endl;
            TransactionTid::unlock(lock);
#endif
            auto it = q->begin();
            if (q->size() == 0) {
#if PRINT_DEBUG
                TransactionTid::lock(lock);
                std::cout << "[" << me << "] tried to iterate empty tree" << std::endl;
                TransactionTid::unlock(lock);
#endif
                op_record* rec = new op_record;
                rec->op = op;
                rec->rdata.push_back(-1);
                return rec;
            }
            for (int i = 0; i <= forward && it != q->end(); i++, it++) {}
            for (int i = backward; i > 0 && it != q->begin(); i--, it--) {}
            if (it == q->end()) {
                it = q->begin();
                assert(it != q->end());
#if PRINT_DEBUG
                TransactionTid::lock(lock);
                std::cout << "[" << me << "] \t iterator was at end" << std::endl;
                TransactionTid::unlock(lock);
#endif
            }
            int val = it->second;
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] found value " << val << " @ " << forward << " - " << backward<< std::endl;
            TransactionTid::unlock(lock);
#endif
            // comment block start
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] redoing "<<  forward << " forward and " << backward << " backward with prefix"<< std::endl;
            TransactionTid::unlock(lock);
#endif
            auto it2 = q->begin();
            tmp = it2;
            for (int i = 0; i <= forward && it2 != q->end(); i++, ++it2) {
                tmp = it2;
            }

            for (int i = backward; i > 0 && it2 != q->begin(); i--, --it2) {
                tmp = it2;
            }
            int val2 = *tmp;
#if PRINT_DEBUG
            TransactionTid::lock(lock);
            std::cout << "[" << me << "] found value2 " << val2 << " @ " << forward << " - " << backward << std::endl;
            TransactionTid::unlock(lock);
#endif
            assert(val == val2);
            // comment block end
            op_record* rec = new op_record;
            rec->op = op;
            rec->args.push_back(forward);
            rec->args.push_back(backward);
            rec->rdata.push_back(val);
            return rec;
        }*/
        return nullptr;
    }

    void redoOp(RT* q, op_record *op) {
        if (op->op == 0) {
            int key = op->args[0];
            int val = op->args[1];
            (*q)[key] = val;
#if PRINT_DEBUG
            std::cout << "inserting: " << key << ", " << val << std::endl;
#endif
        } else if (op->op == 1) {
            int key = op->args[0];
            int erased = q->erase(key);
#if PRINT_DEBUG
            std::cout << "erasing: " << key << std::endl;
            std::cout << "erase replay: " << erased << std::endl;
            std::cout << "erase expected: " << op->rdata[0] << std::endl;
#endif
            assert (erased == op->rdata[0]);
        } else if (op->op == 2) {
            int key = op->args[0];
            int counted = q->count(key);
#if PRINT_DEBUG
            std::cout << "counting: " << key << std::endl;
            std::cout << "count replay: " << counted << std::endl;
            std::cout << "count expected: " << op->rdata[0] << std::endl;
#endif
            assert(counted == op->rdata[0]);
        } else if (op->op == 3) {
            size_t size = q->size();
#if PRINT_DEBUG
            std::cout << "size replay: " << size << std::endl;
            std::cout << "size expected: " << op->rdata[0] << std::endl;
#endif
            assert(size == (size_t) op->rdata[0]);
        } /*else if (op->op == 4) {
            if (q->size() == 0) {
                assert(op->rdata[0] == -1);
                return;
            }
            auto it = q->begin();
            int val = it->second;
#if PRINT_DEBUG
            std::cout << "*it start replay: " << val << std::endl;
            std::cout << "*it start expected: " << op->rdata[0]  << std::endl;
#endif
            assert(val == op->rdata[0]);
        } else if (op->op == 5) {
            auto it = q->end();
            // deal with empty tree end()
            if (q->size() == 0) {
                assert(op->rdata[0] == -1);
                return;
            }
            auto val1 = (--it)->second;
            assert(++it == q->end());
            it--;
            auto val2 = it->second;
#if PRINT_DEBUG
            std::cout << "*it end-1 replay: " << val1 << std::endl;
            std::cout << "*it end-1 expected: " << op->rdata[0]  << std::endl;
#endif
            assert(val1 == val2);
            assert(val1 == op->rdata[0]);
        } else if (op->op == 6) {
            auto it = q->begin();
            // deal with empty tree begin()
            if (q->size() == 0) {
                assert(op->rdata[0] == -1);
                return;
            }
            int forward = op->args[0];
            int backward = op->args[1];
            for (int i = 0; i <= forward && it != q->end(); i++, it++) {}
            for (int i = backward; i > 0 && it != q->begin(); i--, it--) {}
            if (it == q->end()) it = q->begin();
            int val = it->second;
#if PRINT_DEBUG
            std::cout << "*it replay at place " << forward << " - " << backward << ": " << val << std::endl;
            std::cout << "*it expected at place " << forward << " - " << backward << ": " << op->rdata[0]  << std::endl;
#endif
            assert(val == op->rdata[0]);
        }*/
    }

    void check(DT* q, RT*q1) {
        for (int i = 0; i < MAX_VALUE; i++) {
#if PRINT_DEBUG
            std::cout << "i is: " << i << std::endl;
#endif
            TRANSACTION {
/*
                if (q->size() != 0) {
                    auto it = q->begin();
                    auto it1 = q1->begin();
                    for (; (it != q->end() || it1 != q1->end()); it++, it1++) {
                        assert(it->second == it1->second);
                    }
                }
*/
                size_t s = q->size();
                size_t s1 = q1->size();
#if PRINT_DEBUG
                std::cout << "q size: " << std::endl;
                std::cout << "q1 size: " << s1 << std::endl;
#endif
                assert(s == s1);
                size_t c = q->count(i);
                size_t c1 = q1->count(i);
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

                s = q->size();
                s1 = q1->size();
#if PRINT_DEBUG
                std::cout << "q size: " << s << std::endl;
                std::cout << "q1 size: " << s1 << std::endl;
#endif
                assert(s == s1);

                // this should always return 1 because we inserted an empty element if it
                // did not exist before
                size_t e = q->erase(i);
                size_t e1 = q1->erase(i);
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
    void print_stats(DT* q) {
        (void)q;
        //q->print_absent_reads();
    }
#endif

    static const int num_ops_ = 4;
};

/*
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
#if !PRINT_DEBUG
        (void)me;
#endif
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
#if !PRINT_DEBUG
        (void)me;
#endif
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
#if !PRINT_DEBUG
        (void)me;
#endif
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
            } catch (const std::out_of_range& e) {
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
            } catch (std::out_of_range& e) {
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
            int sz = q->size();
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
            int sz = q->size();
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
            int size = q->size();
            if (op->rdata[0]) { assert(key >= size); return; }
            assert(key < size);
            q->transUpdate(key, val);
        } else if (op->op == 1) {
            int key = op->args[0];
            int size = q->size();
            if (op->rdata[1]) { assert(key >= size); return; }
            assert(key < size);
            int val = q->transGet(key);
            assert(val == op->rdata[0]);
        } else if (op->op == 2){
            int val = op->args[0];
            q->push_back(val);
        } else if (op->op == 3) {
            int size = q->size();
            if (!op->rdata[1]) { assert(size == 0); return;}
            assert(size > 0);
            assert(q->transGet(size - 1) == op->rdata[0]);
            q->pop_back();
        } else {
            assert(q->size() == op->rdata[0]);
        }
    }
    
    void check(T* q, T*q1) {
        int size;
        TRANSACTION {
            size = q->size();
            assert(size == q1->size());
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
*/
