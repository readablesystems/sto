// Contains core data structures for MVCC

#pragma once

#include "MVCCCommutators.hh"
#include "MVCCRegistry.hh"
#include "MVCCTypes.hh"

// Status types of MvHistory elements
enum MvStatus {
    UNUSED              = 0b000000,
    ABORTED             = 0b100000,
    DELTA               = 0b001000,  // Commutative update delta
    DELETED             = 0b000001,  // Not a valid state on its own, but defined as a flag
    PENDING             = 0b000010,
    COMMITTED           = 0b000100,
    PENDING_DELTA       = 0b001010,
    COMMITTED_DELTA     = 0b001100,
    PENDING_DELETED     = 0b000011,
    COMMITTED_DELETED   = 0b000101,
    LOCKED              = 0b010000,
    LOCKED_DELTA        = 0b011000,  // Converting from delta to flattened
};


class MvHistoryBase {
public:
    typedef TransactionTid::type type;

    virtual ~MvHistoryBase() {}

    // Attempt to set the pointer to prev; returns true on success
    bool prev(MvHistoryBase *prev) {
        if (is_valid_prev(prev)) {
            prev_.store(prev);
            return true;
        }
        return false;
    }

    // Returns the current rtid
    type rtid() const {
        return rtid_.load();
    }

    // Non-CAS assignment on the rtid
    type rtid(const type new_rtid) {
        rtid_.store(new_rtid);
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
        return status_.load();
    }

    // Sets and returns the status
    // NOT THREADSAFE
    MvStatus status(MvStatus s) {
        status_.store(s);
        return status_.load();
    }
    MvStatus status(unsigned long long s) {
        return status((MvStatus)s);
    }

    // Removes all flags, signaling an aborted status
    // NOT THREADSAFE
    MvStatus status_abort() {
        status(MvStatus::ABORTED);
        return status();
    }

    // Sets the committed flag and unsets the pending flag; does nothing if the
    // current status is ABORTED
    // NOT THREADSAFE
    MvStatus status_commit() {
        if (status() == MvStatus::ABORTED) {
            return status();
        }
        status(MvStatus::COMMITTED | (status() & ~MvStatus::PENDING));
        return status();
    }

    // Sets the deleted flag; does nothing if the current status is ABORTED
    // NOT THREADSAFE
    MvStatus status_delete() {
        if (status() == MvStatus::ABORTED) {
            return status();
        }
        status(status() | MvStatus::DELETED);
        return status();
    }

    // Sets the delta flag; does nothing if the current status is ABORTED
    // NOT THREADSAFE
    MvStatus status_delta() {
        if (status() == MvStatus::ABORTED) {
            return status();
        }
        status(status() | MvStatus::DELTA);
        return status();
    }

    // Sets the history into UNUSED state
    // NOT THREADSAFE
    MvStatus status_unused() {
        status(MvStatus::UNUSED);
        return status();
    }

    // Returns true if all the given flag(s) are set
    bool status_is(const MvStatus status) const {
        return (status_.load() & status) == status;
    }

    // Returns the current wtid
    type wtid() const {
        return wtid_;
    }

protected:
    MvHistoryBase(type ntid, MvHistoryBase *nprev = nullptr)
            : prev_(nprev), status_(MvStatus::PENDING), rtid_(ntid),
              wtid_(ntid), inlined_(false) {

        /*
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
        */
    }

    std::atomic<MvHistoryBase*> prev_;
    std::atomic<MvStatus> status_;  // Status of this element

private:
    // Returns true if the given prev pointer would be a valid prev element
    bool is_valid_prev(const MvHistoryBase *prev) const {
        return prev->rtid_.load() <= wtid_;
    }

    std::atomic<type> rtid_;  // Read TID
    type wtid_;  // Write TID
    bool inlined_;  // Inlined history?

    template <typename T>
    friend class MvObject;
    friend class MvRegistry;
};

template <typename T>
class MvHistory : public MvHistoryBase {
public:
    typedef MvHistory<T> history_type;
    typedef MvObject<T> object_type;
    typedef commutators::MvCommutator<T> comm_type;

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
    void enflatten() {
        T v;
        prev()->flatten(v);
        v = c_.operate(v);
        MvStatus expected = MvStatus::COMMITTED_DELTA;
        if (status_.compare_exchange_strong(expected, MvStatus::LOCKED_DELTA)) {
            v_ = v;
            status(MvStatus::COMMITTED);
        } else {
            // TODO: exponential backoff?
            while (status_is(MvStatus::LOCKED_DELTA));
        }
    }

    // Returns the flattened view of the current type
    void flatten(T &v) {
        MvStatus s = status();
        if (s == COMMITTED) {
            v = v_;
            return;
        }
        prev()->flatten(v);
        if ((s = status()) == COMMITTED) {
            v = v_;
        } else if ((s & DELTA) == DELTA) {
            v = c_.operate(v);
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

    MvObject()
            : h_(&ih_), ih_(this), rentry_(MvRegistry::reg(this)) {
        ih_.inlined_ = true;
        ih_.status_commit();
        if (std::is_trivial<T>::value) {
            ih_.v_ = T();
        } else {
            ih_.status_delete();
        }
    }
    explicit MvObject(const T& value)
            : h_(&ih_), ih_(0, this, new T(value),
              rentry_(MvRegistry::reg(this))) {
        ih_.inlined_ = true;
        ih_.status_commit();
        rentry_ = MvRegistry::reg(this);
    }
    explicit MvObject(T&& value)
            : h_(&ih_), ih_(0, this, new T(std::move(value))),
              rentry_(MvRegistry::reg(this)) {
        ih_.inlined_ = true;
        ih_.status_commit();
        rentry_ = MvRegistry::reg(this);
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(&ih_), ih_(0, this, new T(std::forward<Args>(args)...)),
              rentry_(MvRegistry::reg(this)) {
        ih_.inlined_ = true;
        ih_.status_commit();
        rentry_ = MvRegistry::reg(this);
    }

    ~MvObject() {
        rentry_->valid.store(false);
        base_type *h = h_.load();
        h_.store(nullptr);
        while (h) {
            if (&ih_ == h->prev_.load()) {
                h->prev_.store(ih_.prev_.load());
                ih_.status_unused();
            }
            h->rtid_.store(h->wtid_ = 0);  // Make it GC-able
            h = h->prev_.load();
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
            while (target->load()->wtid() > tid) {
                target = &target->load()->prev_;
            }

            // Properly link h's prev_
            auto target_expected = target->load();
            h->prev_.store(target_expected);

            // Attempt to CAS onto the target
            if (target->compare_exchange_strong(target_expected, h)) {
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
        if (&ih_ == h) {
            ih_.status_unused();
        } else {
            delete h;
        }
    }

    // Finds the current visible version, based on tid; by default, waits on
    // pending versions, but if toggled off, will simply return first version,
    // regardless of status
    history_type* find(const type tid, const bool wait=true) const {
        fence();
        history_type *h = static_cast<history_type*>(h_.load());
        /* TODO: use something smarter than a linear scan */
        while (h) {
            if (wait) {
                if (h->wtid() < tid) {
                    wait_if_pending(h);
                }
                if (h->wtid() <= tid && h->status_is(MvStatus::COMMITTED)) {
                    break;
                }
            } else {
                if (h->wtid() <= tid && h->status_is(MvStatus::COMMITTED)) {
                    break;
                }
            }
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
        auto status = ih_.status();
        if (status == UNUSED && ih_.status_.compare_exchange_strong(status, PENDING)) {
            // Use inlined history element
            history_type h(std::forward<Args>(args)...);
            ih_.prev_.store(h.prev_.load());
            ih_.rtid_.store(h.rtid_.load());
            ih_.wtid_ = h.wtid_;
            ih_.v_ = h.v_;
            ih_.c_ = h.c_;
            return &ih_;
        }
        return new history_type(std::forward<Args>(args)...);
    }

    // Read-only
    const T& nontrans_access() const {
        history_type *h = static_cast<history_type*>(h_.load());
        /* TODO: head version caching */
        while (h) {
            if (h->status_is(COMMITTED)) {
                return h->v();
            }
            h = h->prev();
        }
        // Should never get here!
        throw InvalidState();
    }
    // Writable version
    T& nontrans_access() {
        history_type *h = static_cast<history_type*>(h_.load());
        /* TODO: head version caching */
        while (h) {
            if (h->status_is(COMMITTED)) {
                h->status(MvStatus::COMMITTED);
                return h->v();
            }
            h = h->prev();
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
    history_type ih_;  // Inlined version
    MvRegistry::MvRegistryEntry *rentry_;  // The corresponding registry entry

    friend class MvRegistry;
};
