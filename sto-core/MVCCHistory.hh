#pragma once

// History list item for MVCC

template <typename T, bool Trivial> struct MvHistory;

template <typename T>
struct MvHistory<T, true /* trivial */> {
public:
    MvHistory(TransactionTid::type ntid, T nv, MvHistory<T, true> *nprev = nullptr)
        : tid(ntid), v_(nv), prev(nprev), next(nullptr) {}

    T& v() const {
        return v_;
    }

    T* vp() const {
        return &v_;
    }

    bool check_version() const {
        if (!next) {
            return true;
        }

        const TransactionTid::type commit = Sto::commit_tid();

        // Check succeeds only if the commit time is earlier than the next write
        return commit < next->tid;
    }

    TransactionTid::type tid;
    MvHistory<T, true> *prev;
    MvHistory<T, true> *next;

private:
    T v_;
};

template <typename T>
struct MvHistory<T, false /* !trivial */> {
public:
    MvHistory(TransactionTid::type ntid, T *nvp, MvHistory<T, false> *nprev = nullptr)
        : tid(ntid), vp_(nvp), prev(nprev), next(nullptr) {}

    T& v() const {
        return *vp_;
    }

    T* vp() const {
        return vp_;
    }

    bool check_version() const {
        if (!next) {
            return true;
        }

        const TransactionTid::type commit = Sto::commit_tid();

        // Check succeeds only if the commit time is earlier than the next write
        return commit < next->tid;
    }

    TransactionTid::type tid;
    MvHistory<T, false> *prev;
    MvHistory<T, false> *next;

private:
    T *vp_;
};
