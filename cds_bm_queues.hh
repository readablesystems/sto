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
#include "Queue.hh"
#include "Queue2.hh"
#include "cds_benchmarks.hh"

// value types
#define RANDOM_VALS 10
#define DECREASING_VALS 11

// types of q_operations available
enum q_op {push, pop};
std::array<q_op, 2> q_ops_array = {push, pop};

// txn sets
std::vector<std::vector<std::vector<q_op>>> q_txn_sets = {
    // 0. short txns
    {{push, push, push}, {pop, pop, pop}, {pop}, {pop}, {pop}, {push}, {push}, {push}},
    // 1. longer txns
    {{push, push, push, push, push},{pop, pop, pop, pop, pop}}, 
    // 2. 100% include both pushes and ptps
    {{push, push, pop},{pop, pop, push}},
    // 3. 50% include both pushes and pops
    {{push, push, pop},{pop, pop, push},{pop}, {push}},
    // 4. 33% include both pushes and pops
    {{push, push, pop},{pop, pop, push},{pop}, {pop},{push}, {push}},
    // 5. 33%: longer push + pop txns
    {{push, pop, push, pop, push, pop},{pop}, {push}},
    // 6. one-q_op txns
    {{pop}, {push}}
};

#define FCQUEUE_TRAITS() cds::container::fcqueue::make_traits< \
    cds::q_opt::lock_type<cds::sync::spin>, \
    cds::q_opt::back_off<cds::backoff::delay_of<2>>, \
    cds::q_opt::allocator<CDS_DEFAULT_ALLOCATOR>, \
    cds::q_opt::stat<cds::container::fcqueue::stat<cds::atomicity::event_counter>>, \
    cds::q_opt::memory_model<cds::q_opt::v::relaxed_ordering>, \
    cds::q_opt::enable_elimination<false>>::type

#define FCPQUEUE_TRAITS() cds::container::fcqueue::make_traits< \
    cds::q_opt::lock_type<cds::sync::spin>, \
    cds::q_opt::back_off<cds::backoff::delay_of<2>>, \
    cds::q_opt::allocator<CDS_DEFAULT_ALLOCATOR>, \
    cds::q_opt::stat<cds::container::fcpqueue::stat<cds::atomicity::event_counter>>, \
    cds::q_opt::memory_model<cds::q_opt::v::relaxed_ordering>, \
    cds::q_opt::enable_elimination<false>>::type

template <typename DS> struct CDSQueueHarness { 
    typedef typename DS::value_type value_type;
public:
    CDSQueueHarness() {};
    CDSQueueHarness(size_t nCapacity) : v_(nCapacity) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
    size_t size() { return v_.size(); }

    DS v_;
};

template <typename DS> struct STOQueueHarness { 
    typedef typename DS::value_type value_type;
public:
    bool pop() { return v_.pop(); }
    bool cleanup_pop() { 
        if (v_.unsafe_size() > 0) {
            Sto::start_transaction();
            v_.pop();
            assert(Sto::try_commit());
            return true;
        } else return false;
    }
    void push(value_type v) { v_.push(v); }
    void init_push(value_type v) { 
        Sto::start_transaction();
        v_.push(v); 
        assert(Sto::try_commit());
    }
    size_t size() { return v_.unsafe_size(); }
private:
    DS v_;
};

/* 
 * Priority Queue Templates
 */
template <typename T> struct DatatypeHarness<FCPriorityQueue<T>> : 
    public STOQueueHarness<FCPriorityQueue<T>>{};
template <typename T> struct DatatypeHarness<PriorityQueue<T>> : 
    public STOQueueHarness<PriorityQueue<T>>{};
template <typename T> struct DatatypeHarness<PriorityQueue<T, true>> : 
    public STOQueueHarness<PriorityQueue<T, true>>{};
template <typename T> struct DatatypeHarness<PriorityQueue2<T>> : 
    public STOQueueHarness<PriorityQueue2<T>>{};
template <typename T> struct DatatypeHarness<PriorityQueue2<T, true>> : 
    public STOQueueHarness<PriorityQueue2<T, true>>{};

template <typename T> struct DatatypeHarness<cds::container::MSPriorityQueue<T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(10000000) {};
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
template <typename T> struct DatatypeHarness<Queue<T>> {
    typedef T value_type;
public:
    bool pop() { return v_.pop(); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        bool ret = pop();
        assert(Sto::try_commit());
        return ret;
    }
    void push(T v) { v_.push(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_.push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return 0; }
private:
    Queue<T> v_;
};
template <typename T> struct DatatypeHarness<Queue2<T>> {
    typedef T value_type;
public:
    bool pop() { return v_.pop(); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        bool ret = pop();
        assert(Sto::try_commit());
        return ret;
    }
    void push(T v) { v_.push(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_.push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return 0; }
private:
    Queue2<T> v_;
};
template <typename T> struct DatatypeHarness<FCQueue<T>> {
    typedef T value_type;
public:
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { 
        Sto::start_transaction();
        bool r = pop();
        assert(Sto::try_commit());
        return r;
    }
    void push(T v) { v_.push(v); }
    void init_push(T v) { 
        Sto::start_transaction();
        v_.push(v);
        assert(Sto::try_commit());
    }
    size_t size() { return v_.size(); }
    void print_statistics() { this->v_.print_statistics(); }

private:
    FCQueue<T> v_;
};

template <typename T> struct DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, T>> : 
    public CDSQueueHarness<cds::container::BasketQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::FCQueue<T, std::queue<T>, FCQUEUE_TRAITS()>> : 
    public CDSQueueHarness<cds::container::FCQueue<T, std::queue<T>, FCQUEUE_TRAITS()>>{
        void print_statistics() { this->v_.print_statistics(); }
    };
#define FCQUEUE_TRAITS_ELIM() cds::container::fcqueue::make_traits< \
    cds::q_opt::lock_type<cds::sync::spin>, \
    cds::q_opt::back_off<cds::backoff::delay_of<2>>, \
    cds::q_opt::allocator<CDS_DEFAULT_ALLOCATOR>, \
    cds::q_opt::stat<cds::container::fcqueue::empty_stat>, \
    cds::q_opt::memory_model<cds::q_opt::v::relaxed_ordering>, \
    cds::q_opt::enable_elimination<true>>::type
template <typename T> struct DatatypeHarness<cds::container::FCQueue<T, std::queue<T>, FCQUEUE_TRAITS_ELIM()>>  :
    public CDSQueueHarness<cds::container::FCQueue<T, std::queue<T>, FCQUEUE_TRAITS_ELIM()>>{
        void print_statistics() { this->v_.print_statistics(); }
    };
template <typename T> struct DatatypeHarness<cds::container::MoirQueue<cds::gc::HP, T>> : 
    public CDSQueueHarness<cds::container::MoirQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::MSQueue<cds::gc::HP, T>> : 
    public CDSQueueHarness<cds::container::MSQueue<cds::gc::HP, T>> {};
template <typename T> struct DatatypeHarness<cds::container::q_optimisticQueue<cds::gc::HP, T>> : 
    public CDSQueueHarness<cds::container::q_optimisticQueue<cds::gc::HP, T>>{};
template <typename T> struct DatatypeHarness<cds::container::RWQueue<T>> : 
    public CDSQueueHarness<cds::container::RWQueue<T>>{};

template <typename T> struct DatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(32) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
    size_t size() { return v_.size(); }
private:
    cds::container::SegmentedQueue<cds::gc::HP, T> v_;
};
template <typename T> struct DatatypeHarness<cds::container::TsigasCycleQueue<T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(1000000) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
    size_t size() { return v_.size(); }
private:
    cds::container::TsigasCycleQueue<T> v_;
};
template <typename T> struct DatatypeHarness<cds::container::VyukovMPMCCycleQueue<T>> {
    typedef T value_type;
public:
    DatatypeHarness() : v_(1000000) {};
    bool pop() { int ret; return v_.pop(ret); }
    bool cleanup_pop() { return pop(); }
    void push(value_type v) { assert(v_.push(v)); }
    void init_push(value_type v) { assert(v_.push(v)); }
    size_t size() { return v_.size(); }
private:
    cds::container::VyukovMPMCCycleQueue<T> v_;
};

/*
 * Test interfaces and templates.
 */
template <typename DH> class QueueTest : public GenericTest {
public:
    QueueTest(int val_type) : val_type_(val_type){};
    void initialize(size_t init_sz) {
        global_push_val = MAX_VALUE;
        // initialize with the maximum values, so that
        // at the beginning, pushes and pops will not conflict
        for (unsigned i = 0; i < init_sz; ++i) {
            v_.init_push(global_push_val--);
        }
    }

    void cleanup() {
        while (v_.cleanup_pop()){/*keep popping*/}
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

template <typename DH> class SingleOpTest : public QueueTest<DH> {
public:
    SingleOpTest(int ds_type, int val_type, q_op q_op) : QueueTest<DH>(val_type), q_op_(q_op), ds_type_(ds_type) {};
    void run(int me) {
        Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        this->do_q_op(q_op_, rand_vals[(i*me + i) % arraysize(rand_vals)]);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(q_op_, me);
            } else {
                this->do_q_op(q_op_, rand_vals[(i*me + i)% arraysize(rand_vals)]);
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
        //Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        //std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                //Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        this->do_q_op(q_ops_array[me % arraysize(q_ops_array)], rand_vals[(i*me + i) % arraysize(rand_vals)]);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        //transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(q_ops_array[me % arraysize(q_ops_array)], me);
            } else {
                this->do_q_op(q_ops_array[me % arraysize(q_ops_array)], rand_vals[(i*me + i) % arraysize(rand_vals)]);
                this->inc_ctrs(q_ops_array[me % arraysize(q_ops_array)], me);
            }
        }
    }
private:
    int ds_type_;
};

template <typename DH> class RandomSingleOpTest : public QueueTest<DH> {
public:
    RandomSingleOpTest(int ds_type, int val_type) : QueueTest<DH>(val_type), ds_type_(ds_type) {};
    void run(int me) {
        q_op my_q_op;
        //Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        //std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                //Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        my_q_op = q_ops_array[q_ops_array[i % 2]];
                        //my_q_op = q_ops_array[slotdist(transgen) % arraysize(q_ops_array)];
                        this->do_q_op(my_q_op, rand_vals[(i*me + i) % arraysize(rand_vals)]);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        //transgen = transgen_snap;
                    }
                }
                this->inc_ctrs(my_q_op, me);
            } else {
                my_q_op = q_ops_array[q_ops_array[i%2]];
                //my_q_op = q_ops_array[slotdist(transgen) % arraysize(q_ops_array)];
                this->do_q_op(my_q_op, rand_vals[(i*me + i) % arraysize(rand_vals)]);
                this->inc_ctrs(my_q_op, me);
            }
        }
    }
private:
    int ds_type_;
};

template <typename DH> class GeneralTxnsTest : public QueueTest<DH> {
public:
    GeneralTxnsTest(int ds_type, int val_type, std::vector<std::vector<q_op>> txn_set) : 
        QueueTest<DH>(val_type), ds_type_(ds_type), txn_set_(txn_set) {};
    void run(int me) {
        //Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        //std::uniform_int_distribution<long> slotdist(0, MAX_VALUE);
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                //Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        auto txn = txn_set_[rand_txns[(i*me+i) % arraysize(rand_txns)] % txn_set_.size()];
                        for (unsigned j = 0; j < txn.size(); ++j) {
                            this->do_q_op(txn[j], rand_vals[(i*me + i) % arraysize(rand_vals)]);
                            this->inc_ctrs(txn[j], me); // XXX can lead to overcounting
                        }
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {
                        //transgen = transgen_snap;
                    }
                }
            } else {
                auto txn = txn_set_[rand_txns[(i*me+i) % arraysize(rand_txns)] % txn_set_.size()];
                //auto txn = txn_set_[slotdist(transgen) % txn_set_.size()];
                for (unsigned j = 0; j < txn.size(); ++j) {
                    this->do_q_op(txn[j], rand_vals[(i*me + i) % arraysize(rand_vals)]);
                    this->inc_ctrs(txn[j], me);
                }
            }
        }
    }
private:
    int ds_type_;
    std::vector<std::vector<q_op>> txn_set_;
};

#define MAKE_PQUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO pqueue", new test<DatatypeHarness<PriorityQueue<type>>>(STO, ## __VA_ARGS__)},      \
    {desc, "STO pqueue opaque", new test<DatatypeHarness<PriorityQueue<type, true>>>(STO, ## __VA_ARGS__)},\
    {desc, "STO FC pqueue", new test<DatatypeHarness<FCPriorityQueue<type>>>(STO, ## __VA_ARGS__)},\
    {desc, "FC pqueue", new test<DatatypeHarness<cds::container::FCPriorityQueue<type, std::priority_queue<type>, FCPQUEUE_TRAITS()>>>(CDS, ## __VA_ARGS__)}, \
    {desc, "FC pairing heap pqueue", new test<DatatypeHarness<cds::container::FCPriorityQueue<type, PairingHeap<type>>>>(CDS, ## __VA_ARGS__)} 
   // {desc, "MS pqueue", new test<DatatypeHarness<cds::container::MSPriorityQueue<type>>>(CDS, ## __VA_ARGS__)},

#define MAKE_QUEUE_TESTS(desc, test, type, ...) \
    {desc, "STO queue", new test<DatatypeHarness<Queue<type>>>(STO, ## __VA_ARGS__)},                                  \
    {desc, "STO queue2", new test<DatatypeHarness<Queue2<type>>>(STO, ## __VA_ARGS__)},                                  \
    {desc, "FC queue", new test<DatatypeHarness<cds::container::FCQueue<type, std::queue<type>, FCQUEUE_TRAITS()>>>(CDS, ## __VA_ARGS__)},                 \
    {desc, "STO/FC queue", new test<DatatypeHarness<FCQueue<type>>>(STO, ## __VA_ARGS__)}
    //{desc, "Basket queue", new test<DatatypeHarness<cds::container::BasketQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},         \
    //{desc, "Moir queue", new test<DatatypeHarness<cds::container::MoirQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)}, \
    //{desc, "MS queue", new test<DatatypeHarness<cds::container::MSQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},   \
    //{desc, "Opt queue", new test<DatatypeHarness<cds::container::OptimisticQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)},   \
    //{desc, "RW queue", new test<DatatypeHarness<cds::container::RWQueue<type>>>(CDS, ## __VA_ARGS__)}, \
    //{desc, "Segmented queue", new test<DatatypeHarness<cds::container::SegmentedQueue<cds::gc::HP, type>>>(CDS, ## __VA_ARGS__)}, \
    //{desc, "TC queue", new test<DatatypeHarness<cds::container::TsigasCycleQueue<type>>>(CDS, ## __VA_ARGS__)}, \
    //{desc, "VyukovMPMC queue", new test<DatatypeHarness<cds::container::VyukovMPMCCycleQueue<type>>>(CDS, ## __VA_ARGS__)}

