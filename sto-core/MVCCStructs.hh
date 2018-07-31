#pragma once

// History list item for MVCC

template <typename T, bool Trivial = std::is_trivially_copyable<T>::value> struct MvHistory;

struct MvHistoryBase {
protected:
    MvHistoryBase(TransactionTid::type ntid, MvHistoryBase *nprev = nullptr)
            : rtid(ntid), wtid(ntid), prev_(nprev), next_(nullptr) {
#if 0
        // TODO: work on retroactive writes
        while (prev_ && (wtid < prev_->wtid)) {
            next_ = prev_;
            prev_ = prev_->prev_;
        }
#endif

        if (prev_) {
            always_assert(
                prev_->rtid <= wtid,
                "Cannot write MVCC history with wtid earlier than prev rtid.");
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

public:
    bool check_version() const {
        if (!next_) {
            return true;
        }

        const TransactionTid::type commit = Sto::commit_tid();

        // Check succeeds only if the commit time is earlier than the next write
        return commit < next_->wtid;
    }

    TransactionTid::type rtid;  // Read TID
    TransactionTid::type wtid;  // Write TID

protected:
    MvHistoryBase *prev_;
    MvHistoryBase *next_;
};

template <typename T>
struct MvHistory<T, true /* trivial */> : MvHistoryBase {
public:
    MvHistory(TransactionTid::type ntid, T nv, MvHistory<T, true> *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), v_(nv) {
    }

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

private:
    T v_;
};

template <typename T>
struct MvHistory<T, false /* !trivial */> : MvHistoryBase {
public:
    MvHistory(TransactionTid::type ntid, T *nvp, MvHistory<T, false> *nprev = nullptr)
            : MvHistoryBase(ntid, nprev), vp_(nvp) {
    }

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

private:
    T *vp_;
};
