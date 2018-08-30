#pragma once

// History list item for MVCC
template <typename T, bool Trivial = std::is_trivially_copyable<T>::value> class MvHistory;

// Status types of MvHistory elements
enum MvStatus {
    ABORTED = 0x000,
    DELETED = 0x001,  // Not a valid state on its own, but defined as a flag
    PENDING = 0x010,
    COMMITTED = 0x100,
    PENDING_DELETED = 0x011,
    COMMITTED_DELETED = 0x101,
};

// Generic contained for MVCC abstractions applied to a given object
template <typename T>
class MvObject {
public:
    typedef MvHistory<T> history_type;
    typedef const T& read_type;
    typedef TransactionTid::type type;

    MvObject() :
        h_(new history_type(0, T())) {}
    explicit MvObject(const T& value) :
        h_(new history_type(0, new T(value))) {}
    explicit MvObject(T&& value) :
        h_(new history_type(0, new T(std::move(value)))) {}
    template <typename... Args>
    explicit MvObject(Args&&... args) :
        h_(new history_type(0, new T(std::forward<Args>(args)...))) {}

    ~MvObject() {
        while (h_) {
            history_type *prev = h_->prev();
            delete h_;
            h_ = prev;
        }
    }

    // Aborts currently-pending head version; returns true if the head version
    // is pending and false otherwise.
    bool abort() {
        if (!h_->status_is(MvStatus::PENDING)) {
            return false;
        }

        h_->status_abort();
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
    bool cp_install() {
        if (!h_->status_is(MvStatus::PENDING)) {
            return false;
        }

        h_->status_commit();
        return true;
    }

    // "Lock" step: pending version installation & version consistency check;
    //              returns true if successful, false if aborted
    bool cp_lock(const type tid, history_type *h) {
        // Can only install pending versions
        if (!h->status_is(MvStatus::PENDING)) {
            return false;
        }

        // Can only install onto the latest-visible version
        if (h->prev() != h_) {
            return false;
        }

        // Attempt to CAS onto the head of the version list
        if (!::bool_cmpxchg(&h_, h->prev(), h)) {
            return false;
        }

        // Version consistency verification
        if (h->prev()->rtid() > tid) {
            return false;
        }

        return true;
    }

private:
    // Finds the current visible version, based on tid
    history_type* find(const type tid) const {
        history_type *h = h_;
        /* TODO: use something smarter than a linear scan */
        while (h) {
            while (h->status_is(MvStatus::PENDING)) {
                wait();
            }
            if (h->wtid() <= tid && h->status_is(MvStatus::COMMITTED)) {
                break;
            }
            h = h->prev();
        }

        return h;
    }

    // Spin-wait on interested item
    void wait() const {
        // TODO: implement wait
    }

    history_type *h_;
};

class MvHistoryBase {
public:
    typedef TransactionTid::type type;

    // Attempt to set the pointer to prev; returns true on success
    bool prev(MvHistoryBase *prev) {
        if (is_valid_prev(prev)) {
            prev_ = prev;
            return true;
        }
        return false;
    }

    // Returns the current rtid
    type rtid() const {
        return rtid_;
    }

    // Non-CAS assignment on the rtid
    type rtid(const type new_rtid) {
        return rtid_ = new_rtid;
    }

    // Proxy for a CAS assigment on the rtid
    type rtid(const type prev_rtid, const type new_rtid) {
        return ::cmpxchg(&rtid_, prev_rtid, new_rtid);
    }

    // Returns the status
    MvStatus status() const {
        return status_;
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

    MvHistoryBase *prev_;

private:
    // Returns true if the given prev pointer would be a valid prev element
    bool is_valid_prev(const MvHistoryBase *prev) const {
        return prev->rtid_ <= wtid_;
    }

    type rtid_;  // Read TID
    MvStatus status_;  // Status of this element
    type wtid_;  // Write TID
};

template <typename T>
class MvHistory<T, true /* trivial */> : public MvHistoryBase {
public:
    typedef MvHistory<T, true> history_type;

    MvHistory() : MvHistory(0, T()) {}
    explicit MvHistory(type ntid, T nv, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), v_(nv), vp_(&v_) {}
    explicit MvHistory(type ntid, T *nvp, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), v_(*nvp), vp_(&v_) {}

    const T& v() const {
        return v_;
    }

    T& v() {
        return v_;
    }

    T* vp() {
        return vp_;
    }

    T** vpp() {
        return &vp_;
    }

    history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_);
    }

private:
    T v_;
    T *vp_;
};

template <typename T>
class MvHistory<T, false /* !trivial */> : public MvHistoryBase {
public:
    typedef MvHistory<T, false> history_type;

    MvHistory() : MvHistory(0, nullptr) {}
    explicit MvHistory(type ntid, T& nv, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), vp_(&nv) {}
    explicit MvHistory(type ntid, T *nvp, history_type *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), vp_(nvp) {}

    const T& v() const {
        return *vp_;
    }

    T& v() {
        return *vp_;
    }

    T* vp() {
        return vp_;
    }

    T** vpp() {
        return &vp_;
    }

    history_type* prev() const {
        return reinterpret_cast<history_type*>(prev_);
    }

private:
    T *vp_;
};
