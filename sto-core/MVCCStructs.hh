// Contains core data structures for MVCC

#pragma once

#include <deque>
#include <stack>
#include <thread>

#include "MVCCTypes.hh"
#include "Transaction.hh"
#include "TRcu.hh"
#define MVCC_GARBAGE_DEBUG 1

// Status types of MvHistory elements
enum MvStatus {
    UNUSED                  = 0b0000'0000'0000,
    ABORTED                 = 0b0000'0010'0000,
    ABORTED_RV              = 0b0001'0010'0000,
    ABORTED_WV1             = 0b0010'0010'0000,
    ABORTED_WV2             = 0b0011'0010'0000,
    ABORTED_TXNV            = 0b0100'0010'0000,
    ABORTED_POISON          = 0b0101'0010'0000,
    DELTA                   = 0b0000'0000'1000,  // Commutative update delta
    POISONED                = 0b1000'0000'0000,  // Standalone poison bit
    DELETED                 = 0b0000'0000'0001,  // Not a valid state on its own, but defined as a flag
    PENDING                 = 0b0000'0000'0010,
    COMMITTED               = 0b0000'0000'0100,
    PENDING_DELTA           = 0b0000'0000'1010,
    COMMITTED_DELTA         = 0b0000'0000'1100,
    PENDING_DELETED         = 0b0000'0000'0011,
    COMMITTED_DELETED       = 0b0000'0000'0101,
    LOCKED                  = 0b0000'0001'0000,
    LOCKED_COMMITTED_DELTA  = 0b0000'0001'1100,  // Converting from delta to flattened
    GARBAGE                 = 0b0000'0100'0000,
    GARBAGE2                = 0b0000'1000'0000,
};

std::ostream& operator<<(std::ostream& w, MvStatus s);

class MvHistoryBase {
public:
    using tid_type = TransactionTid::type;

    MvHistoryBase() = delete;
    MvHistoryBase(void* obj, tid_type tid, MvStatus status)
        : status_(status), wtid_(tid), rtid_(tid), prev_(nullptr),
          obj_(obj) {
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
    void assert_status_fail(const char* description);
#endif

    void print_prevs(size_t max = 1000) const;

    std::atomic<MvStatus> status_;  // Status of this element
    tid_type wtid_;  // Write TID
    std::atomic<tid_type> rtid_;  // Read TID
    std::atomic<MvHistoryBase*> prev_;
    void* obj_;  // Parent object
};

template <typename T>
class MvHistory : protected MvHistoryBase {
public:
    typedef TransactionTid::type tid_type;
    typedef TRcuSet::epoch_type epoch_type;
    typedef MvHistory<T> history_type;
    typedef MvObject<T> object_type;
    typedef commutators::Commutator<T> comm_type;

    MvHistory() = delete;
    explicit MvHistory(object_type *obj)
        : MvHistory(obj, 0, nullptr) {
    }
    explicit MvHistory(
            object_type *obj, tid_type ntid, const T& nv)
        : MvHistoryBase(obj, ntid, PENDING),
          v_(nv) {
    }
    explicit MvHistory(
            object_type *obj, tid_type ntid, T&& nv)
        : MvHistoryBase(obj, ntid, PENDING),
          v_(std::move(nv)) {
    }
    explicit MvHistory(
            object_type *obj, tid_type ntid, T *nvp)
        : MvHistoryBase(obj, ntid, PENDING),
          v_(nvp ? *nvp : v_ /*XXXXXXX*/) {
    }
    explicit MvHistory(
            object_type *obj, tid_type ntid, comm_type &&c)
        : MvHistoryBase(obj, ntid, PENDING_DELTA),
          c_(std::move(c)), v_() {
    }

    // Whether this version can be late-inserted in front of hnext
    bool can_precede(const history_type* hnext) const {
        return !this->status_is(DELETED) || !hnext->status_is(DELTA);
    }

    bool can_precede_anything() const {
        return !this->status_is(DELETED);
    }

    // Enqueues the deleted version for future cleanup
    inline void enqueue_for_committed() {
        assert_status((status() & COMMITTED_DELTA) == COMMITTED, "enqueue_for_committed");
        Transaction::rcu_call(gc_committed_cb, this);
    }

    // Retrieve the object for which this history element is intended
    inline object_type* object() const {
        return static_cast<object_type*>(obj_);
    }

    inline history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_.load());
    }

    // Returns the current rtid
    inline tid_type rtid() const {
        return rtid_;
    }

private:
    inline history_type* prev_relaxed() const {
        return reinterpret_cast<history_type*>(prev_.load(std::memory_order_relaxed));
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

    inline T& v() {
        if (status_is(DELTA)) {
            enflatten();
        }
        return v_;
    }

    inline T* vp() {
        if (status_is(DELTA)) {
            enflatten();
        }
        return &v_;
    }

    // Returns the current wtid
    inline tid_type wtid() const {
        return wtid_;
    }

private:
    static void gc_committed_cb(void* ptr) {
        history_type* h = static_cast<history_type*>(ptr);
        h->assert_status((h->status() & COMMITTED_DELTA) == COMMITTED, "gc_committed_cb");
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
        history_type* next = h->prev_relaxed();
        while (next) {
            h = next;
            next = h->prev_relaxed();
            MvStatus status = h->status();
#if MVCC_GARBAGE_DEBUG
            h->assert_status(!(status & (GARBAGE | GARBAGE2)), "gc_committed_cb garbage tracking");
            while (!(status & GARBAGE)
                   && h->status_.compare_exchange_weak(status, MvStatus(status | GARBAGE))) {
            }
#endif
            Transaction::rcu_call(gc_deleted_cb, h);
            h->assert_status(!(status & (LOCKED | PENDING)), "gc_committed_cb unlocked not pending");
            if ((status & COMMITTED_DELTA) == COMMITTED) {
                break;
            }
        }
    }

    static void gc_deleted_cb(void* ptr) {
        history_type* h = static_cast<history_type*>(ptr);
        MvStatus status = h->status_.load(std::memory_order_relaxed);
#if MVCC_GARBAGE_DEBUG
        h->assert_status((status & GARBAGE), "gc_deleted_cb garbage");
        h->assert_status(!(status & GARBAGE2), "gc_deleted_cb garbage");
        bool ok = h->status_.compare_exchange_strong(status, MvStatus(status | GARBAGE2));
        assert(ok);
#endif
        h->object()->delete_history(h);
    }

    // Initializes the flattening process
    inline void enflatten() {
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

        // Current element is the one initiating the flattening here. It is not
        // included in the trace, but it is included in the committed trace.
        history_type* curr = this;
        std::stack<history_type*> trace;  // Of history elements to process

        while (!curr->status_is(COMMITTED_DELTA, COMMITTED)) {
            trace.push(curr);
            curr = curr->prev();
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
            v_ = value;
            TXP_INCREMENT(txp_mvcc_flat_commits);
            status(COMMITTED);
            enqueue_for_committed();
            if (fg) {
                object()->flattenv_.store(0, std::memory_order_relaxed);
            }
        } else {
            TXP_INCREMENT(txp_mvcc_flat_spins);
        }
    }

    comm_type c_;
    T v_;

    friend class MvObject<T>;
};

template <typename T>
class MvObject {
public:
    typedef MvHistory<T> history_type;
    typedef const T& read_type;
    typedef TransactionTid::type tid_type;

    // How many consecutive DELTA versions will be allowed before flattening
    static constexpr int gc_flattening_length = 257;

#if MVCC_INLINING
    MvObject() : h_(&ih_), ih_(this) {
        if (std::is_trivial<T>::value) {
            ih_.v_ = T();
        }
        ih_.status(COMMITTED_DELETED);
    }
    explicit MvObject(const T& value)
            : h_(&ih_), ih_(this, 0, value) {
        ih_.status(COMMITTED);
    }
    explicit MvObject(T&& value)
            : h_(&ih_), ih_(this, 0, std::move(value)) {
        ih_.status(COMMITTED);
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(&ih_), ih_(this, 0, T(std::forward<Args>(args)...)) {
        ih_.status(COMMITTED);
    }
#else
    MvObject() : h_(new history_type(this)) {
        if (std::is_trivial<T>::value) {
            head()->v_ = T(); /* XXXX */
        }
        head()->status(COMMITTED_DELETED);
    }
    explicit MvObject(const T& value)
            : h_(new history_type(this, 0, value)) {
        head()->status(COMMITTED);
    }
    explicit MvObject(T&& value)
            : h_(new history_type(this, 0, std::move(value))) {
        head()->status(COMMITTED);
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(new history_type(this, 0, T(std::forward<Args>(args)...))) {
        head()->status(COMMITTED);
    }
#endif

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

    // "Lock" step: pending version installation & version consistency check;
    //              returns true if successful, false if aborted
    template <bool may_commute = true>
    bool cp_lock(const tid_type tid, history_type* hw) {
        // Can only install pending versions
        hw->assert_status(hw->status_is(PENDING), "cp_lock pending");

        std::atomic<MvHistoryBase*>* target = &h_;
        while (true) {
            // Discover target atomic on which to do CAS
            MvHistoryBase* t = *target;
            if (t->wtid_ > tid) {
                if (may_commute && !hw->can_precede(static_cast<history_type*>(t))) {
                    return false;
                }
                target = &t->prev_;
            } else if (!(t->status_.load(std::memory_order_acquire) & ABORTED)
                       && t->rtid_.load(std::memory_order_acquire) > tid) {
                return false;
            } else {
                // Properly link h's prev_
                hw->prev_.store(t, std::memory_order_release);

                // Attempt to CAS onto the target
                if (target->compare_exchange_strong(t, hw)) {
                    break;
                }
            }
        }

        // Write version consistency check for CU enabling
        if (may_commute && !hw->can_precede_anything()) {
            for (history_type* h = head(); h != hw; h = h->prev()) {
                if (!hw->can_precede(h)) {
                    hw->status_abort(ABORTED_WV1);
                    return false;
                }
            }
        }

        // Write version consistency check for concurrent reads + CU enabling
        history_type* h = nullptr;
        for (h = hw->prev(); h; h = h->prev()) {
            if ((may_commute && !h->can_precede(hw))
                || (h->status_is(COMMITTED) && h->rtid() > tid)) {
                hw->status_abort(ABORTED_WV2);
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
            return false;
        }
    }

    // "Check" step: read timestamp updates and version consistency check;
    //               returns true if successful, false is aborted
    bool cp_check(const tid_type tid, history_type* hr) {
        // rtid update
        hr->update_rtid(tid);

        // Read version consistency check
        for (history_type* h = head(); h != hr; h = h->prev()) {
            if (!h->status_is(ABORTED) && h->wtid() < tid) {
                return false;
            }
        }

        return !hr->status_is(POISONED);
    }

    // "Install" step: set status to committed
    void cp_install(history_type* h) {
        int s = h->status();
        h->assert_status((s & (PENDING | ABORTED)) == PENDING, "cp_install");
        h->status((s & ~PENDING) | COMMITTED);
        if (!(s & DELTA)) {
            cuctr_.store(0, std::memory_order_relaxed);
            flattenv_.store(0, std::memory_order_relaxed);
            h->enqueue_for_committed();
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
        if (is_inlined(h)) {
            h->status_.store(UNUSED, std::memory_order_release);
        } else {
#if MVCC_GARBAGE_DEBUG
            memset(h, 0xFF, sizeof(MvHistoryBase));
#endif
            delete h;
        }
    }

    // Finds the current visible version, based on tid; by default, waits on
    // pending versions, but if toggled off, will simply return first version,
    // regardless of status
    history_type* find(const tid_type tid, const bool wait=true) const {
        history_type* h = head();

        /* TODO: use something smarter than a linear scan */
        while (h) {
            auto status = h->status();
            auto wtid = h->wtid();
            h->assert_status(status & (PENDING | ABORTED | COMMITTED), "find");
            if (wait) {
                if (wtid < tid) {
                    h->wait_if_pending(status);
                }
                if (wtid <= tid && (status & COMMITTED)) {
                    break;
                }
            } else {
                if (wtid <= tid && (status & COMMITTED)) {
                    break;
                }
            }
            h = h->prev();
        }

        assert(h);

        return h;
    }

    history_type* head() const {
        return reinterpret_cast<history_type*>(h_.load(std::memory_order_acquire));
    }

    history_type* find_latest(const bool wait = true) const {
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
            h = h->prev();
        }
    }

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
#if MVCC_INLINING
        auto status = ih_.status();
        if (status == UNUSED &&
                ih_.status_.compare_exchange_strong(status, PENDING)) {
            // Use inlined history element
            new (&ih_) history_type(this, std::forward<Args>(args)...);
            return &ih_;
        }
#endif
        return new(std::nothrow) history_type(this, std::forward<Args>(args)...);
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
        auto object = static_cast<MvObject<T>*>(ptr);
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
                h->flatten(status, false);
            }
        }
    }

    std::atomic<MvHistoryBase*> h_;
    std::atomic<int> cuctr_ = 0;  // For gc-time flattening
    std::atomic<tid_type> flattenv_;

#if MVCC_INLINING
    history_type ih_;  // Inlined version
#endif

    friend class MvHistory<T>;
};
