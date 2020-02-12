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

// An (object, tid) reference for when version pointers are unreliable
template <typename T>
struct MvLocation {
    MvObject<T>* obj;
    TransactionTid::type tid;
};

template <typename T>
struct MvDelLocation {
    MvObject<T>* obj;
    MvHistory<T>* h_del;
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
              status_(PENDING), rtid_(ntid), wtid_(ntid), delete_cb(nullptr) {
        if (prev_) {
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev wtid.");
        }
    }
    explicit MvHistory(
            type ntid, object_type *obj, T&& nv, history_type *nprev = nullptr)
            : obj_(obj), v_(std::move(nv)), gc_enqueued_(false), prev_(nprev),
              status_(PENDING), rtid_(ntid), wtid_(ntid), delete_cb(nullptr) {
        if (prev_) {
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev wtid.");
        }
    }
    explicit MvHistory(
            type ntid, object_type *obj, T *nvp, history_type *nprev = nullptr)
            : obj_(obj), gc_enqueued_(false), prev_(nprev), status_(PENDING),
              rtid_(ntid), wtid_(ntid), delete_cb(nullptr) {
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
              status_(PENDING), rtid_(ntid), wtid_(ntid), delete_cb(nullptr) {
        if (prev_) {
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev wtid.");
        }

        status_delta();
    }

    // Enqueues the deleted version for future cleanup
    inline void enqueue_for_delete() {
        if (!status_is(COMMITTED_DELETED)) {
            return;
        }
        auto location = new MvDelLocation<T>();
        location->h_del = this;
        location->obj = object();
        Transaction::rcu_call(delete_prep_cb, location);
    }

    // Enqueues the delta version for future forced flattening
    inline void enqueue_for_flattening() {
        if (!status_is(DELTA)) {
            return;
        }
        auto location = new MvLocation<T>();
        location->obj = object();
        location->tid = wtid();
        Transaction::rcu_call(gc_time_flattening_cb, location);
    }

    // Pushes the current element onto the GC list if it isn't already, along
    // with a boolean for whether it is the inlined version.
    // Returns whether the push succeeded (fails if already on queue)
    inline bool gc_push(const bool inlined) {
        bool expected = false;
        if (gc_enqueued_.compare_exchange_strong(expected, true)) {
            hard_gc_push(inlined);
            return true;
        }
        return false;
    }

    inline void hard_gc_push(const bool inlined) {
        assert(gc_enqueued_.load());
        if (inlined) {
#if MVCC_INLINING
            Transaction::rcu_call(gc_inlined_cb, this);
#else
            assert(false);
#endif
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
    inline bool prev(history_type* prev) {
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

    // Sets the delete callback function
    inline void set_delete_cb(void *index_p,
            void (*f) (void*, void*, void*), void *param) {
        index_ptr = index_p;
        delete_cb = f;
        delete_param = param;
    }

    // Returns the status
    inline MvStatus status() const {
        return status_.load();
    }

    // Sets and returns the status
    // NOT THREADSAFE
    inline MvStatus status(MvStatus s) {
        status_.store(s, std::memory_order_release);
        return status_.load(std::memory_order_acquire);
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

#if SAFE_FLATTEN
    inline T* vp_safe_flatten();
#endif

    // Returns the current wtid
    inline type wtid() const {
        return wtid_;
    }

private:
    static void delete_prep_cb(void *ptr) {
        auto location = reinterpret_cast<MvDelLocation<T>*>(ptr);
        history_type* hd = location->h_del;  // DELETED version
        history_type* h = location->obj->h_;
        while (h) {
            if (h == hd) {
                hd->delete_cb(hd->index_ptr, hd->delete_param, hd);
                break;
            }

            // A committed version means we can skip physical deletion
            if (h->status_is(COMMITTED)) {
                break;
            }

            h = h->prev();
        }
        delete location;
    }

    // Initializes the flattening process
    inline void enflatten() {
        T v{};
        if (status_is(COMMITTED_DELTA)) {
            assert(prev());
            TXP_INCREMENT(txp_mvcc_flat_runs);
#if CU_READ_AT_PRESENT
            prev()->flatten(v, wtid_);
#else
            prev()->flatten(v);
#endif
            v = c_.operate(v);
        }
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

#if CU_READ_AT_PRESENT
    void flatten(T &v, type next_wtid) {
#else
    void flatten(T &v) {
#endif
        std::stack<history_type*> trace;
        history_type* curr = this;
#if CU_READ_AT_PRESENT
        while (true) {
            type ets = curr->rtid();
            if (ets >= next_wtid) break;
            if (curr->rtid(ets, next_wtid)) break;
        }
#endif
        trace.push(curr);
        TXP_INCREMENT(txp_mvcc_flat_versions);
#if CU_READ_AT_PRESENT
        while (true) {
            // Wait for pending versions to resolve.
            while (curr->status_is(PENDING)) { relax_fence(); }
            // Means we have reached the "base" committed version.
            if (curr->status_is(COMMITTED_DELTA, COMMITTED)) { break; }

            curr = curr->prev();

            while (true) {
                type ets = curr->rtid();
                if (ets >= next_wtid) break;
                if (curr->rtid(ets, next_wtid)) break;
            }
#else
        while (!curr->status_is(COMMITTED_DELTA, COMMITTED)) {
            curr = curr->prev();
#endif
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
                } else if (!h->status_is(COMMITTED_DELETED)) {
                    v = h->v_;
                }
            }

        }
    }

    static void gc_inlined_cb(void *ptr) {
        history_type* h = static_cast<history_type*>(ptr);
        assert(h->gc_enqueued_.load());
        h->object()->ih_.status_unused();
    }

    static void gc_time_flattening_cb(void *ptr) {
        auto location = static_cast<MvLocation<T>*>(ptr);
        history_type* h = location->obj->head();
        while (h && !h->status_is(COMMITTED_DELTA, COMMITTED) &&
                (h->wtid() > location->tid)) {
            h = h->prev();
        }

        if (h && (h->wtid() == location->tid) &&
                h->status_is(COMMITTED_DELTA) && !h->is_gc_enqueued()) {
            h->enflatten();
        }
        delete location;
    }

    // Returns true if the given prev pointer would be a valid prev element
    inline bool is_valid_prev(const history_type* prev) const {
        return prev->wtid_ <= wtid_;
    }

    // Returns the newest element that satisfies the expectation with the given
    // mask. Optionally increments a counter.
    inline history_type* with(const MvStatus mask, const MvStatus expect,
                        size_t * const visited=nullptr) const {
        history_type* ele = (history_type*)this;
        while (ele && !ele->status_is(mask, expect)) {
            ele = ele->prev_;
            if (visited) {
                (*visited)++;
            }
        }
        return ele;
    }

    object_type * const obj_;  // Parent object
    comm_type c_;
    T v_;

    std::atomic<bool> gc_enqueued_;  // Whether this element is on the GC queue
    std::atomic<history_type*> prev_;
    std::atomic<MvStatus> status_;  // Status of this element
    std::atomic<type> rtid_;  // Read TID
    type wtid_;  // Write TID
    void *index_ptr;
    void (*delete_cb) (void*, void*, void*);
    void *delete_param;

    friend class MvObject<T>;
};

template <typename T>
class MvObject {
public:
    typedef MvHistory<T> history_type;
    typedef const T& read_type;
    typedef TransactionTid::type type;

    // How many consecutive DELTA versions will be allowed before flattening
    static constexpr uint64_t gc_flattening_length = 257;

#if MVCC_INLINING
    MvObject() : h_(&ih_), ih_(this) {
        if (std::is_trivial<T>::value) {
            ih_.v_ = T();
            ih_.status_delete();
        } else {
            ih_.status_delete();
        }
        ih_.status_commit();
    }
    explicit MvObject(const T& value)
            : h_(&ih_), ih_(0, this, value) {
        ih_.status_commit();
    }
    explicit MvObject(T&& value)
            : h_(&ih_), ih_(0, this, value) {
        ih_.status_commit();
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(&ih_), ih_(0, this, T(std::forward<Args>(args)...)) {
        ih_.status_commit();
    }
#else
    MvObject() : h_(new history_type(this)) {
        if (std::is_trivial<T>::value) {
            h_.load()->v_ = T();
            h_.load()->status_delete();
        } else {
            h_.load()->status_delete();
        }
        h_.load()->status_commit();
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

    // Aborts currently-pending head version; returns true if the head version
    // is pending and false otherwise.
    bool abort(history_type* h) {
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
    bool cp_check(const type tid, history_type* h /* read item */) {
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
    bool cp_install(history_type* h) {
        if (!h->status_is(MvStatus::PENDING)) {
            return false;
        }

        h->status_commit();

        if (h->status_is(COMMITTED_DELTA, COMMITTED)) {
            // Put predecessors in the RCU list, but only if they aren't already
            history_type* prev = h->prev();
            bool stop = prev->is_gc_enqueued();
            do {
                stop = prev->status_is(COMMITTED_DELTA, COMMITTED) ||
                       prev->is_gc_enqueued();
                history_type* ele = prev;
                prev = prev->prev();
                ele->gc_push(is_inlined(ele));
            } while (prev != nullptr && !stop);

            // And for DELETED versions, enqueue the callback
            if (h->status_is(COMMITTED_DELETED)) {
                h->enqueue_for_delete();
            }
        } else if (h->status_is(COMMITTED_DELTA)) {
            // Counter update from nearest COMMITTED version
            history_type* prev = h->prev();
            while (!prev->status_is(COMMITTED)) {
                prev = prev->prev();
            }
            if (!(delta_counter++ % gc_flattening_length)) {
                h->enqueue_for_flattening();
            }
        }

        return true;
    }

    // "Lock" step: pending version installation & version consistency check;
    //              returns true if successful, false if aborted
    bool cp_lock(const type tid, history_type* h) {
        // Can only install pending versions
        if (!h->status_is(MvStatus::PENDING)) {
            return false;
        }

        // The previously-visible version for h
        history_type* hvis = h->prev();

        do {
            // Can only install onto the latest-visible version
            if (hvis && hvis != find(tid)) {
                return false;
            }

            // Discover target atomic on which to do CAS
            std::atomic<history_type*>* target = &h_;
            history_type* t = *target;
            history_type* next = nullptr;
            while (t->wtid() > tid) {
                target = &t->prev_;
                next = t;
                t = *target;
            }

            // Properly link h's prev_
            h->prev_.store(t, std::memory_order_relaxed);

            // Attempt to CAS onto the target
            if (target->compare_exchange_strong(t, h)) {
                if (next && next->is_gc_enqueued()) {
                    history_type* v = h;
                    while (v && !v->is_gc_enqueued()) {
                        if (!v->gc_push(is_inlined(v))) {
                            break;
                        }
                        v = v->prev();
                    }
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
    // !!! IMPORTANT !!!
    // This function should only be used to free history nodes that have NOT been
    // hooked into the version chain
    void delete_history(history_type* h) {
#if MVCC_INLINING
        if (&ih_ == h) {
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
        history_type* h = head();

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

    history_type* head() const {
        return h_.load(std::memory_order_acquire);
    }

    inline bool is_head(history_type*  const h) const {
        return h == h_;
    }

    // Returns whether the given history element is the inlined version
    inline bool is_inlined(history_type*  const h) const {
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
            return &ih_;
        }
#endif
        return new history_type(std::forward<Args>(args)...);
    }

    // Read-only
    const T& nontrans_access() const {
        history_type* h = h_;
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
        history_type* h = h_;
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
    // Spin-wait on interested item
    void wait_if_pending(const history_type* h) const {
        while (h->status_is(MvStatus::PENDING)) {
            // TODO: implement a backoff or something
            relax_fence();
        }
    }

    std::atomic<uint64_t> delta_counter;  // For gc-time flattening
    std::atomic<history_type*> h_;

#if MVCC_INLINING
    history_type ih_;  // Inlined version
#endif

    friend class MvHistory<T>;
};

#if SAFE_FLATTEN
#include "Transaction.hh"
#include "MVCCStructs.hh"
template <typename T>
T* MvHistory<T>::vp_safe_flatten() {
    if (status_is(DELTA)) {
        if (wtid_ > Sto::write_tid_inf())
            return nullptr;
        enflatten();
    }
    return &v_;
}
#endif
