// Contains core data structures for MVCC

#pragma once

#include <deque>
#include <stack>
#include <thread>

#include "MVCCTypes.hh"
#include "Transaction.hh"
#include "TRcu.hh"

#ifndef VERBOSE
#define VERBOSE 0
#endif
#define MVCC_GARBAGE_DEBUG (!NDEBUG && (VERBOSE > 1))

// Status types of MvHistory elements
enum MvStatus {
    // Single-bit states first, sorted by bit index
    UNUSED                         = 0b00'0000'0000'0000,
    DELETED                        = 0b00'0000'0000'0001,  // Not a valid state on its own, but defined as a flag
    PENDING                        = 0b00'0000'0000'0010,
    COMMITTED                      = 0b00'0000'0000'0100,
    DELTA                          = 0b00'0000'0000'1000,  // Commutative update delta
    LOCKED                         = 0b00'0000'0001'0000,
    ABORTED                        = 0b00'0000'0010'0000,
    GARBAGE                        = 0b00'0000'0100'0000,
    GARBAGE2                       = 0b00'0000'1000'0000,
    POISONED                       = 0b00'1000'0000'0000,  // Standalone poison bit
    RESPLIT                        = 0b01'0000'0000'0000,  // This version signals a resplit
    ENQUEUED                       = 0b10'0000'0000'0000,  // This version leads a GC enqueue

    // Multi-bit states
    ABORTED_RV                     = 0b00'0001'0010'0000,
    ABORTED_WV1                    = 0b00'0010'0010'0000,
    ABORTED_WV2                    = 0b00'0011'0010'0000,
    ABORTED_TXNV                   = 0b00'0100'0010'0000,
    ABORTED_POISON                 = 0b00'0101'0010'0000,
    PENDING_DELTA                  = 0b00'0000'0000'1010,
    COMMITTED_DELTA                = 0b00'0000'0000'1100,
    PENDING_DELETED                = 0b00'0000'0000'0011,
    COMMITTED_DELETED              = 0b00'0000'0000'0101,
    PENDING_RESPLIT                = 0b01'0000'0000'0010,
    COMMITTED_RESPLIT              = 0b01'0000'0000'0100,
    RESPLIT_DELTA                  = 0b01'0000'0000'1000,
    LOCKED_DELTA                   = 0b00'0000'0001'1000,
    LOCKED_COMMITTED_DELTA         = 0b00'0000'0001'1100,  // Converting from delta to flattened
    PENDING_RESPLIT_DELTA          = 0b01'0000'0000'1010,
    COMMITTED_RESPLIT_DELTA        = 0b01'0000'0000'1100,
    LOCKED_PENDING_RESPLIT_DELTA   = 0b01'0000'0001'1010,
    LOCKED_COMMITTED_RESPLIT_DELTA = 0b01'0000'0001'1100,
};

std::ostream& operator<<(std::ostream& w, MvStatus s);
namespace mvstatus {
    std::string stringof(MvStatus s);
}

class MvHistoryBase {
public:
    using tid_type = TransactionTid::type;

    MvHistoryBase() = delete;
    MvHistoryBase(void* obj, tid_type tid, MvStatus status)
        : status_(status), wtid_(tid), rtid_(tid), obj_(obj), split_(0), cells_(0) {
#if VERBOSE > 0
        Transaction::fprint("Creating history ", this, " at TID ", tid, "\n");
#endif
    }

    std::atomic<MvStatus> status_;  // Status of this element
    tid_type wtid_;  // Write TID
    std::atomic<tid_type> rtid_;  // Read TID
    void* obj_;  // Parent object
    std::atomic<int> split_;  // Split policy index
    std::atomic<ssize_t> cells_;  // Number of cells this is participating in
};

template <typename T, size_t Cells>
class MvHistory : public MvHistoryBase {
public:
    typedef TransactionTid::type tid_type;
    typedef TRcuSet::epoch_type epoch_type;
    typedef MvHistoryBase base_type;
    typedef MvHistory<T, Cells> history_type;
    typedef MvObject<T, Cells> object_type;
    typedef commutators::Commutator<T> comm_type;

    MvHistory() = delete;
    explicit MvHistory(object_type *obj)
        : MvHistory(obj, 0, nullptr) {
    }
    explicit MvHistory(
            object_type *obj, tid_type ntid, const T& nv)
        : base_type(obj, ntid, PENDING),
          prev_(), v_(nv) {
    }
    explicit MvHistory(
            object_type *obj, tid_type ntid, T&& nv)
        : base_type(obj, ntid, PENDING),
          prev_(), v_(std::move(nv)) {
    }
    explicit MvHistory(
            object_type *obj, tid_type ntid, T *nvp)
        : base_type(obj, ntid, PENDING),
          prev_(), v_(nvp ? *nvp : v_ /*XXXXXXX*/) {
    }
    explicit MvHistory(
            object_type *obj, tid_type ntid, comm_type &&c)
        : base_type(obj, ntid, PENDING_DELTA),
          prev_(), c_(std::move(c)), v_() {
    }

private:
    template <typename TT>
    using Scaled = std::array<TT, Cells>;
    template <class Function, class Iterable, typename... Args>
    static constexpr void IterateOver(Iterable& container, Function&& func, Args&... args) {
        IterateOver<0, Function, Iterable, Args...>(container, func, std::forward<Args*>(&args)...);
    }
    template <size_t Cell, class Function, class Iterable, typename... Args>
    static constexpr void IterateOver(Iterable& container, Function& func, Args*... args) {
        static_assert(Cell < Cells);
        //printf("Iterating over cell %zu\n", Cell);
        func(Cell, std::get<Cell>(container), std::get<Cell>(*args)...);
        if constexpr (Cell + 1 < Cells) {
            IterateOver<Cell + 1, Function, Iterable, Args...>(
                container, func, std::forward<Args*>(args)...);
        }
    }

public:
    // Initiate a multi-cell flattening procedure
    static void adaptive_flatten(tid_type, Scaled<history_type*> histories, const bool /*fg*/=true) {
        using RecordAccessor = typename T::RecordAccessor;
        // Current element is the one initiating the flattening here. It is not
        // included in the trace, but it is included in the committed trace.
        Scaled<std::stack<history_type*>> traces;  // Of history elements to process
        Scaled<tid_type> safe_wtids {};
        T value {};
        ssize_t traces_left = 0;

        IterateOver(histories, [&](const size_t cell, auto& base, auto& trace, auto& safe) {
            if (!base) {
                return;
            }
            auto status = base->status_.load(std::memory_order_relaxed);
            base->assert_status(!(status & ABORTED), "Adaptive flatten cannot happen on aborted versions.");
            if ((status & COMMITTED_DELTA) == COMMITTED_DELTA) {
                if (status & LOCKED) {
                    while (!base->status_is(COMMITTED_DELTA, COMMITTED));
                }
            }
            history_type* curr = base;
            while (!curr->status_is(COMMITTED_DELTA, COMMITTED)) {
                trace.push(curr);
                auto prev = curr->prev(cell);
                //while (!prev) wait_cycles(1000000);
                curr = prev;
            }
            TXP_INCREMENT(txp_mvcc_flat_versions);
            RecordAccessor::copy_cell(curr->split(), cell, &value, &curr->v_);
            safe = curr->wtid();
            curr->update_rtid(base->wtid());
            if (!trace.empty()) {
                ++traces_left;
            }
        }, traces, safe_wtids);

        while (traces_left) {
            history_type* hnext = nullptr;
            IterateOver(traces, [&](const size_t cell, auto& trace, auto& safe) {
                if (trace.empty()) {
                    return;
                }
                auto h = trace.top();
                while (true) {
                    auto prev = h->prev(cell);
                    if (prev->wtid() <= safe) {
                        break;
                    }
                    trace.push(prev);
                    h = prev;
                }
                if (!hnext || h->wtid() < hnext->wtid()) {
                    hnext = h;
                }
            }, safe_wtids);

            bool applied = false;
            IterateOver(traces, [&](const size_t cell, auto& trace, auto& safe) {
                if (trace.empty()) {
                    return;
                }
                if (trace.top() != hnext) {
                    return;
                }
                auto status = hnext->status();
                if (hnext != histories[cell]) {
                    hnext->wait_if_pending(status);
                }
                if (status & COMMITTED) {
                    hnext->update_rtid(histories[cell]->wtid());
                    assert(!(status & DELETED));
                    if (status & DELTA) {
                        if (!applied) {
                            hnext->c_.operate(value);  // XXX: how do we order this part properly?
                            applied = true;
                        }
                    } else {
                        RecordAccessor::copy_cell(hnext->split(), cell, &value, &hnext->v_);
                    }
                }

                trace.pop();
                if (trace.empty()) {
                    --traces_left;
                }
                safe = hnext->wtid();
            }, safe_wtids);
        }

        IterateOver(histories, [&](const size_t, auto h) {
            if (!h) {
                return;
            }
            auto status = h->status();
            /*
            auto expected = static_cast<MvStatus>((status & RESPLIT) ?
                (RESPLIT_DELTA | ((status & PENDING) ? PENDING : COMMITTED)) :
                COMMITTED_DELTA);
            */
            auto expected = static_cast<MvStatus>((status & RESPLIT) | COMMITTED_DELTA);
            auto desired = static_cast<MvStatus>(LOCKED | expected);
            //printf("%d %p Adaptively flattening %p with status %x expecting %x\n", TThread::id(), h->object(), h, h->status(), expected);
            if (h->status_.compare_exchange_strong(expected, desired)) {
#if VERBOSE > 0
                Transaction::fprint(
                        h, " is being flattened with status ", expected,
                        " to status ", (status & ~LOCKED_DELTA), "\n");
#endif
#if !NDEBUG
                h->metadata.flatten_thread = TThread::id();
#endif
                h->v_ = value;
                TXP_INCREMENT(txp_mvcc_flat_commits);
                h->status(status & ~LOCKED_DELTA);
                if (h->object()->is_inlined(h)) {
                    //
                    //printf("%p flatten enq %p tid %zu\n", object(), this, wtid());
                }
                assert((status & PENDING_RESPLIT) != PENDING_RESPLIT);
                /*
                if (fg) {
                    h->enqueue_for_committed(__FILE__, __LINE__);
                    h->object()->flattenv_.store(0, std::memory_order_relaxed);
                }
                */
            } else {
                //printf("%d %p Flattening lock failed %p with status %x\n", TThread::id(), h->object(), h, expected);
                TXP_INCREMENT(txp_mvcc_flat_spins);
                while ( !h->status_is(COMMITTED_DELTA, COMMITTED)/* &&
                        !h->status_is(RESPLIT_DELTA, RESPLIT)*/);
            }
        });
    }

    void adaptive_flatten_pending(tid_type tid) {
        assert(status_is(PENDING));
        std::array<history_type*, Cells> histories;
        std::fill(histories.begin(), histories.end(), nullptr);
        for (size_t cell = 0; cell < Cells; ++cell) {
            history_type* h = prev(cell);
            if (h) {
                histories[cell] = object()->find_from(h, tid, cell);
            }
        }
        history_type::adaptive_flatten(tid, histories);
        for (size_t cell = 0; cell < Cells; ++cell) {
            if (histories[cell]) {
                assert(histories[cell] == object()->find_from(this->prev(cell), tid, cell));
                T::RecordAccessor::copy_cell(histories[cell]->split(), cell, &v_, &histories[cell]->v_);
            }
        }
        c_.operate(v_);
#if VERBOSE > 0
        Transaction::fprint(
            "This is ", this, ", with status ", status(), " and histories ",
            histories[0], " and ", histories[1], "; prevs ", prev(0), " and ", prev(1), "\n");
#endif
        status(status() & ~DELTA);
    }

#if NDEBUG
    void assert_status(bool, const char*) {
    }
#else
    void assert_status(bool ok, const char* description) {
        if (!ok) {
            assert_status_fail(description);
        }
    }
    void assert_status_fail(const char* description) {
        std::cerr << "MvHistoryBase::assert_status_fail: " << description << "\n";
        for (size_t cell = 0; cell < Cells; ++cell) {
            print_prevs(5, cell);
        }
        assert(false);
    }
#endif

    // Whether this version can be late-inserted in front of hnext
    bool can_precede(const history_type* hnext) const {
        return (!this->status_is(DELETED) || !hnext->status_is(DELTA)) &&
            (!this->status_is(RESPLIT) || hnext->status_is(PENDING));
    }

    bool can_precede_anything() const {
        return !this->status_is(DELETED) && !this->status_is(RESPLIT);
    }

    // Enqueues the deleted version for future cleanup
#if VERBOSE > 0
    inline void enqueue_for_committed(const char* file, const int line)
#else
    inline void enqueue_for_committed(const char*, const int)
#endif
    {
        //printf("%p committed enq %p tid %zu %s:%d\n", object(), this, wtid(), file, line);
#if VERBOSE > 0
        Transaction::fprint(
                "object ", object(), " committed enq ", this, " tid ", wtid(),
                " ", file, ":", line, "\n");
#endif
        MvStatus s;
        do {
            s = status();
            assert_status((s & COMMITTED_DELTA) == COMMITTED, "enqueue_for_committed");
        } while (!status_.compare_exchange_weak(s, static_cast<MvStatus>(s | ENQUEUED)));
        Transaction::rcu_call(gc_committed_cb, this);
    }

    // Returns the effective split, recursing if necessary
    inline int get_split() {
        int split = this->split();
        assert(split >= 0);
        return split;
    }

    // Retrieve the object for which this history element is intended
    inline object_type* object() const {
        return static_cast<object_type*>(obj_);
    }

    template <size_t Cell=0>
    inline history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_[Cell].load());
    }

    inline history_type* prev(const size_t cell=0) const {
        return reinterpret_cast<history_type*>(prev_[cell].load());
    }

    void print_prevs(size_t max=1000, size_t cell=0, std::FILE* stream=stderr) const {
        fprintf(stream, "%s", list_prevs(max, cell).c_str());
    }

    std::string list_prevs(size_t max, size_t cell) const {
        std::ostringstream stream;
        stream << "Thread " << TThread::id() << ", TX" << Sto::read_tid()
               << "; obj " << object() << std::endl;
        auto h = this;
        for (size_t i = 0;
             h && i < max;
             ++i, h = h->prev_relaxed(cell)) {
            stream << cell << "." << i << ". "
                   << h << (h->object()->is_inlined(h) ? " INLINE " : " ")
                   << mvstatus::stringof(h->status_.load(std::memory_order_relaxed))
                   << " W" << h->wtid_
                   << " R" << h->rtid_.load(std::memory_order_relaxed)
                   << " SP" << h->split_.load(std::memory_order_relaxed)
                   << std::endl;
            /*
            if (h->status_is(COMMITTED)) {
                break;
            }
            */
            /*
            fprintf(stream, "%zu.%zu. %p%s %s W%zu R%zu SP%d\n", cell, i, (void*)h,
                    h->object()->is_inlined(h) ? " INLINE" : "",
                    mvstatus::stringof(h->status_.load(std::memory_order_relaxed)).c_str(),
                    h->wtid_, h->rtid_.load(std::memory_order_relaxed),
                    h->split_.load(std::memory_order_relaxed));
            */
        }
        return stream.str();
    }

    // Returns the current rtid
    inline tid_type rtid() const {
        return rtid_;
    }

private:
    template <size_t Cell=0>
    inline history_type* prev_relaxed() const {
        return reinterpret_cast<history_type*>(prev_[Cell].load(std::memory_order_relaxed));
    }

    inline history_type* prev_relaxed(size_t cell=0) const {
        return reinterpret_cast<history_type*>(prev_[cell].load(std::memory_order_relaxed));
    }

    inline void update_rtid(tid_type minimum_new_rtid) {
        tid_type prev = rtid_.load(std::memory_order_relaxed);
        while (prev < minimum_new_rtid
               && !rtid_.compare_exchange_weak(prev, minimum_new_rtid,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
        }
    }

public:
    // Set the corresponding value slot
    inline void set_raw_value(comm_type& value) {
        c_ = value;
    }

    // Set the corresponding value slot
    inline void set_raw_value(comm_type&& value) {
        c_ = value;
    }

    // Set the corresponding value slot
    inline void set_raw_value(T& value) {
        v_ = value;
    }

    // Set the corresponding value slot
    inline void set_raw_value(T&& value) {
        v_ = value;
    }

    // Set the corresponding value slot
    inline void set_raw_value(T* value) {
        if (value) {
            v_ = *value;
        }
    }

    // Get the split index
    inline int split() const {
        return split_.load(std::memory_order::memory_order_relaxed);
    }

    // Set the split index
    inline void split(int split) {
        split_.store(split, std::memory_order::memory_order_relaxed);
    }

    // Returns the status
    inline MvStatus status() const {
        return status_.load();
    }

    // Sets and returns the status
    // NOT THREADSAFE
    inline void status(MvStatus s) {
        status_.store(s, std::memory_order_release);
    }
    inline void status(unsigned long long s) {
        status((MvStatus)s);
    }

    // Removes all flags, signaling an aborted status
    inline void status_abort(MvStatus reason = ABORTED) {
        int s = status();
        assert_status((s & (PENDING|COMMITTED)) == PENDING, "status_abort");
        assert(reason & ABORTED);
        status(reason);
    }
    inline void status_txn_abort() {
        int s = status();
        if (s != 0x2) {
            //printf("%p aborting %p status %x\n", object(), this, s);
        }
        if (!(s & ABORTED)) {
            assert_status((s & (PENDING|COMMITTED)) == PENDING, "status_txn_abort");
            status(ABORTED_TXNV);
        }
    }

    // Sets the committed flag and unsets the pending flag; does nothing if the
    // current status is ABORTED
    inline void status_commit() {
        int s = status();
        assert_status(!(s & ABORTED), "status_commit");
        status(COMMITTED | (s & ~PENDING));
    }

    // Sets the deleted flag; does nothing if the current status is ABORTED
    // NOT THREADSAFE
    inline void status_delete() {
        int s = status();
        assert(!(s & ABORTED));
        status(DELETED | s);
    }

    // Sets the poisoned flag
    // NOT THREADSAFE
    inline void status_poisoned() {
        int s = status();
        assert((s & COMMITTED_DELETED) == COMMITTED_DELETED);
        status(POISONED | s);
    }

    // Unsets the poisoned flag
    // NOT THREADSAFE
    inline void status_unpoisoned() {
        int s = status();
        assert(s & POISONED);
        status(~POISONED & s);
    }

    // Returns true if all the given flag(s) are set
    inline bool status_is(const MvStatus status) const {
        return status_is(status, status);
    }

    // Returns true if the status with the given mask equals the expected
    inline bool status_is(const MvStatus mask, const MvStatus expected) const {
        assert(mask != 0);
        return (status_ & mask) == expected;
    }

    // Spin-wait on interested item
    void wait_if_pending(MvStatus &s) const {
        while (s & PENDING) {
            // TODO: implement a backoff or something
            relax_fence();
            s = status();
        }
    }

    inline auto& c() {
        return c_;
    }

    inline T& v() {
        if (status_is(DELTA)) {
            enflatten();
        }
        return v_;
    }

    inline T* vp() {
        if (status_is(DELTA)) {
            //printf("%d %p Calling flatten to resolve value %p\n", TThread::id(), object(), this);
            enflatten();
        }
        return &v_;
    }

    // Returns the current wtid
    inline tid_type wtid() const {
        return wtid_;
    }

private:
    bool check_consistency(size_t depth, size_t cell) {
        for (history_type* h = this; depth > 0; --depth) {
            if (!h) {
                break;
            }
            auto prev = h->prev(cell);
            if (!prev) {
                break;
            }
            /*
            if (h->status_is(COMMITTED)) {
                break;
            }
            */
            if (h->wtid_ <= prev->wtid_) {
                return false;
            }
            h = prev;
        }
        return true;
    }

    static void gc_committed_cb(void* ptr) {
        history_type* hptr = static_cast<history_type*>(ptr);
#if VERBOSE > 0
        Transaction::fprint("gc_committed_cb called on ", hptr, "\n");
#endif
        //printf("%p gc cb on %p\n", hptr->object(), hptr);
        auto s = hptr->status();
        if ((s & COMMITTED_DELTA) != COMMITTED) {
            //printf("%p status %p: %d\n", hptr->object(), hptr, s);
        }
        hptr->assert_status((s & COMMITTED_DELTA) == COMMITTED, "gc_committed_cb");
        hptr->assert_status((s & ENQUEUED) == ENQUEUED, "gc_committed_cb enq flag");
        // Here is how we ensure that `gc_committed_cb` never conflicts
        // with a flatten operation.
        // (1) When `gc_committed_cb` runs, `h->prev()`
        // is definitively in the past. No active flattens
        // will overlap with it.
        // (2) `gc_flatten_cb`, the background flattener, stops at
        // the first committed version it sees, which will always be
        // in the present (not the past---all versions touched by
        // `gc_committed_cb` are in the past).
        // EXCEPTION: Some nontransactional accesses (`v()`, `nontrans_*`)
        // ignore this protocol.
        //history_type* newer[Cells];
#if !NDEBUG
        bool active[Cells];
#endif
        history_type* older[Cells];
        //auto obj = hptr->object();
        auto count = 0;
        for (size_t cell = 0; cell < Cells; ++cell) {
            //newer[cell] = hptr;
            older[cell] = hptr->prev_relaxed(cell);
            if (older[cell]) {
                MvHistoryBase* expected = older[cell];
                /*
                while (!hptr->prev_[cell].compare_exchange_weak(expected, nullptr)) {
                    if (!expected) {
                        break;
                    }
                    expected = older[cell];
                }
                assert(!expected || expected == older[cell]);
                */

                if (expected) {
#if VERBOSE > 0
                    auto before = older[cell]->cells_.fetch_add(-1, std::memory_order_relaxed);
                    Transaction::fprint(
                            "Precleared pointer from ", hptr, " to ", older[cell],
                            " on cell ", cell, ", ", before - 1, " remaining \n");
#else
                    older[cell]->cells_.fetch_add(-1, std::memory_order_relaxed);
#endif
                    ++count;
                } else {
                    older[cell] = nullptr;
                }
            }
#if !NDEBUG
            active[cell] = !!older[cell];
            (void)active[cell];
#endif
        }
        while (count > 0) {
            auto h = older[0];
            //auto cell = 0;
            for (size_t ncell = 1; ncell < Cells; ++ncell) {
                if (!h) {
                    h = older[ncell];
                } else if (older[ncell]) {
                    if (older[ncell]->wtid() > h->wtid()) {
                        h = older[ncell];
                    }
                }
                /*
                if (!older[cell]) {
                    cell = ncell;
                } else if (older[ncell]) {
                    if (older[ncell]->wtid() > older[cell]->wtid()) {
                        cell = ncell;
                    }
                }
                */
            }
            //auto h = older[cell];
            assert(h);
            auto status = h->status();
            //if ((status & COMMITTED_DELTA) == COMMITTED) {
            if ((status & ENQUEUED) == ENQUEUED) {
#if VERBOSE > 0
                Transaction::fprint(
                        "Ending GC loop because ", h, " has status ", status, "\n");
#endif
                /*
                for (size_t cell = 0; cell < Cells; ++cell) {
                    if (h->prev_relaxed(cell)) {
                        h->prev_[cell].store(nullptr, std::memory_order_relaxed);
                    }
                }
                */
                break;
            }
            //bool skip = true;
            ssize_t iterations = 0;
            for (size_t ncell = 0; ncell < Cells; ++ncell) {
                if (older[ncell] == h) {

                    /*
                    MvHistoryBase* expected = h;
                    while (!newer[ncell]->prev_[ncell].compare_exchange_weak(expected, nullptr)) {
                        expected = h;
                    }
                    assert(expected == h);
                    Transaction::fprint(
                            "Cleared pointer from ", newer[ncell], " to ", h,
                            " on cell ", ncell, "\n");
                    */

                    //newer[ncell] = h;
                    older[ncell] = h->prev_relaxed(ncell);
                    if (!older[ncell]) {  // Assumes that there are no dangling pointers
                        --count;
                    } else {
                        MvHistoryBase* expected = older[ncell];
                        /*
                        while (!h->prev_[ncell].compare_exchange_weak(expected, nullptr)) {
                            if (!expected) {
                                break;
                            }
                            expected = older[ncell];
                        }
                        */
                        assert(!expected || expected == older[ncell]);
                        if (expected) {
                            /*
                            size_t kcount = 0;
                            for (size_t kcell = 0; kcell < Cells; ++kcell) {
                                if (h->prev_relaxed(kcell)) {
                                    ++kcount;
                                }
                            }
                            Transaction::fprint(
                                    "Cleared pointer from ", h, " to ", older[ncell],
                                    " on cell ", ncell, ", ", kcount, " remaining \n");
                            */
#if VERBOSE > 0
                            auto before = older[ncell]->cells_.fetch_add(-1, std::memory_order_relaxed);
                            Transaction::fprint(
                                    "Cleared pointer from ", h, " to ", older[ncell],
                                    " on cell ", ncell, ", ", before - 1, " remaining \n");
#else
                            older[ncell]->cells_.fetch_add(-1, std::memory_order_relaxed);
#endif
                        } else {
                            older[ncell] = nullptr;
                            --count;
                        }
                    }
                    ++iterations;
                    /*
                    ssize_t c;
                    if ((c = h->cells_.fetch_add(-1, std::memory_order::memory_order_relaxed)) == 1) {
                        skip = false;
                    }
                    */
                    //printf("%p decr %p on cell %zu to %zx\n", obj, h, ncell, c - 1);
                } else {
                    assert(!older[ncell] || older[ncell]->wtid() < h->wtid());
                }
            }
            status = h->status();
            //assert(h->cells_.load(std::memory_order::memory_order_relaxed) >= iterations);
            //if (skip) {
                // Skipping versions that are still in use by another chain,
                // should be cleaned up by gc there
                //printf("%p skipping %p with %zx references left\n", obj, h, h->cells_.load());
            //} else {
                //printf("%p enqueuing delete %p\n", obj, h);
            //if (h->cells_.fetch_add(-iterations, std::memory_order_relaxed) == iterations) {
            ssize_t gc_cells = 0;
            /*
            if ((status & ENQUEUED) != ENQUEUED && h->status_.compare_exchange_strong(
                        status, static_cast<MvStatus>(status | ENQUEUED))) {
                assert((status & COMMITTED_DELTA) == COMMITTED);
                h->enqueue_for_committed(__FILE__, __LINE__);
            */
            if (h->cells_.compare_exchange_strong(gc_cells, -1, std::memory_order_relaxed)) {
#if MVCC_GARBAGE_DEBUG
                h->assert_status(!(status & (GARBAGE | GARBAGE2)), "gc_committed_cb garbage tracking");
                while (!(status & GARBAGE)
                       && h->status_.compare_exchange_weak(status, MvStatus(status | GARBAGE))) {
                }
#endif
#if VERBOSE > 0
                Transaction::fprint(
                        "Enqueuing gc for ", h, " with prevs ", h->prev(0), " and ", h->prev(1),
                        " after ", iterations, " iterations at ",
                        h->cells_.load(std::memory_order_relaxed), " cells\n");
                for (size_t cell = 0; cell < Cells; ++cell) {
                    if (h->prev_relaxed(cell)) {
                        Transaction::fprint(
                                h, " has non-null prev in cell ", cell, ": ",
                                h->prev_relaxed(cell),
                                active[cell] ? " (active)" : " (inactive)", "\n");
                    }
                }
#endif
                Transaction::rcu_call(gc_deleted_cb, h);
            }
            h->assert_status(!(status & (LOCKED | PENDING)), "gc_committed_cb unlocked not pending");
        }
    }

    static void gc_deleted_cb(void* ptr) {
        history_type* h = static_cast<history_type*>(ptr);
#if MVCC_GARBAGE_DEBUG
        MvStatus status = h->status_.load(std::memory_order_relaxed);
        h->assert_status((status & GARBAGE), "gc_deleted_cb garbage");
        h->assert_status(!(status & GARBAGE2), "gc_deleted_cb garbage");
        bool ok = h->status_.compare_exchange_strong(status, MvStatus(status | GARBAGE2));
        assert(ok);
#endif
#if VERBOSE > 0
        Transaction::fprint(
                "Thread ", TThread::id(), " intending to delete from ", h->object(),
                " element: ", h, " status ", h->status(), "\n");
#endif
        h->object()->delete_history(h);
    }

    // Initializes the flattening process
    inline void enflatten() {
        if constexpr (Cells > 1 && commutators::CommAdapter::Properties<T>::has_record_accessor) {
            //printf("Using adaptive flatten subroutine.\n");
            adaptive_flatten(Sto::read_tid(), {this});
            return;
        }
        int s = status_.load(std::memory_order_relaxed);
        assert_status(!(s & ABORTED), "enflatten not aborted");
        if ((s & COMMITTED_DELTA) == COMMITTED_DELTA) {
            if (!(s & LOCKED)) {
                flatten(s, true);
            } else {
                TXP_INCREMENT(txp_mvcc_flat_spins);
            }
            while (!status_is(COMMITTED_DELTA, COMMITTED)) {
            }
        }
    }

    // To be called from the source of the flattening.
    void flatten(int old_status, bool fg) {
        assert(old_status == COMMITTED_DELTA);
#if NDEBUG
        (void) old_status;
#endif

        // Current element is the one initiating the flattening here. It is not
        // included in the trace, but it is included in the committed trace.
        history_type* curr = this;
        std::stack<history_type*> trace;  // Of history elements to process

        //printf("%d %p Flattening %p with status %x\n", TThread::id(), object(), this, old_status);

        while (!curr->status_is(COMMITTED_DELTA, COMMITTED)) {
            trace.push(curr);
            curr = curr->prev();
            if (!curr) {
                //printf("%d %p Flattening error %p with status %x\n", TThread::id(), object(), curr, old_status);
            }
        }

        TXP_INCREMENT(txp_mvcc_flat_versions);
        T value {curr->v_};
        tid_type safe_wtid = curr->wtid();
        curr->update_rtid(this->wtid());

        while (!trace.empty()) {
            auto hnext = trace.top();

            while (true) {
                auto prev = hnext->prev();
                if (prev->wtid() <= safe_wtid) {
                    break;
                }
                trace.push(prev);
                hnext = prev;
            }

            auto status = hnext->status();
            hnext->wait_if_pending(status);
            if (status & COMMITTED) {
                hnext->update_rtid(this->wtid());
                assert(!(status & DELETED));
                if (status & DELTA) {
                    hnext->c_.operate(value);
                } else {
                    value = hnext->v_;
                }
            }

            trace.pop();
            safe_wtid = hnext->wtid();
        }

        auto expected = COMMITTED_DELTA;
        if (status_.compare_exchange_strong(expected, LOCKED_COMMITTED_DELTA)) {
#if !NDEBUG
            metadata.flatten_thread = TThread::id();
#endif
            v_ = value;
            TXP_INCREMENT(txp_mvcc_flat_commits);
            status(COMMITTED);
            if (object()->is_inlined(this)) {
                //
                //printf("%p flatten enq %p tid %zu\n", object(), this, wtid());
            }
            enqueue_for_committed(__FILE__, __LINE__);
            if (fg) {
                object()->flattenv_.store(0, std::memory_order_relaxed);
            }
        } else {
            TXP_INCREMENT(txp_mvcc_flat_spins);
        }
    }

    std::array<std::atomic<MvHistoryBase*>, Cells> prev_;
    comm_type c_;
    T v_;

#if !NDEBUG
    struct metadata_t_ {
        int creator_thread = TThread::id();
        int flatten_thread = -1;
    };
    metadata_t_ metadata;
#endif

    friend class MvObject<T, Cells>;
};

template <typename T, size_t Cells>
class MvObject {
public:
    typedef MvHistory<T, Cells> history_type;
    typedef typename history_type::base_type base_type;
    typedef const T& read_type;
    typedef TransactionTid::type tid_type;

    // How many consecutive DELTA versions will be allowed before flattening
    static constexpr int gc_flattening_length = 257;

    MvObject()
#if MVCC_INLINING
        : ih_(this)
#endif
    {
#if MVCC_INLINING
        history_type* hnew = &ih_;
#else
        auto* hnew = new_history();
#endif
        if (std::is_trivial<T>::value) {
            hnew->v_ = T(); /* XXXX */
        }
        hnew->status(COMMITTED_DELETED);
        setup(hnew);
    }
    explicit MvObject(const T& value)
#if MVCC_INLINING
        : ih_(this, 0, value)
#endif
        {
#if MVCC_INLINING
        history_type* hnew = &ih_;
#else
        auto* hnew = new_history(0, value);
#endif
        hnew->status(COMMITTED);
        setup(hnew);
    }
    explicit MvObject(T&& value)
#if MVCC_INLINING
        : ih_(this, 0, std::move(value))
#endif
        {
#if MVCC_INLINING
        history_type* hnew = &ih_;
#else
        auto* hnew = new_history(0, std::move(value));
#endif
        hnew->status(COMMITTED);
        setup(hnew);
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
#if MVCC_INLINING
        : ih_(this, 0, T(std::forward<Args>(args)...))
#endif
        {
#if MVCC_INLINING
        history_type* hnew = &ih_;
#else
        auto* hnew = new_history(0, T(std::forward<Args>(args)...));
#endif
        hnew->status(COMMITTED);
        setup(hnew);
    }

    /*
    ~MvObject() {
        history_type* h = h_;
        while (h && !h->is_gc_enqueued()) {
            history_type* prev = h->prev();
            h->gc_push(is_inlined(h));
            h = prev;
        }
    }
    */

    class InvalidState {};

    const T& access(const tid_type tid) const {
        return find(tid)->v();
    }
    T& access(const tid_type tid) {
        return find(tid)->v();
    }

    void auxdata(void* ptr) {
        auxdata_ = ptr;
    }
    template <typename Type=void>
    Type* auxdata() const {
        return reinterpret_cast<Type*>(auxdata_);
    }

    // "Lock" step: pending version installation & version consistency check;
    //              returns true if successful, false if aborted
    template <bool may_commute = true>
    bool cp_lock(const tid_type tid, history_type* hw, size_t cell=0) {
        // Can only install pending versions
        hw->assert_status(hw->status_is(PENDING), "cp_lock pending");

        //printf("%p cp_lock %p cell %zu\n", this, hw, cell);

        std::atomic<base_type*>* target = &h_[cell];
        hw->cells_.fetch_add(1, std::memory_order_relaxed);
        while (true) {
            // Discover target atomic on which to do CAS
            base_type* t = *target;
            history_type* ht = reinterpret_cast<history_type*>(t);
            if (t->wtid_ > tid) {
                if (may_commute && !hw->can_precede(ht)) {
                    Sto::transaction()->mark_abort_because(
                            nullptr, "MVCC Lock: Invalid DU reordering.",
                            0, __FILE__, __LINE__, __FUNCTION__);
                    hw->cells_.fetch_add(-1, std::memory_order_relaxed);
                    return false;
                }
                target = &ht->prev_[cell];
            } else if (!ht->status_is(ABORTED) && ht->rtid() > tid) {
                Sto::transaction()->mark_abort_because(
                        nullptr, "MVCC Lock: Predecessor RTID higher than this WTID.",
                        0, __FILE__, __LINE__, __FUNCTION__);
                hw->cells_.fetch_add(-1, std::memory_order_relaxed);
                return false;
            } else {
                // Properly link h's prev_
                assert(hw != t);
#if VERBOSE > 0
                if (!ht->check_consistency(10, cell)) {
                    Transaction::fprint("ht prevs:\n", ht->list_prevs(10, cell));
                    while (true) wait_cycles(1000000);
                }
#endif
                hw->prev_[cell].store(t, std::memory_order_release);
#if VERBOSE > 0
                if (!hw->check_consistency(10, cell)) {
                    Transaction::fprint("hw prevs:\n", hw->list_prevs(10, cell));
                    while (true) wait_cycles(1000000);
                }
#endif

                // Attempt to CAS onto the target
                if (target->compare_exchange_strong(t, hw)) {
#if VERBOSE > 0
                    if (!hw->check_consistency(10, cell)) {
                        Transaction::fprint("hw prevs:\n", hw->list_prevs(10, cell));
                        while (true) wait_cycles(1000000);
                    }
#endif
                    break;
                }

                // Unset the prev
                hw->prev_[cell].store(nullptr, std::memory_order_release);
            }
        }

        // Write version consistency check for CU enabling
        if (may_commute && !hw->can_precede_anything()) {
            for (history_type* h = head(cell); h != hw; h = h->prev(cell)) {
                if (!hw->can_precede(h)) {
                    hw->status_abort(ABORTED_WV1);
                    Sto::transaction()->mark_abort_because(
                            nullptr, "MVCC Lock: Write version consistency check #1.",
                            0, __FILE__, __LINE__, __FUNCTION__);
                    return false;
                }
            }
        }

        // Write version consistency check for concurrent reads + CU enabling
        history_type* h = nullptr;
        for (h = hw->prev(cell); h; h = h->prev(cell)) {
            if ((may_commute && !h->can_precede(hw))
                || (h->status_is(COMMITTED) && h->rtid() > tid)) {
                hw->status_abort(ABORTED_WV2);
                std::ostringstream reason;
                reason << "MVCC Lock: Write version consistency check #2 on cell "
                       << cell;
                Sto::transaction()->mark_abort_because(
                        nullptr, reason.str(),
                        0, __FILE__, __LINE__, __FUNCTION__);
                return false;
            }
            if (h->status_is(COMMITTED)) {
                break;
            }
        }

        if (!h->status_is(POISONED)) {
            return true;
        } else {
            hw->status_abort(ABORTED_POISON);
            Sto::transaction()->mark_abort_because(
                    nullptr, "MVCC Lock: Poisoned version",
                    0, __FILE__, __LINE__, __FUNCTION__);
            return false;
        }
    }

    // "Check" step: read timestamp updates and version consistency check;
    //               returns true if successful, false is aborted
    bool cp_check(const tid_type tid, history_type* hr, size_t cell=0) {
        // rtid update
        hr->update_rtid(tid);

        // XXX[wqian]
        //printf("ctid: %d, check tid: %d, head: %p, hr: %p, hr->wtid: %d\n", Sto::commit_tid(), Sto::read_tid(), head(cell), hr, hr->wtid_);

        // Read version consistency check
        for (history_type* h = head(cell); h != hr; h = h->prev(cell)) {
            if (!h->status_is(ABORTED) && h->wtid() < tid) {
#if VERBOSE > 0
                Transaction::fprint(
                        "hr: ", hr, "; h: ", h, "\n",
                        hr->list_prevs(10, cell));
#endif
                /*
                fprintf(stdout, "%p aborting %p @ %zu cell %zu at %p with status %x tid %zu/%zu\n", this, hr, hr->wtid(), cell, h, h->status(), h->wtid(), tid);
                for (size_t cell = 0; cell < Cells; ++cell) {
                    head(cell)->print_prevs(5, cell, stdout);
                }
                */
                return false;
            }
        }

        return !hr->status_is(POISONED);
    }

    // "Install" step: set status to committed
    void cp_install(history_type* h) {
        int s = h->status();
        if (s != 0x2 && s != 0x3) {
            //printf("%p installing %x\n", this, s);
        }
        h->assert_status((s & (PENDING | ABORTED)) == PENDING, "cp_install");
        h->status((s & ~PENDING) | COMMITTED);
        if (!(s & DELTA)) {
            cuctr_.store(0, std::memory_order_relaxed);
            flattenv_.store(0, std::memory_order_relaxed);
            /*
            if (this->is_inlined(h)) {
                //printf("%p enqueuing %p tid %zu for gc\n", this, h, h->wtid());
            }
            */
            h->enqueue_for_committed(__FILE__, __LINE__);
            //Transaction::rcu_call(gc_flatten_cb, this);
        } else {
            int dc = cuctr_.load(std::memory_order_relaxed) + 1;
            if (dc <= gc_flattening_length) {
                cuctr_.store(dc, std::memory_order_relaxed);
            } else if (flattenv_.load(std::memory_order_relaxed) == 0) {
                cuctr_.store(0, std::memory_order_relaxed);
                flattenv_.store(h->wtid(), std::memory_order_relaxed);
                Transaction::rcu_call(gc_flatten_cb, this);
            }
        }
    }

    // Deletes the history element if it was new'ed, or set it as UNUSED if it
    // is the inlined version
    // !!! IMPORTANT !!!
    // This function should only be used to free history nodes that have NOT been
    // hooked into the version chain
    void delete_history(history_type* h) {
#if VERBOSE > 0
        Transaction::fprint(
                "Thread ", TThread::id(), " deleting from ", this, " element: ",
                h, " status ", h->status(),
                " with ", h->cells_.load(std::memory_order_relaxed),
                " cell connections remaining\n");
        assert(h->cells_.load(std::memory_order_relaxed) == -1);
        for (size_t cell = 0; cell < Cells; ++cell) {
            assert(h->prev_relaxed(cell) == nullptr);
        }
#endif
        if (is_inlined(h)) {
            //printf("%p deleting %p\n", this, h);
            h->status_.store(UNUSED, std::memory_order_release);
        } else {
#if MVCC_GARBAGE_DEBUG
            memset((void*)h, 0xFF, sizeof(base_type));
#endif
            delete h;
        }
    }

    template <bool B=(Cells == 1)>
    std::enable_if_t<B, history_type*>
    inline find(const tid_type tid, const bool wait=true) const {
        return find(tid, 0, wait);
    }

    inline history_type* find(const tid_type tid, const size_t cell, const bool wait=true) const {
        return find_from(head(cell), tid, cell, wait);
    }

    // Finds the current visible version, based on tid; by default, waits on
    // pending versions, but if toggled off, will simply return first version,
    // regardless of status
    inline history_type* find_from(history_type* h, const tid_type tid, const size_t cell, const bool wait=true) const {
        /* TODO: use something smarter than a linear scan */
        while (!find_iter(h, tid, wait)) {
            h = h->prev(cell);
        }

        assert(h);

        return h;
    }

    // A single iteration of find; returns true if the element is found, false otherwise
    inline bool find_iter(history_type* h, const tid_type tid, const bool wait) const {
        auto status = h->status();
        auto wtid = h->wtid();
        h->assert_status(status & (PENDING | ABORTED | COMMITTED), "find");
        if (wait) {
            if (wtid < tid) {
                h->wait_if_pending(status);
            }
            if (wtid <= tid && (status & COMMITTED)) {
                return true;
            }
        } else {
            if (wtid <= tid && (status & COMMITTED)) {
                return true;
            }
        }
        return false;
    }

    history_type* find_latest(const size_t cell, const bool wait) const {
        history_type* h = head();
        while (true) {
            auto status = h->status();
            h->assert_status(status & (PENDING | ABORTED | COMMITTED), "find_latest");
            if (wait) {
                h->wait_if_pending(status);
            }
            if (status & COMMITTED) {
                return h;
            }
            h = h->prev(cell);
        }
    }

    history_type* head(size_t cell=0) const {
        return reinterpret_cast<history_type*>(h_[cell].load(std::memory_order_acquire));
    }

    /*
    history_type* headhs() const {
        return reinterpret_cast<history_type*>(hs_.load(std::memory_order_relaxed));
    }
    */

    // Returns whether the given history element is the inlined version
    inline bool is_inlined(const history_type* h) const {
#if MVCC_INLINING
        return h == &ih_;
#else
        (void)h;
        return false;
#endif
    }

    // Returns a pointer to a history element, initialized with the given
    // parameters. This allows the MvObject to properly allocate the inlined
    // version as needed.
    template <typename... Args>
    history_type* new_history(Args&&... args) {
        history_type* h = nullptr;
#if MVCC_INLINING
        auto status = ih_.status();
        if (status == UNUSED &&
                ih_.status_.compare_exchange_strong(status, PENDING)) {
            //printf("%p reallocating %p\n", this, &ih_);
            // Use inlined history element
            new (&ih_) history_type(this, std::forward<Args>(args)...);
            h = &ih_;
        } else {
#endif
        h = new(std::nothrow) history_type(this, std::forward<Args>(args)...);
#if MVCC_INLINING
        }
#endif
        std::fill(h->prev_.begin(), h->prev_.end(), nullptr);
#if VERBOSE > 0
        Transaction::fprint("Allocating ", h, " to ", this, "\n");
#endif
        return h;
    }

    // Read-only
    const T& nontrans_access() const {
        history_type* h = head();
        history_type* next = nullptr;
        /* TODO: head version caching */
        while (h) {
            if (h->status_is(COMMITTED)) {
                if (h->status_is(DELTA)) {
                    h->enflatten();
                }
                return h->v();
            }
            next = h;
            h = next->prev();
        }
        // Should never get here!
        throw InvalidState();
    }
    // Writable version
    T& nontrans_access() {
        history_type* h = head();
        history_type* next = nullptr;
        /* TODO: head version caching */
        while (h) {
            if (h->status_is(COMMITTED)) {
                if (h->status_is(DELTA)) {
                    h->enflatten();
                }
                h->status(COMMITTED);
                return h->v();
            }
            next = h;
            h = next->prev();
        }
        // Should never get here!
        throw InvalidState();
    }

protected:
    static void gc_flatten_cb(void* ptr) {
        auto object = static_cast<MvObject<T, Cells>*>(ptr);
        auto flattenv = object->flattenv_.load(std::memory_order_relaxed);
        object->flattenv_.store(0, std::memory_order_relaxed);
        if (flattenv) {
            history_type* h = object->head();
            int status = h->status();
            while (h
                   && h->wtid() > flattenv
                   && (status & LOCKED_COMMITTED_DELTA) != COMMITTED
                   && (status & LOCKED_COMMITTED_DELTA) != LOCKED_COMMITTED_DELTA) {
                h = h->prev();
                status = h->status();
            }
            if (h && status == COMMITTED_DELTA) {
                if constexpr (Cells > 1 && commutators::CommAdapter::Properties<T>::has_record_accessor) {
                    std::array<history_type*, Cells> histories = {};
                    for (size_t cell = 0; cell < Cells; ++cell) {
                        histories[cell] = (h->prev_relaxed(cell) ? h : nullptr);
                    }
                    history_type::adaptive_flatten(h->wtid(), histories, false);
                } else {
                    h->flatten(status, false);
                }
            }
        }
    }

    std::array<std::atomic<base_type*>, Cells> h_;
    //std::atomic<base_type*> hs_;  // Split-changing shortchain
    std::atomic<int> cuctr_ = 0;  // For gc-time flattening
    std::atomic<tid_type> flattenv_;

    inline void setup(history_type* hnew) {
        for (size_t cell = 0; cell < Cells; ++cell) {
            h_[cell].store(hnew, std::memory_order::memory_order_relaxed);
        }
        hnew->cells_.store(Cells, std::memory_order::memory_order_relaxed);
    }

#if MVCC_INLINING
    history_type ih_;  // Inlined version
#endif

    void* auxdata_ = nullptr;  // Auxiliary data pointer

    friend class MvHistory<T, Cells>;
};
