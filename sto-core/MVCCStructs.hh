// Contains core data structures for MVCC

#pragma once

#include <deque>
#include <stack>
#include <thread>

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
struct MvGcLocation {
    MvObject<T>* obj;
    MvHistory<T>* h;
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
    explicit MvHistory(object_type *obj) : MvHistory(obj, 0, nullptr) {}
    explicit MvHistory(
            object_type *obj, type ntid, const T& nv)
            : obj_(obj), v_(nv), gc_enqueued_(false), prev_(nullptr),
              status_(PENDING), rtid_(ntid), wtid_(ntid), delete_cb(nullptr) {
    }
    explicit MvHistory(
            object_type *obj, type ntid, T&& nv)
            : obj_(obj), v_(std::move(nv)), gc_enqueued_(false), prev_(nullptr),
              status_(PENDING), rtid_(ntid), wtid_(ntid), delete_cb(nullptr) {
    }
    explicit MvHistory(
            object_type *obj, type ntid, T *nvp)
            : obj_(obj), v_(nvp ? *nvp : v_), gc_enqueued_(false), prev_(nullptr),
              status_(PENDING), rtid_(ntid), wtid_(ntid), delete_cb(nullptr) {
    }
    explicit MvHistory(
            object_type *obj, type ntid, comm_type &&c)
            : obj_(obj), c_(std::move(c)), v_(), gc_enqueued_(false), prev_(nullptr),
              status_(PENDING_DELTA), rtid_(ntid), wtid_(ntid), delete_cb(nullptr) {
    }

    // Whether this version can be late-inserted in front of hnext
    bool can_precede(const history_type* hnext) {
        if (this->status_is(DELETED) && hnext->status_is(DELTA)) {
            return false;
        }
        return true;
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

    inline void prepare_gc() {
        auto location = new MvGcLocation<T>();
        location->obj = object();
        location->h = this;
        Transaction::rcu_call(gc_delete_cb, location);
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

private:
    inline void update_rtid(type minimum_new_rtid) {
        type prev = rtid_.load(std::memory_order_relaxed);
        while (prev < minimum_new_rtid
               && !rtid_.compare_exchange_weak(prev, minimum_new_rtid,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
        }
    }

public:
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
    inline void status(MvStatus s) {
        status_.store(s, std::memory_order_release);
    }
    inline void status(unsigned long long s) {
        status((MvStatus)s);
    }

    // Removes all flags, signaling an aborted status
    // NOT THREADSAFE
    inline void status_abort() {
        status(ABORTED);
    }

    // Sets the committed flag and unsets the pending flag; does nothing if the
    // current status is ABORTED
    // NOT THREADSAFE
    inline void status_commit() {
        int s = status();
        assert(!(s & ABORTED));
        status(COMMITTED | (s & ~PENDING));
    }

    // Sets the deleted flag; does nothing if the current status is ABORTED
    // NOT THREADSAFE
    inline void status_delete() {
        int s = status();
        assert(!(s & ABORTED));
        status(DELETED | s);
    }

    // Sets the history into UNUSED state
    // NOT THREADSAFE
    inline void status_unused() {
        status(UNUSED);
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
                if (hd->delete_cb) {
                    hd->delete_cb(hd->index_ptr, hd->delete_param, hd);
                }
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
        assert(!status_is(ABORTED));
        if (status_is(COMMITTED_DELTA)) {
            if (!status_is(LOCKED)) {
                flatten();
            } else {
                TXP_INCREMENT(txp_mvcc_flat_spins);
            }
            while (!status_is(COMMITTED_DELTA, COMMITTED));
        }
    }

    // To be called from the source of the flattening.
    void flatten() {
        // Current element is the one initiating the flattening here. It is not
        // included in the trace, but it is included in the committed trace.
        history_type* curr = this;
        std::stack<history_type*> trace;  // Of history elements to process

        while (!curr->status_is(COMMITTED) || curr->status_is(DELTA)) {
            trace.push(curr);
            curr = curr->prev();
        }

        TXP_INCREMENT(txp_mvcc_flat_versions);
        T value {curr->v_};
        while (!trace.empty()) {
            auto hnext = trace.top();

            while (true) {
                auto prev = hnext->prev();
                if (prev == curr) {
                    break;
                }
                trace.push(prev);
                hnext = prev;
            }

            obj_->wait_if_pending(hnext);

            if (hnext->status_is(COMMITTED)) {
                hnext->update_rtid(this->wtid());
                assert(!hnext->status_is(COMMITTED_DELETED));
                if (hnext->status_is(DELTA)) {
                    hnext->c_.operate(value);
                } else {
                    value = hnext->v_;
                }
            }

            trace.pop();
            curr = hnext;
        }

        auto expected = COMMITTED_DELTA;
        if (status_.compare_exchange_strong(expected, LOCKED_COMMITTED_DELTA)) {
            v_ = value;
            TXP_INCREMENT(txp_mvcc_flat_commits);
            status(COMMITTED);
            prepare_gc();
        } else {
            TXP_INCREMENT(txp_mvcc_flat_spins);
        }
    }

    static void gc_delete_cb(void *ptr) {
        auto location = static_cast<MvGcLocation<T>*>(ptr);
        auto obj = location->obj;
        auto h = location->h;

        // Put predecessors in the RCU list, but only if they aren't already
        history_type* prev = h->prev();
        bool stop = prev->is_gc_enqueued();
        do {
            stop = prev->status_is(COMMITTED_DELTA, COMMITTED) ||
                   prev->is_gc_enqueued();
            history_type* ele = prev;
            prev = prev->prev();
            ele->gc_push(obj->is_inlined(ele));
        } while (prev != nullptr && !stop);

        delete location;
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
            h_.load()->v_ = T();
        }
        h_.load()->status(COMMITTED_DELETED);
    }
    explicit MvObject(const T& value)
            : h_(new history_type(this, 0, value)) {
        h_.load()->status(COMMITTED);
    }
    explicit MvObject(T&& value)
            : h_(new history_type(this, 0, std::move(value))) {
        h_.load()->status(COMMITTED);
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(new history_type(this, 0, T(std::forward<Args>(args)...))) {
        h_.load()->status(COMMITTED);
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
    bool cp_check(const type tid, history_type* hr) {
        // rtid update
        hr->update_rtid(tid);

        // Read version consistency check
        for (history_type* h = h_.load(); h != hr; h = h->prev()) {
            if (!h->status_is(ABORTED) && h->wtid() < tid) {
                return false;
            }
        }

        return true;
    }

    // "Install" step: set status to committed
    void cp_install(history_type* h) {
        int s = h->status();
        assert((s & (MvStatus::PENDING | MvStatus::ABORTED)) == MvStatus::PENDING);
        h->status((s & ~MvStatus::PENDING) | MvStatus::COMMITTED);
        if (!(s & DELTA)) {
            h->prepare_gc();

            // And for DELETED versions, enqueue the callback
            if (s & DELETED) {
                h->enqueue_for_delete();
            }
        } else {
            if (!(delta_counter++ % gc_flattening_length)) {
                h->enqueue_for_flattening();
            }
        }
    }

    // "Lock" step: pending version installation & version consistency check;
    //              returns true if successful, false if aborted
    bool cp_lock(const type tid, history_type* hw) {
        // Can only install pending versions
        if (!hw->status_is(PENDING)) {
            TXP_INCREMENT(txp_mvcc_lock_status_aborts);
            return false;
        }

        std::atomic<history_type*>* target = &h_;
        while (true) {
            // Discover target atomic on which to do CAS
            history_type* t = *target;
            if (t->wtid() > tid) {
                if (t->status_is(DELTA) && !hw->can_precede(t)) {
                    return false;
                }
                target = &t->prev_;
            } else if (!t->status_is(ABORTED) && t->rtid() > tid) {
                return false;
            } else {
                // Properly link h's prev_
                hw->prev_.store(t, std::memory_order_relaxed);

                // Attempt to CAS onto the target
                if (target->compare_exchange_strong(t, hw)) {
                    break;
                }
            }
        }

        // Write version consistency check for CU enabling
        for (history_type* h = h_.load(); h != hw; h = h->prev()) {
            if (h->status_is(DELTA) && !hw->can_precede(h)) {
                hw->status_abort();
                return false;
            }
        }

        // Write version consistency check for concurrent reads + CU enabling
        for (history_type* h = hw->prev(); h; h = h->prev()) {
            if ((hw->status_is(DELTA) && !h->status_is(ABORTED) && !h->can_precede(hw))
                || (h->status_is(COMMITTED) && h->rtid() > tid)) {
                hw->status_abort();
                return false;
            }
            if (h->status_is(COMMITTED)) {
                break;
            }
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
            auto status = h->status();
            auto wtid = h->wtid();
            assert(status & (PENDING | ABORTED | COMMITTED));
            if (wait) {
                if (wtid < tid) {
                    wait_if_pending(h);
                    status = h->status();
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
            new (&ih_) history_type(this, std::forward<Args>(args)...);
            return &ih_;
        }
#endif
        return new(std::nothrow) history_type(this, std::forward<Args>(args)...);
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
