#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <random>
#include <climits>

#include <cds/init.h>
#include <cds/container/fcpriority_queue.h>
#include <cds/container/mspriority_queue.h>

#include "Transaction.hh"
#include "PriorityQueue.hh"
#include "randgen.hh"

#define GLOBAL_SEED 10

#define MAX_VALUE  100000
#define MAX_SIZE 100000
#define NTRANS 20000 // Number of transactions each thread should run.
#define N_THREADS 4 // Number of concurrent threads

// type of data structure to be used
#define STO 0
#define CDS 1

// type of benchmark to use
#define RANDOM 10
#define DECREASING 11
#define NOABORTS 12

std::atomic_int global_val(INT_MAX);

enum op {push, pop};

std::vector<int> sizes = {100, 1000, 10000};

template <typename T>
struct Tester {
    T* ds;          // pointer to the data structure
    int me;         // tid
    int ds_type;    // cds or sto
    int bm;         // which benchmark to run
    size_t size;    // initial size of the ds 
    std::vector<std::vector<op>> txn_set;
};

std::vector<std::vector<op>> q_single_op_txn_set = {{push}, {pop}};

// set of transactions to choose from
// approximately equivalent pushes and pops
std::vector<std::vector<op>> q_txn_sets[] = 
{
    // 0. only-growing data structure (no conflicts)
    {
        {push, push, push},
        {push, push},
        {push}, {push}, {push}
    },
    // 1. only-shrinking data structure
    {
        {pop, pop, pop},
        {pop, pop},
        {pop}, {pop}, {pop}
    },
    // 2. short txns
    {
        {push, push, push},
        {pop, pop, pop},
        {pop}, {pop}, {pop},
        {push}, {push}, {push}
    },
    // 3. longer txns
    {
        {push, push, push, push, push},
        {pop, pop, pop, pop, pop},
        {pop}, {pop}, {pop}, {pop}, {pop}, 
        {push}, {push}, {push}, {push}, {push}
    },
    // 4. 100% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
    },
    // 5. 50% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
        {pop}, {push}
    },
    // 6. 33% include both pushes and pops
    {
        {push, push, pop},
        {pop, pop, push},
        {pop}, {pop},
        {push}, {push}
    },
    // 7. 33%: longer push + pop txns
    {
        {push, pop, push, pop, push, pop},
        {pop}, 
        {push}
    }
};

/* 
 * PQUEUE WRAPPERS
 */
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
