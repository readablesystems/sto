// Contains core data structures for MVCC

#pragma once

#include "MVCCRegistry.hh"
#include "MVCCTypes.hh"

// Status types of MvHistory elements
enum MvStatus {
    UNUSED              = 0b0000,
    ABORTED             = 0b1000,
    DELETED             = 0b0001,  // Not a valid state on its own, but defined as a flag
    PENDING             = 0b0010,
    COMMITTED           = 0b0100,
    PENDING_DELETED     = 0b0011,
    COMMITTED_DELETED   = 0b0101,
};


class MvHistoryBase {
public:
    typedef TransactionTid::type type;

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
        return new_rtid;
    }

    // Proxy for a CAS assigment on the rtid
    type rtid(type prev_rtid, const type new_rtid) {
        assert(prev_rtid <= new_rtid);
        return rtid_.compare_exchange_strong(prev_rtid, new_rtid);
    }

    // Returns the status
    MvStatus status() const {
        return status_;
    }

    // Sets and returns the status
    MvStatus status(MvStatus status) {
        return status_ = status;
    }

    // Removes all flags, signaling an aborted status
    MvStatus status_abort() {
        return status_ = MvStatus::ABORTED;
    }

    // Sets the committed flag and unsets the pending flag; does nothing if the
    // current status is ABORTED
    MvStatus status_commit() {
        if (status_ == MvStatus::ABORTED) {
            return status_;
        }
        return status_ = static_cast<MvStatus>(MvStatus::COMMITTED | (status_ & ~MvStatus::PENDING));
    }

    // Sets the deleted flag; does nothing if the current status is ABORTED
    MvStatus status_delete() {
        if (status_ == MvStatus::ABORTED) {
            return status_;
        }
        return status_ = static_cast<MvStatus>(status_ | MvStatus::DELETED);
    }

    // Sets the history into UNUSED state
    MvStatus status_unused() {
        return status_ = MvStatus::UNUSED;
    }

    // Returns true if all the given flag(s) are set
    bool status_is(const MvStatus status) const {
        return (status_ & status) == status;
    }

    // Returns the current wtid
    type wtid() const {
        return wtid_;
    }

protected:
    MvHistoryBase(type ntid, MvHistoryBase *nprev = nullptr)
            : prev_(nprev), rtid_(ntid), status_(MvStatus::PENDING), wtid_(ntid) {

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

private:
    // Returns true if the given prev pointer would be a valid prev element
    bool is_valid_prev(const MvHistoryBase *prev) const {
        return prev->rtid_.load() <= wtid_;
    }

    std::atomic<type> rtid_;  // Read TID
    MvStatus status_;  // Status of this element
    const type wtid_;  // Write TID

    template <typename T>
    friend class MvObject;
    friend class MvRegistry;
};

template <typename T>
class MvHistory<T, true /* trivial */> : public MvHistoryBase {
public:
    typedef MvHistory<T, true> history_type;
    typedef MvObject<T> object_type;

    MvHistory() = delete;
    explicit MvHistory(object_type *obj) : MvHistory(0, obj, T()) {}
    explicit MvHistory(
            type ntid, object_type *obj, T nv,history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), obj_(obj), v_(nv), vp_(&v_) {}
    explicit MvHistory(
            type ntid, object_type *obj, T *nvp, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), obj_(obj), vp_(&v_) {
        if (nvp) {
            v_ = *nvp;
        }
    }

    // Retrieve the object for which this history element is intended
    object_type* object() const {
        return obj_;
    }

    const T& v() const {
        return v_;
    }

    T& v() {
        return v_;
    }

    T* vp() const {
        return vp_;
    }

    T** vpp() const {
        return &vp_;
    }

    history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_.load());
    }

private:
    object_type *obj_;  // Parent object
    T v_;
    T *vp_;
};

template <typename T>
class MvHistory<T, false /* !trivial */> : public MvHistoryBase {
public:
    typedef MvHistory<T, false> history_type;
    typedef MvObject<T> object_type;

    MvHistory() = delete;
    explicit MvHistory(object_type *obj) : MvHistory(0, obj, nullptr) {}
    explicit MvHistory(
            type ntid, object_type *obj, const T& nv, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), obj_(obj), v_(nv), vp_(&v_) {}
    explicit MvHistory(
            type ntid, object_type *obj, T&& nv, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), obj_(obj), v_(std::move(nv)), vp_(&v_) {}
    explicit MvHistory(
            type ntid, object_type *obj, T *nvp, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), obj_(obj), vp_(&v_) {
        if (nvp) {
            v_ = *nvp;
        }
    }

    // Retrieve the object for which this history element is intended
    object_type* object() const {
        return obj_;
    }

    const T& v() const {
        return v_;
    }

    T& v() {
        return v_;
    }

    T* vp() const {
        return vp_;
    }

    T** vpp() const {
        return &vp_;
    }

    history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_.load());
    }

private:
    object_type *obj_;  // Parent object
    T v_;
    T *vp_;
};

template <typename T>
class MvObject {
public:
    typedef MvHistory<T> history_type;
    typedef const T& read_type;
    typedef TransactionTid::type type;

    MvObject()
            : h_(new history_type(this)) {
        h_.load()->status_commit();
        h_.load()->status_delete();
        MvRegistry::reg(this);
    }
    explicit MvObject(const T& value)
            : h_(new history_type(0, this, new T(value))) {
        h_.load()->status_commit();
        MvRegistry::reg(this);
    }
    explicit MvObject(T&& value)
            : h_(new history_type(0, this, new T(std::move(value)))) {
        h_.load()->status_commit();
        MvRegistry::reg(this);
    }
    template <typename... Args>
    explicit MvObject(Args&&... args)
            : h_(new history_type(0, this, new T(std::forward<Args>(args)...))) {
        h_.load()->status_commit();
        MvRegistry::reg(this);
    }

    ~MvObject() {
        while (h_) {
            history_type *prev = static_cast<history_type*>(h_.load())->prev();
            delete h_.load();
            h_.store(prev);
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
            h->status_abort();
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
            if (hvis != find(tid)) {
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

    friend class MvRegistry;
};
