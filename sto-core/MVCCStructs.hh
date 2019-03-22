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


class MvHistoryBase {
public:
    typedef TransactionTid::type type;
    typedef MvHistoryBase base_type;
    typedef TRcuSet::epoch_type epoch_type;

    virtual ~MvHistoryBase() {}

    // Attempt to set the pointer to prev; returns true on success
    bool prev(base_type *prev) {
        if (is_valid_prev(prev)) {
            prev_ = prev;
            return true;
        }
        return false;
    }

    // Return true if this history element is currently enqueued for GC
    bool is_gc_enqueued() const {
        return gc_enqueued_.load();
    }

    // Set whether current history element is enqueued for GC
    void set_gc_enqueued(const bool value) {
        gc_enqueued_ = value;
    }

    // Pushes the current element onto the GC list if it isn't already, along
    // with a boolean for whether it is the inlined version
    void gc_push(const bool inlined);
    void hard_gc_push(const bool inlined);

    // Returns the current rtid
    type rtid() const {
        return rtid_;
    }

    // Non-CAS assignment on the rtid
    type rtid(const type new_rtid) {
        rtid_ = new_rtid;
        return rtid();
    }

    // Proxy for a CAS assigment on the rtid
    type rtid(type prev_rtid, const type new_rtid) {
        assert(prev_rtid <= new_rtid);
        rtid_.compare_exchange_strong(prev_rtid, new_rtid);
        return rtid();
    }

    // Returns the status
    MvStatus status() const {
        return status_;
    }

    // Sets and returns the status
    // NOT THREADSAFE
    MvStatus status(MvStatus s) {
        status_ = s;
        return status_;
    }
    MvStatus status(unsigned long long s) {
        return status((MvStatus)s);
    }

    // Removes all flags, signaling an aborted status
    // NOT THREADSAFE
    MvStatus status_abort() {
        status(ABORTED);
        return status();
    }

    // Sets the committed flag and unsets the pending flag; does nothing if the
    // current status is ABORTED
    // NOT THREADSAFE
    MvStatus status_commit() {
        if (status() == ABORTED) {
            return status();
        }
        status(COMMITTED | (status() & ~PENDING));
        return status();
    }

    // Sets the deleted flag; does nothing if the current status is ABORTED
    // NOT THREADSAFE
    MvStatus status_delete() {
        if (status() == ABORTED) {
            return status();
        }
        status(status() | DELETED);
        return status();
    }

    // Sets the delta flag; does nothing if the current status is ABORTED
    // NOT THREADSAFE
    MvStatus status_delta() {
        if (status() == ABORTED) {
            return status();
        }
        status(status() | DELTA);
        return status();
    }

    // Sets the history into UNUSED state
    // NOT THREADSAFE
    MvStatus status_unused() {
        status(UNUSED);
        return status();
    }

    // Returns true if all the given flag(s) are set
    bool status_is(const MvStatus status) const {
        return status_is(status, status);
    }

    // Returns true if the status with the given mask equals the expected
    bool status_is(const MvStatus mask, const MvStatus expected) const {
        if (mask == 0 && expected == 0) {  // Special case
            return status_ == 0;
        }
        return (status_ & mask) == expected;
    }

    // Returns the current wtid
    type wtid() const {
        return wtid_;
    }

protected:
    MvHistoryBase(type ntid, base_type *nprev = nullptr)
            : prev_(nprev), status_(PENDING), rtid_(ntid), wtid_(ntid),
              gc_enqueued_(false) {

        if (prev_) {
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev wtid.");
        }
    }

    std::atomic<base_type*> prev_;
    std::atomic<MvStatus> status_;  // Status of this element

    virtual void enflatten() {}
    virtual void gc_inlined_cb_() {}

private:
    // GC callback for inlined versions
    static void gc_inlined_cb(void *ptr) {
        base_type *h = static_cast<base_type*>(ptr);
        h->gc_inlined_cb_();
    }

    // Returns true if the given prev pointer would be a valid prev element
    bool is_valid_prev(const base_type *prev) const {
        return prev->wtid_ <= wtid_;
    }

    // Returns the newest element that satisfies the expectation with the given
    // mask. Optionally increments a counter.
    base_type* with(const MvStatus mask, const MvStatus expect,
                        size_t * const visited=nullptr) const {
        base_type *ele = (base_type*)this;
        while (ele && !ele->status_is(mask, expect)) {
            ele = ele->prev_;
            if (visited) {
                (*visited)++;
            }
        }
        return ele;
    }

    std::atomic<type> rtid_;  // Read TID
    type wtid_;  // Write TID
    std::atomic<bool> gc_enqueued_;  // Whether this element is on the GC queue

    template <typename>
    friend class MvObject;
};

template <typename T>
class MvHistory : public MvHistoryBase {
public:
    typedef MvHistory<T> history_type;
    typedef MvObject<T> object_type;
    typedef commutators::Commutator<T> comm_type;

    MvHistory() = delete;
    explicit MvHistory(object_type *obj) : MvHistory(0, obj, nullptr) {}
    explicit MvHistory(
            type ntid, object_type *obj, const T& nv, history_type *nprev = nullptr)
            : base_type(ntid, nprev), obj_(obj), v_(nv) {}
    explicit MvHistory(
            type ntid, object_type *obj, T&& nv, history_type *nprev = nullptr)
            : base_type(ntid, nprev), obj_(obj), v_(std::move(nv)) {}
    explicit MvHistory(
            type ntid, object_type *obj, T *nvp, history_type *nprev = nullptr)
            : base_type(ntid, nprev), obj_(obj) {
        if (nvp) {
            v_ = *nvp;
        }
    }
    explicit MvHistory(
            type ntid, object_type *obj, comm_type &&c, history_type *nprev = nullptr)
            : base_type(ntid, nprev), obj_(obj), c_(c), v_() {
        status_delta();
    }

    ~MvHistory() override {}

    // Retrieve the object for which this history element is intended
    object_type* object() const {
        return obj_;
    }

    history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_.load());
    }

    T& v() {
        if (status_is(DELTA)) {
            enflatten();
        }
        return v_;
    }

    T* vp() {
        if (status_is(DELTA)) {
            enflatten();
        }
        return &v_;
    }

private:
    object_type *obj_;  // Parent object
    comm_type c_;
    T v_;

    // Initializes the flattening process
    void enflatten() override {
        T v{};
        assert(prev());
        prev()->flatten(v);
        v = c_.operate(v);
        MvStatus expected = COMMITTED_DELTA;
        if (status_.compare_exchange_strong(expected, LOCKED_COMMITTED_DELTA)) {
            v_ = v;
            status(COMMITTED);
        } else {
            // TODO: exponential backoff?
            while (status_is(LOCKED_COMMITTED_DELTA));
        }
    }

    void flatten(T &v) {
        std::stack<history_type*> trace;
        history_type *curr = this;
        trace.push(curr);
        while (curr->status() != COMMITTED) {
            curr = curr->prev();
            trace.push(curr);
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

    void gc_inlined_cb_() override {
        object()->delete_history((history_type*)this);
    }

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
            static_cast<history_type*>(h_.load())->v_ = T();
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
        /*
        base_type *h = h_;
        h_ = nullptr;
        while (h) {
#if MVCC_INLINING
            if (&ih_ == h->prev_) {
                h->prev_ = ih_.prev_.load();
                ih_.status_unused();
            }
#endif
            h->rtid_ = h->wtid_ = 0;  // Make it GC-able
            h = h->prev_;
        }
        */
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
            bool stop = false;
            do {
                stop = prev->status_is(COMMITTED_DELTA, COMMITTED);
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
            std::atomic<base_type*> *target = &h_;
            base_type* t = *target;
            while (t->wtid() > tid) {
                target = &t->prev_;
                t = *target;
            }
            const bool target_enqueued = t->is_gc_enqueued();

            // Properly link h's prev_
            h->prev_ = t;

            // Depending on h's immediate neighbors in the chain,
            // set h's enqueue status
            if (target_enqueued) {
                // target_enqueued == true means h is to be installed
                // under a flat version, and the flat version already
                // started putting its predecessors to the GC queue, and
                // it have already missed h.
                // h must immediately put itself to GC queue otherwise
                // it will be leaked.
                h->set_gc_enqueued(true);
                // In this situation, I believe h actually does not need
                // to attempt to enqueue any of its predecessors during
                // cp_install(). Need proof, but feels correct to me.
                // We currently don't carry this information over but we
                // could optimize it later.
            }

            // Attempt to CAS onto the target
            if (target->compare_exchange_strong(t, h)) {
                if (target_enqueued) {
                    h->hard_gc_push(is_inlined(h));
                }
                break;
            }
            // Revert GC enqueue status before retrying
            if (target_enqueued)
                h->set_gc_enqueued(false);
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
            ih_.gc_enqueued_ = false;
            ih_.rtid_ = 0;
            ih_.wtid_ = 0;
            ih_.prev_ = 0;
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
            h = static_cast<history_type*>(h_.load());
        }
#else
        h = static_cast<history_type*>(h_.load());
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
        history_type *h = static_cast<history_type*>(h_.load());
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
        history_type *h = static_cast<history_type*>(h_.load());
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
    typedef MvHistoryBase base_type;

    // Spin-wait on interested item
    void wait_if_pending(const base_type *h) const {
        while (h->status_is(MvStatus::PENDING)) {
            // TODO: implement a backoff or something
            fence();
        }
    }

    std::atomic<base_type*> h_;
#if MVCC_INLINING
    history_type ih_;  // Inlined version
    std::atomic<type> itid_;  // TID representing until when the inlined version is correct
#endif
};
