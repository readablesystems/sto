#pragma once

#include <cds/algo/elimination_opt.h> 
#include <cds/container/fcpriority_queue.h>
#include <cds/container/mspriority_queue.h>
#include <cds/container/basket_queue.h>
#include <cds/container/fcqueue.h>
#include <cds/container/moir_queue.h>
#include <cds/container/msqueue.h>
#include <cds/container/rwqueue.h>
#include <cds/container/optimistic_queue.h>
#include <cds/container/segmented_queue.h>
#include <cds/container/tsigas_cycle_queue.h>
#include <cds/container/vyukov_mpmc_cycle_queue.h>

#include "PriorityQueue.hh"
#include "PairingHeap.hh"
#include "Queue1.hh"
#include "Queue2.hh"
#include "QueueLP.hh"
#include "QueuePops.hh"
#include "FCQueueLP.hh"
#include "FCQueueNT.hh"
#include "FCQueueT.hh"
#include "FCQueueTPops.hh"
#include "cds_benchmarks.hh"

// value types
#define RANDOM_VALS 10
#define DECREASING_VALS 11

// types of q_operations available
std::atomic_int global_push_val(MAX_VALUE);
enum q_op {push, pop};
std::array<q_op, 2> q_ops_array = {push, pop};

#define FCQUEUE_TRAITS() cds::container::fcqueue::make_traits< \
    cds::opt::lock_type<cds::sync::spin>, \
    cds::opt::back_off<cds::backoff::delay_of<2>>, \
    cds::opt::allocator<CDS_DEFAULT_ALLOCATOR>, \
    cds::opt::stat<cds::container::fcqueue::stat<cds::atomicity::event_counter>>, \
    cds::opt::memory_model<cds::opt::v::relaxed_ordering>, \
    cds::opt::enable_elimination<false>>::type

#define FCPQUEUE_TRAITS() cds::container::fcqueue::make_traits< \
    cds::opt::lock_type<cds::sync::spin>, \
    cds::opt::back_off<cds::backoff::delay_of<2>>, \
    cds::opt::allocator<CDS_DEFAULT_ALLOCATOR>, \
    cds::opt::stat<cds::container::fcpqueue::stat<cds::atomicity::event_counter>>, \
    cds::opt::memory_model<cds::opt::v::relaxed_ordering>, \
    cds::opt::enable_elimination<false>>::type

template <typename DS> struct CDSQueueHarness { 
    typedef typename DS::value_type value_type;
public:
    CDSQueueHarness() {};
    CDSQueueHarness(size_t nCapacity) : v_(nCapacity) {};
    void init() { v_ = new DS(); }
    bool pop() { int ret; return v_->pop(ret); }
    void cleanup() { delete v_; }
    void push(value_type v) { assert(v_->push(v)); }
    void init_push(value_type v) { assert(v_->push(v)); }
    size_t size() { return v_->size(); }

    DS* v_;
};

template <typename DS> struct STOPQueueHarness { 
    typedef typename DS::value_type value_type;
public:
    void init() { v_ = new DS(); }
    bool pop() { return v_->pop(); }
    void cleanup() { v_ = new DS(); }
    void push(value_type v) { v_->push(v); }
    void init_push(value_type v) { 
        Sto::start_transaction();
        v_->push(v); 
        assert(Sto::try_commit());
    }
    size_t size() { return v_->unsafe_size(); }
private:
    DS* v_;
};

/* 
 * Priority Queue Templates
 */
template <typename T> struct DatatypeHarness<PriorityQueue<T>> : 
    public STOPQueueHarness<PriorityQueue<T>>{};
template <typename T> struct DatatypeHarness<PriorityQueue<T, true>> : 
    public STOPQueueHarness<PriorityQueue<T, true>>{};

template <typename T> struct DatatypeHarness<cds::container::MSPriorityQueue<T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(10000000) {};
    void init() { v_ = NULL; }
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
    size_t size() { return v_.size(); }
    cds::container::MSPriorityQueue<T> v_;
};

template <typename T> struct DatatypeHarness<cds::container::FCPriorityQueue<T, std::priority_queue<T>, FCPQUEUE_TRAITS()>> : 
    public CDSQueueHarness<cds::container::FCPriorityQueue<T, std::priority_queue<T>, FCPQUEUE_TRAITS()>>{
        void print_statistics() { this->v_.print_statistics(); }
    };

template <typename T> struct DatatypeHarness<cds::container::FCPriorityQueue<T, PairingHeap<T>>> : 
    public CDSQueueHarness<cds::container::FCPriorityQueue<T, PairingHeap<T>>>{
        void print_statistics() { this->v_.print_statistics(); }
    };

/* 
 * Queue Templates
 */
template <typename DS> struct STOQueueHarness {
    typedef typename DS::value_type value_type;
public:
    void init() {
        v_ = new DS();
    }
    bool pop() { return v_->pop(); }
    void cleanup() { 
        delete v_;
    }
    void push(value_type v) { v_->push(v); }
    void init_push(value_type v) { 
        Sto::start_transaction();
        v_->push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return 0; }
private:
    DS* v_;
};
template <typename T> struct DatatypeHarness<Queue1<T>> : public STOQueueHarness<Queue1<T>>{};
template <typename T> struct DatatypeHarness<Queue1<T, false>> : public STOQueueHarness<Queue1<T, false>>{};
template <typename T> struct DatatypeHarness<Queue2<T>> : public STOQueueHarness<Queue2<T>>{};
template <typename T> struct DatatypeHarness<Queue2<T, false>> : public STOQueueHarness<Queue2<T,false>>{};
template <typename T> struct DatatypeHarness<QueuePops<T>> : public STOQueueHarness<QueuePops<T>>{};
template <typename T> struct DatatypeHarness<QueuePops<T, false>> : public STOQueueHarness<QueuePops<T,false>>{};
template <typename T> struct DatatypeHarness<FCQueueT<T>> {
    typedef T value_type;
public:
    void init() {
        v_ = new FCQueueT<T>();
    }
    bool pop() { return v_->pop(); }
    void cleanup() { 
        delete v_;
    }
    void push(T v) { v_->push(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_->push(v);
        assert(Sto::try_commit());
    }

private:
    FCQueueT<T>* v_;
};
template <typename T> struct DatatypeHarness<FCQueueTPops<T>> {
    typedef T value_type;
public:
    void init() {
        v_ = new FCQueueTPops<T>();
    }
    bool pop() { return v_->pop(); }
    void cleanup() { 
        delete v_;
    }
    void push(T v) { v_->push(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_->push(v);
        assert(Sto::try_commit());
    }

private:
    FCQueueTPops<T>* v_;
};
template <typename T> struct DatatypeHarness<FCQueueLP<T>> { 
    typedef T value_type;
    typedef typename FCQueueLP<T>::pop_type pop_type;
public:
    void init() {
        v_ = new FCQueueLP<T>();
    }
    pop_type& pop() { return v_->pop(); }
    void cleanup() { 
        delete v_;
    }
    void push(value_type v) { v_->push(v); }
    void init_push(value_type v) { 
        Sto::start_transaction();
        v_->push(v);
        assert(Sto::try_commit());
    }
private:
    FCQueueLP<T>* v_;
};

template <typename T> struct DatatypeHarness<QueueLP<T, false>> {
    typedef T value_type;
    typedef typename QueueLP<T, false>::pop_type pop_type;
public:
    void init() {
        v_ = new QueueLP<T, false>();
    }
    pop_type& pop() { return v_->pop(); }
    void cleanup() { 
        delete v_;
    }
    void push(value_type v) { v_->push(v); }
    void init_push(value_type v) { 
        Sto::start_transaction();
        v_->push(v);
        assert(Sto::try_commit());
    }
private:
    QueueLP<T, false>* v_;
};


template <typename T> struct DatatypeHarness<FCQueueNT<T>> {
    typedef T value_type;
public:
    void init() {
        v_ = new FCQueueNT<T>();
    }
    bool pop() { return v_->pop(); }
    void cleanup() { 
        delete v_;
    }
    void push(T v) { v_->push(v); }
    void init_push(T v) { 
        v_->push(v);
    }

private:
    FCQueueNT<T>* v_;
};
template <typename T> struct DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, T>> : 
    public CDSQueueHarness<cds::container::BasketQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::MoirQueue<cds::gc::HP, T>> : 
    public CDSQueueHarness<cds::container::MoirQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::MSQueue<cds::gc::HP, T>> : 
    public CDSQueueHarness<cds::container::MSQueue<cds::gc::HP, T>> {};
template <typename T> struct DatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, T>> : 
    public CDSQueueHarness<cds::container::OptimisticQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::RWQueue<T>> : 
    public CDSQueueHarness<cds::container::RWQueue<T>>{};

template <typename T> struct DatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, T>> {
    typedef T value_type;
public:
    void init() {v_ = new cds::container::SegmentedQueue<cds::gc::HP, T>(32);}
    bool pop() { int ret; return v_->pop(ret); }
    void cleanup() { delete v_;}
    void push(value_type v) { assert(v_->push(v)); }
    void init_push(value_type v) { assert(v_->push(v)); }
    size_t size() { return v_->size(); }
private:
    cds::container::SegmentedQueue<cds::gc::HP, T>* v_;
};
template <typename T> struct DatatypeHarness<cds::container::TsigasCycleQueue<T>> {
    typedef T value_type;
public:
    void init() {v_ = new cds::container::TsigasCycleQueue<T>(1000000);}
    bool pop() { int ret; return v_->pop(ret); }
    void cleanup() { delete v_; v_ = new cds::container::TsigasCycleQueue<T>(1000000); }
    void push(value_type v) { assert(v_->push(v)); }
    void init_push(value_type v) { assert(v_->push(v)); }
    size_t size() { return v_->size(); }
private:
    cds::container::TsigasCycleQueue<T>* v_;
};

/*
 * Test interfaces and templates.
 */
template <typename DH> class QueueTest : public GenericTest {
public:
    QueueTest(int val_type) : val_type_(val_type){};
    void initialize(size_t init_sz) {
        v_.init();
        global_push_val = MAX_VALUE;
        // initialize with the maximum values, so that
        // at the beginning, pushes and pops will not conflict
        for (unsigned i = 0; i < init_sz; ++i) {
            v_.init_push(global_push_val--);
        }
    }

    void cleanup() {
        v_.cleanup();
    }

    inline void do_q_op(q_op q_op, int val) {
        switch(val_type_) {
            case RANDOM_VALS:
                break;
            case DECREASING_VALS:
                val = --global_push_val;
                break;
            default: assert(0);
        }
        switch (q_op) {
            case pop: v_.pop(); break;
            case push: v_.push(val); break;
            default: assert(0);
        }
    }
    inline void inc_ctrs(q_op q_op, int me) {
        switch(q_op) {
            case push: global_thread_ctrs[me].push++; break;
            case pop: global_thread_ctrs[me].pop++; break;
            default: assert(0);
        }
    }

    void print_fc_stats() {
        v_.print_statistics();
    }

protected:
    DH v_;
    int val_type_;
};

template <typename DH> class QSingleOpTest : public QueueTest<DH> {
public:
    QSingleOpTest(int ds_type, int val_type, q_op q_op) : QueueTest<DH>(val_type), q_op_(q_op), ds_type_(ds_type) {};
    void run(int me) {
        Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        std::uniform_int_distribution<long> slotdist(1, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        this->do_q_op(q_op_, slotdist(transgen));
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(q_op_, me);
            } else {
                this->do_q_op(q_op_, slotdist(transgen)); 
                this->inc_ctrs(q_op_, me);
            }
        }
    }
private:
    q_op q_op_;
    int ds_type_;
};

template <typename DH> class PushPopTest : public QueueTest<DH> {
public:
    PushPopTest(int ds_type, int val_type) : QueueTest<DH>(val_type), ds_type_(ds_type) {};
    void run(int me) {
        if (me > 1) { sleep(1); return; }
        Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        std::uniform_int_distribution<long> slotdist(1, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        this->do_q_op(q_ops_array[me % arraysize(q_ops_array)], slotdist(transgen));
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(q_ops_array[me % arraysize(q_ops_array)], me);
            } else {
                this->do_q_op(q_ops_array[me % arraysize(q_ops_array)], slotdist(transgen));
                this->inc_ctrs(q_ops_array[me % arraysize(q_ops_array)], me);
            }
        }
    }
private:
    int ds_type_;
};

template <typename DH> class RandomQSingleOpTest : public QueueTest<DH> {
public:
    RandomQSingleOpTest(int ds_type, int val_type) : QueueTest<DH>(val_type), ds_type_(ds_type) {};
    void run(int me) {
        q_op my_q_op;
        Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        std::uniform_int_distribution<long> slotdist(1, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        my_q_op = q_ops_array[slotdist(transgen) % arraysize(q_ops_array)];
                        this->do_q_op(my_q_op, slotdist(transgen)); 
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(my_q_op, me);
            } else {
                my_q_op = q_ops_array[slotdist(transgen) % arraysize(q_ops_array)];
                this->do_q_op(my_q_op, slotdist(transgen));
                this->inc_ctrs(my_q_op, me);
            }
        }
    }
private:
    int ds_type_;
};

template <typename DH> class GeneralTxnsTest : public QueueTest<DH> {
public:
    GeneralTxnsTest(int ds_type, int val_type, int num_ops) : 
        QueueTest<DH>(val_type), ds_type_(ds_type), num_ops_(num_ops) {};
    void run(int me) {
        q_op my_op;
        int numOps;
        int p;

        Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        std::uniform_int_distribution<long> opdist(0, 100);

        for (int i = NTRANS; i > 0; --i) {
            numOps = num_ops_ ? opdist(transgen) % num_ops_ + 1 : 0;
            p = opdist(transgen);
            if (p > 50) my_op = pop;
            else my_op = push;
            
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        for (int j = 0; j < numOps; ++j) {
                            this->do_q_op(my_op, j);
                            this->inc_ctrs(my_op, me); // XXX can lead to overcounting
                        }
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
            } else {
                for (int j = 0; j < numOps; ++j) {
                    this->do_q_op(my_op, j);
                    this->inc_ctrs(my_op, me);
                }
            }
        }
    }
    
private:
    int ds_type_;
    int num_ops_;
};

#define MAKE_PQUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO pqueue", new test<DatatypeHarness<PriorityQueue<type>>>(STO, ## __VA_ARGS__)},      \
    {desc, "STO pqueue opaque", new test<DatatypeHarness<PriorityQueue<type, true>>>(STO, ## __VA_ARGS__)},\
    {desc, "FC pqueue", new test<DatatypeHarness<cds::container::FCPriorityQueue<type, std::priority_queue<type>, FCPQUEUE_TRAITS()>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "FC pairing heap pqueue", new test<DatatypeHarness<cds::container::FCPriorityQueue<type, PairingHeap<type>>>>(CDS, ## __VA_ARGS__)} 
   // {desc, "MS pqueue", new test<DatatypeHarness<cds::container::MSPriorityQueue<type>>>(CDS, ## __VA_ARGS__)},

std::vector<Test> make_pqueue_tests() {
    return {
        MAKE_PQUEUE_TESTS("PQRandSingleOps:R", RandomQSingleOpTest, int, RANDOM_VALS),
        MAKE_PQUEUE_TESTS("PQRandSingleOps:D", RandomQSingleOpTest, int, DECREASING_VALS),
        MAKE_PQUEUE_TESTS("PQPushPop:R", PushPopTest, int, RANDOM_VALS),
        MAKE_PQUEUE_TESTS("PQPushPop:D", PushPopTest, int, DECREASING_VALS),
        //MAKE_PQUEUE_TESTS("PQPushOnly:R", QSingleOpTest, int, RANDOM_VALS, push),
        //MAKE_PQUEUE_TESTS("PQPushOnly:D", QSingleOpTest, int, DECREASING_VALS, push),
        //MAKE_PQUEUE_TESTS("General Txns Test with Random Vals", GeneralTxnsTest, int, RANDOM_VALS, q_txn_sets[0]),
        //MAKE_PQUEUE_TESTS("General Txns Test with Decreasing Vals", GeneralTxnsTest, int, DECREASING_VALS, q_txn_sets[0]),
    };
}
int num_pqueues = 4;

#define MAKE_QUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO1 queue", new test<DatatypeHarness<Queue1<type, false>>>(STO, ## __VA_ARGS__)},                                  \
    {desc, "STO2 queue", new test<DatatypeHarness<Queue2<type, false>>>(STO, ## __VA_ARGS__)},                                  \
    {desc, "NonTrans FC Queue", new test<DatatypeHarness<FCQueueNT<type>>>(CDS, ## __VA_ARGS__)},\
    {desc, "Basket queue", new test<DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},         \
    {desc, "Moir queue", new test<DatatypeHarness<cds::container::MoirQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "MS queue", new test<DatatypeHarness<cds::container::MSQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},   \
    {desc, "Opt queue", new test<DatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},   \
    {desc, "RW queue", new test<DatatypeHarness<cds::container::RWQueue<type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "Segmented queue", new test<DatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "TC queue", new test<DatatypeHarness<cds::container::TsigasCycleQueue<type>>>(CDS, ## __VA_ARGS__)} 

/*
#define MAKE_QUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO1 queue", new test<DatatypeHarness<Queue1<type, false>>>(STO, ## __VA_ARGS__)},      \
    {desc, "STO2 queue", new test<DatatypeHarness<Queue2<type, false>>>(STO, ## __VA_ARGS__)},      \
    {desc, "FCQueueNT", new test<DatatypeHarness<FCQueueNT<type>>>(CDS, ## __VA_ARGS__)},           \
    {desc, "Wrapped FCQueueNT", new test<DatatypeHarness<FCQueueNT<type>>>(STO, ## __VA_ARGS__)},   \
    {desc, "FCQueueT", new test<DatatypeHarness<FCQueueT<type>>>(STO, ## __VA_ARGS__)}, \
    {desc, "FCQueueLP", new test<DatatypeHarness<FCQueueLP<type>>>(STO, ## __VA_ARGS__)},\
    {desc, "STO2-QueueLP queue", new test<DatatypeHarness<QueueLP<type, false>>>(STO, ## __VA_ARGS__)}
             
    //{desc, "STOPops queue", new test<DatatypeHarness<QueuePops<type, false>>>(STO, ## __VA_ARGS__)},
*/
std::vector<Test> make_queue_tests() {
    return {
        MAKE_QUEUE_TESTS("Q:PushPop", PushPopTest, int, RANDOM_VALS),
        MAKE_QUEUE_TESTS("Q:RandSingleOps", RandomQSingleOpTest, int, RANDOM_VALS),
        //MAKE_QUEUE_TESTS("Q:RandMultiOps", GeneralTxnsTest, int, RANDOM_VALS, 5),
        //MAKE_QUEUE_TESTS("Q:RandMultiOps", GeneralTxnsTest, int, RANDOM_VALS, 10),
        //MAKE_QUEUE_TESTS("General Txns Test with Random Vals", GeneralTxnsTest, int, RANDOM_VALS, q_txn_sets[2]),
    };
}
int num_queues = 10;
//int num_queues = 7;
//int num_queues = 1;
