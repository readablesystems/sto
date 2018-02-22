#pragma once

#include "VersionBase.hh"

// Default STO/Silo OCC version (with opacity)
class TVersion : public BasicVersion<TVersion> {
public:
    TVersion() = default;
    explicit TVersion(type v)
            : BasicVersion(v) {}
    TVersion(type v, bool insert)
            : BasicVersion(v) {(void)insert;}

    bool cp_check_version_impl(Transaction& txn, TransItem& item) {
        (void)txn;
        assert(item.has_read());
        if (TransactionTid::is_locked(v_) && !item.has_write())
            return false;
        return check_version(item.read_value<TVersion>());
    }

    inline bool observe_read_impl(TransItem& item, bool add_read);

    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);
};

// STO/Silo OCC version without opacity
class TNonopaqueVersion : public BasicVersion<TNonopaqueVersion> {
public:
    TNonopaqueVersion()
            : BasicVersion<TNonopaqueVersion>(TransactionTid::nonopaque_bit) {}
    explicit TNonopaqueVersion(type v)
            : BasicVersion<TNonopaqueVersion>(v | TransactionTid::nonopaque_bit) {}
    TNonopaqueVersion(type v, bool insert)
            : BasicVersion<TNonopaqueVersion>(v | TransactionTid::nonopaque_bit) {(void)insert;};

    bool cp_check_version_impl(Transaction& txn, TransItem& item) {
        (void)txn;
        assert(item.has_read());
        if (TransactionTid::is_locked(v_) && !item.has_write())
            return false;
        return check_version(item.read_value<TNonopaqueVersion>());
    }

    inline bool observe_read_impl(TransItem& item, bool add_read);

    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);
};

// XXX not sure if it's really used anywhere
class TCommutativeVersion : BasicVersion<TCommutativeVersion> {
public:
    typedef TransactionTid::type type;
    // we don't store thread ids and instead use those bits as a count of
    // how many threads own the lock (aka a read lock)
    static constexpr type lock_mask = TransactionTid::threadid_mask;

    using BasicVersion<TCommutativeVersion>::v_;

    TCommutativeVersion() = default;
    explicit TCommutativeVersion(type v)
            : BasicVersion<TCommutativeVersion>(v) {}

    bool cp_try_lock_impl(TransItem& item, int threadid) {
        (void)item;
        (void)threadid;
        return try_lock();
    }
    void cp_unlock_impl(TransItem& item) {
        (void)item;
        unlock();
    }
    void cp_set_version_unlock_impl(TCommutativeVersion new_v) {
        (void) new_v;
        always_assert(false, "not implemented");
    }
    void cp_set_version_impl(TCommutativeVersion new_v) {
        set_version(new_v);
    }
    bool cp_check_version_impl(TransItem& item) {
        auto old_vers = item.read_value<TCommutativeVersion>();
        int lock = item.needs_unlock()? 1 : 0;
        return v_ == (old_vers.v_ | lock);
    }
    void inc_nonopaque_impl() {
        inc_nonopaque_version();
    }

    inline bool observe_read_impl(TransItem& item, bool add_read);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);

private:
    bool is_locked() const {
        return (v_ & lock_mask) > 0;
    }
    unsigned num_locks() const {
        return unsigned(v_ & lock_mask);
    }

    bool try_lock() {
        lock();
        // never fails
        return true;
    }
    void lock() {
        __sync_fetch_and_add(&v_, 1);
    }
    void unlock() {
        __sync_fetch_and_add(&v_, -1);
    }

    type unlocked() const {
        return TransactionTid::unlocked(v_);
    }

    void set_version(TCommutativeVersion new_v) {
        assert((new_v.v_ & lock_mask) == 0);
        while (true) {
            type cur = v_;
            fence();
            // It's possible that someone else has already set the version to
            // a higher tid than us. That's okay--that had to have happened
            // after we got our lock, so readers are still invalidated, and a
            // higher tid will still invalidate old readers after we've
            // unlocked. We're done in that case.
            // (I think it'd be ok to downgrade the TID too but it's harder to
            // reason about).
            if (TransactionTid::unlocked(cur) > new_v.unlocked())
                return;
            // we maintain the lock state.
            type cur_acquired = cur & lock_mask;
            if (::bool_cmpxchg(&v_, cur, new_v.v_ | cur_acquired))
                break;
            relax_fence();
        }
    }

    void inc_nonopaque_version() {
        assert(is_locked());
        // can't quite fetch and add here either because we need to both
        // increment and OR in the nonopaque_bit.
        while (true) {
            type cur = v_;
            fence();
            type new_v = (cur + TransactionTid::increment_value) | TransactionTid::nonopaque_bit;
            release_fence();
            if (::bool_cmpxchg(&v_, cur, new_v))
                break;
            relax_fence();
        }
    }
};
