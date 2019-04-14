// Contains core data structures for MVCC

#pragma once

#include <stack>

#include "MVCCTypes.hh"
#include "TRcu.hh"

// Status types of MvHistory elements
enum MvStatus {
    UNUSED                  = 0b0000000,
    ABORTED                 = 0b0100000,
    GARBAGE                 = 0b1000000,
    DELTA                   = 0b0001000,  // Commutative update delta
    DELETED                 = 0b0000001,  // Not a valid state on its own, but defined as a flag
    PENDING                 = 0b0000010,
    COMMITTED               = 0b0000100,
    PENDING_DELTA           = 0b0001010,
    COMMITTED_DELTA         = 0b0001100,
    PENDING_DELETED         = 0b0000011,
    COMMITTED_DELETED       = 0b0000101,
    LOCKED                  = 0b0010000,
    LOCKED_COMMITTED        = 0b0010100,
    LOCKED_COMMITTED_DELTA  = 0b0011100,  // Converting from delta to flattened
};


template <typename T>
class MvHistory {
public:
    typedef TransactionTid::type type;
    typedef TRcuSet::epoch_type epoch_type;
    typedef MvHistory<T> history_type;
    typedef MvObject<T> object_type;
    typedef commutators::Commutator<T> comm_type;

    MvHistory() = delete;
    explicit MvHistory(object_type *obj) : MvHistory(0, obj, nullptr) {}
    explicit MvHistory(
            type ntid, object_type *obj, const T& nv, history_type *nprev = nullptr)
            : obj_(obj), v_(nv), gc_enqueued_(false), prev_(nprev),
              status_(PENDING), rtid_(ntid), wtid_(ntid) {
        if (prev_) {
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev wtid.");
        }
    }
    explicit MvHistory(
            type ntid, object_type *obj, T&& nv, history_type *nprev = nullptr)
            : obj_(obj), v_(std::move(nv)), gc_enqueued_(false), prev_(nprev),
              status_(PENDING), rtid_(ntid), wtid_(ntid) {
        if (prev_) {
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev wtid.");
        }
    }
    explicit MvHistory(
            type ntid, object_type *obj, T *nvp, history_type *nprev = nullptr)
            : obj_(obj), gc_enqueued_(false), prev_(nprev), status_(PENDING),
              rtid_(ntid), wtid_(ntid) {
        if (prev_) {
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev wtid.");
        }

        if (nvp) {
            v_ = *nvp;
        }
    }
    explicit MvHistory(
            type ntid, object_type *obj, comm_type &&c, history_type *nprev = nullptr)
            : obj_(obj), c_(c), v_(), gc_enqueued_(false), prev_(nprev),
              status_(PENDING), rtid_(ntid), wtid_(ntid) {
        if (prev_) {
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev wtid.");
        }

        status_delta();
    }

    // Pushes the current element onto the GC list if it isn't already, along
    // with a boolean for whether it is the inlined version
    inline void gc_push(const bool inlined) {
        bool expected = false;
        if (gc_enqueued_.compare_exchange_strong(expected, true)) {
            hard_gc_push(inlined);
        }
    }

    inline void hard_gc_push(const bool inlined) {
        assert(gc_enqueued_.load());
        if (inlined) {
            Transaction::rcu_call(gc_inlined_cb, this);
        } else {
            Transaction::rcu_delete(this);
        }
    }

    // Return true if this history element is currently enqueued for GC
    inline bool is_gc_enqueued() const {
        return gc_enqueued_.load();
    }

    // Retrieve the object for which this history element is intended
    inline object_type* object() const {
        return obj_;
    }

    inline history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_.load());
    }

    // Attempt to set the pointer to prev; returns true on success
    inline bool prev(history_type *prev) {
        if (is_valid_prev(prev)) {
            prev_ = prev;
            return true;
        }
        return false;
    }

    // Returns the current rtid
    inline type rtid() const {
        return rtid_;
    }

    // Non-CAS assignment on the rtid
    inline type rtid(const type new_rtid) {
        rtid_ = new_rtid;
        return rtid();
    }

    // Proxy for a CAS assigment on the rtid
    inline type rtid(type prev_rtid, const type new_rtid) {
        assert(prev_rtid <= new_rtid);
        rtid_.compare_exchange_strong(prev_rtid, new_rtid);
        return rtid();
    }

    // Set whether current history element is enqueued for GC
    inline void set_gc_enqueued(const bool value) {
        gc_enqueued_ = value;
    }

    // Returns the status
    inline MvStatus status() const {
        return status_;
    }

    // Sets and returns the status
    // NOT THREADSAFE
    inline MvStatus status(MvStatus s) {
        status_ = s;
        return status_;
    }
    inline MvStatus status(unsigned long long s) {
        return status((MvStatus)s);
    }

    // Removes all flags, signaling an aborted status
    // NOT THREADSAFE
    inline MvStatus status_abort() {
        status(ABORTED);
        return status();
    }

    // Sets the committed flag and unsets the pending flag; does nothing if the
    // current status is ABORTED
    // NOT THREADSAFE
    inline MvStatus status_commit() {
        if (status() == ABORTED) {
            return status();
        }
        status(COMMITTED | (status() & ~PENDING));
        return status();
    }

    // Sets the deleted flag; does nothing if the current status is ABORTED
    // NOT THREADSAFE
    inline MvStatus status_delete() {
        if (status() == ABORTED) {
            return status();
        }
        status(status() | DELETED);
        return status();
    }

    // Sets the delta flag; does nothing if the current status is ABORTED
    // NOT THREADSAFE
    inline MvStatus status_delta() {
        if (status() == ABORTED) {
            return status();
        }
        status(status() | DELTA);
        return status();
    }

    // Sets the history into UNUSED state
    // NOT THREADSAFE
    inline MvStatus status_unused() {
        status(UNUSED);
        return status();
    }

    // Returns true if all the given flag(s) are set
    inline bool status_is(const MvStatus status) const {
        return status_is(status, status);
    }

    // Returns true if the status with the given mask equals the expected
    inline bool status_is(const MvStatus mask, const MvStatus expected) const {
        if (mask == 0 && expected == 0) {  // Special case
            return status_ == 0;
        }
        return (status_ & mask) == expected;
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

    inline T* vp_safe_flatten(type wtid_inf) {
        if (status_is(DELTA)) {
            if (wtid_ > wtid_inf)
                return nullptr;
            enflatten();
        }
        return &v_;
    }

    // Returns the current wtid
    inline type wtid() const {
        return wtid_;
    }
private:
    // Initializes the flattening process
    inline void enflatten() {
        T v{};
        assert(prev());
        TXP_INCREMENT(txp_mvcc_flat_runs);
        prev()->flatten(v);
        v = c_.operate(v);
        MvStatus expected = COMMITTED_DELTA;
        if (status_.compare_exchange_strong(expected, LOCKED_COMMITTED_DELTA)) {
            TXP_INCREMENT(txp_mvcc_flat_commits);
            v_ = v;
            status(COMMITTED);
        } else {
            TXP_INCREMENT(txp_mvcc_flat_spins);
            // TODO: exponential backoff?
            while (status_is(LOCKED_COMMITTED_DELTA));
        }
    }

    void flatten(T &v) {
        std::stack<history_type*> trace;
        history_type *curr = this;
        trace.push(curr);
        TXP_INCREMENT(txp_mvcc_flat_versions);
        while (curr->status() != COMMITTED) {
            curr = curr->prev();
            trace.push(curr);
            TXP_INCREMENT(txp_mvcc_flat_versions);
            curr->gc_push(object()->is_inlined(curr));
        }
        while (!trace.empty()) {
            auto h = trace.top();
            trace.pop();

            if (h->status_is(COMMITTED)) {
                if (h->status_is(DELTA)) {
                    v = h->c_.operate(v);
                } else {
                    v = h->v_;
                }
            }

        }
    }

    static void gc_inlined_cb(void *ptr) {
        history_type *h = static_cast<history_type*>(ptr);
        h->object()->delete_history(h);
    }

    // Returns true if the given prev pointer would be a valid prev element
    inline bool is_valid_prev(const history_type *prev) const {
        return prev->wtid_ <= wtid_;
    }

    // Returns the newest element that satisfies the expectation with the given
    // mask. Optionally increments a counter.
    inline history_type* with(const MvStatus mask, const MvStatus expect,
                        size_t * const visited=nullptr) const {
        history_type *ele = (history_type*)this;
        while (ele && !ele->status_is(mask, expect)) {
            ele = ele->prev_;
            if (visited) {
                (*visited)++;
            }
        }
        return ele;
    }

    object_type *obj_;  // Parent object
    comm_type c_;
    T v_;

    std::atomic<bool> gc_enqueued_;  // Whether this element is on the GC queue
    std::atomic<history_type*> prev_;
    std::atomic<MvStatus> status_;  // Status of this element
    std::atomic<type> rtid_;  // Read TID
    type wtid_;  // Write TID

    friend class MvObject<T>;
};

template <typename T>
class MvObject {
public:
    typedef MvHistory<T> history_type;
    typedef const T& read_type;
    typedef TransactionTid::type type;

#if MVCC_INLINING
    MvObject() : h_(&ih_), ih_(this) {
        ih_.status_commit();
        itid_ = ih_.rtid();
        if (std::is_trivial<T>::value) {
            ih_.v_ = T();
        } else {
            ih_.status_delete();
        }
    }
    explicit MvObject(const T& value)
            : h_(&ih_), ih_(0, this, value) {
        ih_.status_commit();
        itid_ = ih_.rtid();
    }
    explicit MvObject(T&& value)
            : h_(&ih_), ih_(0, this, value) {
        ih_.status_commit();
        itid_ = ih_.rtid();
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(&ih_), ih_(0, this, T(std::forward<Args>(args)...)) {
        ih_.status_commit();
        itid_ = ih_.rtid();
    }
#else
    MvObject() : h_(new history_type(this)) {
        h_.load()->status_commit();
        if (std::is_trivial<T>::value) {
            h_.load()->v_ = T();
        } else {
            h_.load()->status_delete();
        }
    }
    explicit MvObject(const T& value)
            : h_(new history_type(0, this, value)) {
        h_.load()->status_commit();
    }
    explicit MvObject(T&& value)
            : h_(new history_type(0, this, value)) {
        h_.load()->status_commit();
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(new history_type(0, this, T(std::forward<Args>(args)...))) {
        h_.load()->status_commit();
    }
#endif

    ~MvObject() {
        history_type *h = h_;
        while (h && !h->is_gc_enqueued()) {
            history_type *prev = h->prev();
            h->gc_push(is_inlined(h));
            h = prev;
        }
    }

    class InvalidState {};

    // Aborts currently-pending head version; returns true if the head version
    // is pending and false otherwise.
    bool abort(history_type *h) {
        if (h) {
            if (!h->status_is(MvStatus::PENDING)) {
                return false;
            }

            h->status_abort();
        }
        return true;
    }

    const T& access(const type tid) const {
        return find(tid)->v();
    }
    T& access(const type tid) {
        return find(tid)->v();
    }

    // "Check" step: read timestamp updates and version consistency check;
    //               returns true if successful, false is aborted
    bool cp_check(const type tid, history_type *h /* read item */) {
        // rtid update
        type prev_rtid;
        while ((prev_rtid = h->rtid()) < tid) {
            h->rtid(prev_rtid, tid);  // CAS-based rtid update
        }

        // Version consistency check
        if (find(tid) != h) {
            return false;
        }

        return true;
    }

    // "Install" step: set status to committed if head element is pending; fails
    //                 otherwise
    bool cp_install(history_type *h) {
        if (!h->status_is(MvStatus::PENDING)) {
            return false;
        }

        h->status_commit();
#if MVCC_INLINING
        if (h->prev() == &ih_) {
            itid_ = h->wtid();
        }
#endif

        if (h->status_is(COMMITTED_DELTA, COMMITTED)) {
            // Put predecessors in the RCU list, but only if they aren't already
            history_type *prev = h->prev();
            bool stop = prev->is_gc_enqueued();
            do {
                stop = prev->status_is(COMMITTED_DELTA, COMMITTED) ||
                       prev->is_gc_enqueued();
                history_type* ele = prev;
                prev = prev->prev();
                ele->gc_push(is_inlined(ele));
            } while(!stop);
        }

        return true;
    }

    // "Lock" step: pending version installation & version consistency check;
    //              returns true if successful, false if aborted
    bool cp_lock(const type tid, history_type *h) {
        // Can only install pending versions
        if (!h->status_is(MvStatus::PENDING)) {
            return false;
        }

        // The previously-visible version for h
        history_type *hvis = h->prev();

        do {
            // Can only install onto the latest-visible version
            if (hvis && hvis != find(tid)) {
                return false;
            }

            // Discover target atomic on which to do CAS
            std::atomic<history_type*> *target = &h_;
            history_type* t = *target;
            while (t->wtid() > tid) {
                target = &t->prev_;
                t = *target;
            }

            // Properly link h's prev_
            h->prev_ = t;

            // Attempt to CAS onto the target
            if (target->compare_exchange_strong(t, h)) {
                if (t->is_gc_enqueued()) {
                    h->gc_push(is_inlined(h));
                }
                break;
            }
        } while (true);

        // Version consistency verification
        if (h->prev()->rtid() > tid) {
            h->status_abort();
            return false;
        }

        return true;
    }

    // Deletes the history element if it was new'ed, or set it as UNUSED if it
    // is the inlined version
    void delete_history(history_type *h) {
#if MVCC_INLINING
        if (&ih_ == h) {
            itid_ = 0;
            ih_.status_unused();
            return;
        }
#endif
        delete h;
    }

    // Finds the current visible version, based on tid; by default, waits on
    // pending versions, but if toggled off, will simply return first version,
    // regardless of status
    history_type* find(const type tid, const bool wait=true) const {
        history_type *h;
#if MVCC_INLINING
        if ((((ih_.status() & LOCKED_COMMITTED) == COMMITTED) ||
                    ih_.status_is(ABORTED)) &&
                tid < itid_) {
            h = (history_type*)&ih_;
        } else {
            h = h_;
        }
#else
        h = h_;
#endif
        /* TODO: use something smarter than a linear scan */
        while (h) {
            assert(h->status() & (PENDING | ABORTED | COMMITTED));
            if (wait) {
                if (h->wtid() < tid) {
                    wait_if_pending(h);
                }
                if (h->wtid() <= tid && h->status_is(COMMITTED)) {
                    break;
                }
            } else {
                if (h->wtid() <= tid && h->status_is(COMMITTED)) {
                    break;
                }
            }
            h = h->prev();
        }

        assert(h);

        return h;
    }

    // Returns whether the given history element is the inlined version
    inline bool is_inlined(history_type * const h) const {
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
            new (&ih_) history_type(std::forward<Args>(args)...);
            itid_ = ih_.rtid();
            return &ih_;
        }
#endif
        return new history_type(std::forward<Args>(args)...);
    }

    // Read-only
    const T& nontrans_access() const {
        history_type *h = h_;
        history_type *next = nullptr;
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
        history_type *h = h_;
        history_type *next = nullptr;
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
    // Spin-wait on interested item
    void wait_if_pending(const history_type *h) const {
        while (h->status_is(MvStatus::PENDING)) {
            // TODO: implement a backoff or something
            fence();
        }
    }

    std::atomic<history_type*> h_;
#if MVCC_INLINING
    history_type ih_;  // Inlined version
    std::atomic<type> itid_;  // TID representing until when the inlined version is correct
#endif
};
