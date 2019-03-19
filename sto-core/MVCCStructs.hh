// Contains core data structures for MVCC

#pragma once

#include <stack>

#include "MVCCRegistry.hh"
#include "MVCCTypes.hh"

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

    virtual ~MvHistoryBase() {}

    // Sets the pointer to next
    void next(MvHistoryBase *next) {
        next_ = next;
    }

    // Attempt to set the pointer to prev; returns true on success
    bool prev(MvHistoryBase *prev) {
        if (is_valid_prev(prev)) {
            prev_ = prev;
            return true;
        }
        return false;
    }

    // Returns the current btid
    type btid() const {
        return btid_;
    }

    // Non-CAS assignment on the btid
    type btid(const type new_btid) {
        btid_ = new_btid;
        return btid();
    }

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
        if (status == 0) {  // Special case
            return status_ == 0;
        }
        return (status_ & status) == status;
    }

    // Returns the current wtid
    type wtid() const {
        return wtid_;
    }

protected:
    MvHistoryBase(type ntid, MvHistoryBase *nprev = nullptr)
            : next_(nullptr), prev_(nprev), status_(PENDING), rtid_(ntid),
              wtid_(ntid), btid_(ntid) {

        // Can't actually assume this assertion because it may have changed
        // between when the history item was made and when this check is done
        if (prev_) {
//            char buf[1000];
//            snprintf(buf, 1000,
//                "Thread %d: Cannot write MVCC history with wtid (%lu) earlier than prev (%p) rtid (%lu)",
//                Sto::transaction()->threadid(), rtid, prev_, prev_->rtid);
//            always_assert(prev_->rtid <= wtid, buf);
            always_assert(
                is_valid_prev(prev_),
                "Cannot write MVCC history with wtid earlier than prev rtid.");
//            prev_->rtid = rtid;
        }
    }

    // Updates btid to Transaction::_RTID
    void update_btid();

    std::atomic<MvHistoryBase*> next_;  // Some next element, with a newer tid;
                                        // used for GC optimizations
    std::atomic<MvHistoryBase*> prev_;  // Used to build the history chain
    std::atomic<MvStatus> status_;  // Status of this element

    virtual void enflatten() {}

private:
    // Returns true if the given prev pointer would be a valid prev element
    bool is_valid_prev(const MvHistoryBase *prev) const {
        return prev->wtid_ <= wtid_;
    }

    // Returns the newest element that satisfies the expectation with the given
    // mask. Optionally increments a counter.
    MvHistoryBase* with(const MvStatus mask, const MvStatus expect,
                        size_t * const visited=nullptr) const {
        MvHistoryBase *ele = (MvHistoryBase*)this;
        while (ele && ((ele->status() & mask) != expect)) {
            ele = ele->prev_;
            if (visited) {
                (*visited)++;
            }
        }
        return ele;
    }

    std::atomic<type> rtid_;  // Read TID
    type wtid_;  // Write TID
    std::atomic<type> btid_;  // Base-version TID

    template <typename>
    friend class MvObject;
    friend class MvRegistry;
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
            : MvHistoryBase(ntid, nprev), obj_(obj), v_(nv) {}
    explicit MvHistory(
            type ntid, object_type *obj, T&& nv, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), obj_(obj), v_(std::move(nv)) {}
    explicit MvHistory(
            type ntid, object_type *obj, T *nvp, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), obj_(obj) {
        if (nvp) {
            v_ = *nvp;
        }
    }
    explicit MvHistory(
            type ntid, object_type *obj, comm_type &&c, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), obj_(obj), c_(c), v_() {
        status_delta();
    }

    ~MvHistory() override {}

    // Retrieve the object for which this history element is intended
    object_type* object() const {
        return obj_;
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

    history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_.load());
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
            update_btid();
            status(COMMITTED);
        } else {
            // TODO: exponential backoff?
            while (status_is(LOCKED_COMMITTED_DELTA));
        }
    }

    // Returns the flattened view of the current type
    //void flatten(T &v) {
    //    MvStatus s = status();
    //    if (s == COMMITTED) {
    //        v = v_;
    //        return;
    //    }
    //    prev()->flatten(v);
    //    if ((s = status()) == COMMITTED) {
    //        v = v_;
    //    } else if ((s & DELTA) == DELTA) {
    //        v = c_.operate(v);
    //    }
    //}

    void flatten(T &v) {
        std::stack<history_type*> trace;
        history_type* curr = this;
        trace.push(curr);
        while (curr->status() != COMMITTED) {
            curr = curr->prev();
            trace.push(curr);
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

    friend class MvObject<T>;
};

template <typename T>
class MvObject {
public:
    typedef MvHistory<T> history_type;
    typedef const T& read_type;
    typedef TransactionTid::type type;

#if MVCC_INLINING
    MvObject()
            : h_(&ih_), ih_(this), enqueued_(false) {
        ih_.status_commit();
        itid_ = ih_.rtid();
        if (std::is_trivial<T>::value) {
            ih_.v_ = T();
        } else {
            ih_.status_delete();
        }
    }
    explicit MvObject(const T& value)
            : h_(&ih_), ih_(0, this, value), enqueued_(false) {
        ih_.status_commit();
        itid_ = ih_.rtid();
    }
    explicit MvObject(T&& value)
            : h_(&ih_), ih_(0, this, value), enqueued_(false) {
        ih_.status_commit();
        itid_ = ih_.rtid();
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(&ih_), ih_(0, this, T(std::forward<Args>(args)...)), enqueued_(false) {
        ih_.status_commit();
        itid_ = ih_.rtid();
    }
#else
    MvObject() : h_(new history_type(this)), enqueued_(false) {
        h_.load()->status_commit();
        if (std::is_trivial<T>::value) {
            static_cast<history_type*>(h_.load())->v_ = T();
        } else {
            h_.load()->status_delete();
        }
    }
    explicit MvObject(const T& value)
            : h_(new history_type(0, this, value)), enqueued_(false) {
        h_.load()->status_commit();
    }
    explicit MvObject(T&& value)
            : h_(new history_type(0, this, value)), enqueued_(false) {
        h_.load()->status_commit();
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(new history_type(0, this, T(std::forward<Args>(args)...))), enqueued_(false) {
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

            // Properly link h's prev_
            h->prev_ = t;

            // Attempt to CAS onto the target
            if (target->compare_exchange_strong(t, h)) {
                bool false_ = false;
                if (enqueued_.compare_exchange_strong(false_, true)) {
                    MvRegistry::reg(this, h->wtid(), &enqueued_);
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
            h = static_cast<history_type*>(h_.load());
        }
#else
        h = static_cast<history_type*>(h_.load());
#endif
        base_type * const head = h;
        base_type *next = nullptr;
        /* TODO: use something smarter than a linear scan */
        while (h) {
            auto s = h->status();
            assert(s & (PENDING | ABORTED | COMMITTED));
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
            next = h;
            h = h->prev();
        }

        assert(h);

        return h;
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
    std::atomic<bool> enqueued_;  // Whether this has been enqueued for GC

    friend class MvRegistry;
};
