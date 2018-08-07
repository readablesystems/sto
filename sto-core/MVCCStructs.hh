#pragma once

// History list item for MVCC

template <typename T, bool Trivial = std::is_trivially_copyable<T>::value> struct MvHistory;

struct MvHistoryBase {
public:
    typedef TransactionTid::type type;

protected:
    MvHistoryBase(type ntid, MvHistoryBase *nprev = nullptr)
            : rtid(ntid), wtid(ntid), prev_(nprev), next_(nullptr) {
#if 0
        // TODO: work on retroactive writes
        while (prev_ && (wtid < prev_->wtid)) {
            next_ = prev_;
            prev_ = prev_->prev_;
        }
#endif

        if (prev_) {
            char buf[1000];
            snprintf(buf, 1000,
                "Thread %d: Cannot write MVCC history with wtid (%lu) earlier than prev (%p) rtid (%lu) and wtid(%lu)",
                Sto::transaction()->threadid(), wtid, prev_, prev_->rtid, prev_->wtid);
            always_assert(prev_->rtid <= wtid, buf);
//            always_assert(
//                prev_->rtid <= wtid,
//                "Cannot write MVCC history with wtid earlier than prev rtid.");
        }

#if 0
        // TODO: work on retroactive writes
        if (next_) {
            always_assert(
                rtid < next_->wtid,
                "Cannot write MVCC history with rtid later than next wtid.");
        }

        // TODO: deal with this nonatomic update of prev and next
        if (prev_) {
            while (::cmpxchg(&prev_->next_, next_, this) != next_);
        }

        if (next_) {
            while (::cmpxchg(&next_->prev_, prev_, this) != prev_);
        }
#endif
    }

    static MvHistoryBase* read_at(const MvHistoryBase *history, type tid) {
        MvHistoryBase *h = (MvHistoryBase*)history;
        /* TODO: use something smarter than a linear scan */
        for (;h->prev_; h = h->prev_) {
            if (h->wtid < tid) {
                break;
            }
        }

        type rtid_e;  // Expected rtid
        while (tid > (rtid_e = h->rtid)) {
            ::cmpxchg(&h->rtid, rtid_e, tid);
        }

        return h;
    }

public:
    bool check_version() const {
        const type commit = Sto::commit_tid();

        // Check succeeds only if the commit time is earlier than the next write
        return (!prev_ || commit >= prev_->rtid) && (!next_ || commit < next_->wtid);
    }

    type rtid;  // Read TID
    type wtid;  // Write TID

protected:
    MvHistoryBase *prev_;
    MvHistoryBase *next_;
};

template <typename T>
struct MvHistory<T, true /* trivial */> : MvHistoryBase {
public:
    MvHistory() : MvHistory(0, T()) {}
    MvHistory(type ntid, T nv, MvHistory<T, true> *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), v_(nv) {}
    MvHistory(type ntid, T *nvp, MvHistory<T, true> *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), v_(*nvp) {}

    const T& v() const {
        return v_;
    }

    T& v() {
        return v_;
    }

    T* vp() {
        return &v_;
    }

    MvHistory<T, true>* next() const {
        return reinterpret_cast<MvHistory<T, true>*>(next_);
    }

    MvHistory<T, true>* prev() const {
        return reinterpret_cast<MvHistory<T, true>*>(prev_);
    }

    MvHistory<T, true>* read_at(type tid) const {
        return reinterpret_cast<MvHistory<T, true>*>(MvHistoryBase::read_at(this, tid));
    }

private:
    T v_;
};

template <typename T>
struct MvHistory<T, false /* !trivial */> : MvHistoryBase {
public:
    MvHistory() : MvHistory(0, nullptr) {}
    MvHistory(type ntid, T& nv, MvHistory<T, false> *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), vp_(&nv) {}
    MvHistory(type ntid, T *nvp, MvHistory<T, false> *nprev = nullptr)
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

    MvHistory<T, false>* next() const {
        return reinterpret_cast<MvHistory<T, false>*>(next_);
    }

    MvHistory<T, false>* prev() const {
        return reinterpret_cast<MvHistory<T, false>*>(prev_);
    }

    MvHistory<T, false>* read_at(type tid) const {
        return reinterpret_cast<MvHistory<T, false>*>(MvHistoryBase::read_at(this, tid));
    }

private:
    T *vp_;
};
