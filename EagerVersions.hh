#pragma once

// This file contains definition of classes TLockVersion and TSwissVersion
// They both perform eager (or pessimistic) write-write concurrency control and hence
// the file name

#include "VersionBase.hh"
#include "TThread.hh"

enum class LockResponse : int {locked, failed, optimistic, spin};

template <bool Adaptive>
class TLockVersion : public BasicVersion<TLockVersion<Adaptive>> {
public:
    typedef TransactionTid::type type;
    static constexpr type mask = TransactionTid::threadid_mask;
    static constexpr type rlock_cnt_max = type(0x10); // max number of readers
    static constexpr type lock_bit = TransactionTid::lock_bit;
    static constexpr type opt_bit = TransactionTid::opt_bit;
    static constexpr type dirty_bit = TransactionTid::dirty_bit;

    using BV = BasicVersion<TLockVersion<Adaptive>>;
    using BV::v_;

    TLockVersion() = default;
    explicit TLockVersion(type v)
            : BV(v) {}
    TLockVersion(type v, bool insert)
            : BV(v | (insert ? lock_bit : 0)) {}

    bool cp_try_lock_impl(TransItem& item, int threadid) {
        (void)item;
        (void)threadid;
        v_ |= dirty_bit;
        release_fence();
        return true;
    }

    void cp_unlock_impl(TransItem& item) {
        assert(item.needs_unlock());
        if (item.has_write()) {
            unlock_write();
            release_fence();
        } else {
            assert(item.has_read());
            unlock_read();
        }
    }
    bool cp_check_version_impl(Transaction& txn, TransItem& item) {
        (void)txn;
        type vv = v_;
        fence();
        if (TransactionTid::is_dirty(vv) && !item.has_write())
            return false;
        return TransactionTid::check_version(vv, item.read_value<type>());
    }
    void cp_set_version_unlock_impl(type new_v) {
        TransactionTid::set_version_unlock_dirty(v_, new_v);
    }

    inline bool acquire_write_impl(TransItem& item);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, const T& wdata);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, T&& wdata);
    template <typename T, typename... Args>
    inline bool acquire_write_impl(TransItem& item, Args&&... args);

    inline bool observe_read_impl(TransItem& item, bool add_read);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);

    bool hint_optimistic() const {
        acquire_fence();
        return (v_ & opt_bit) != 0;
    }

private:
    // read/writer/optimistic combined lock
    std::pair<LockResponse, type> try_lock_read() {
        while (true) {
            type vv = v_;
            bool write_locked = ((vv & lock_bit) != 0);
            type rlock_cnt = vv & mask;
            bool rlock_avail = rlock_cnt < rlock_cnt_max;
            if (write_locked)
                return std::make_pair(LockResponse::spin, type());
            if (!rlock_avail) {
                return std::make_pair(LockResponse::optimistic, vv);
            }
            if (::bool_cmpxchg(&v_, vv, (vv & ~mask) | (rlock_cnt+1)))
                return std::make_pair(LockResponse::locked, type());
            else
                relax_fence();
        }
    }

    LockResponse try_lock_write() {
        while (true) {
            type vv = v_;
            bool write_locked = ((vv & lock_bit) != 0);
            bool read_locked = ((vv & mask) != 0);
            if (write_locked || read_locked)
                return LockResponse::spin;
            if (::bool_cmpxchg(&v_, vv, (vv | lock_bit)))
                return LockResponse::locked;
            else
                relax_fence();
        }
    }

    LockResponse try_upgrade() {
        // XXX not used
        type vv = v_;
        type rlock_cnt = vv & mask;
        assert(!TransactionTid::is_locked(vv));
        assert(rlock_cnt >= 1);
        if ((rlock_cnt == 1) && ::bool_cmpxchg(&v_, vv, (vv - 1) | lock_bit))
            return LockResponse::locked;
        else
            return LockResponse::spin;
    }

    void unlock_read() {
        if (!Adaptive) {
            type vv = __sync_fetch_and_add(&v_, -1);
            (void)vv;
            assert((vv & mask) >= 1);
        } else {
            bool reset_opt_bit = TThread::gen[TThread::id()].chance(50);
            if (!reset_opt_bit) {
                fetch_and_add(&v_, -1);
            } else {
                while (1) {
                    type vv = v_;
                    assert((vv & mask) >= 1);
                    type new_v = (vv - 1) | opt_bit;
                    if (::bool_cmpxchg(&v_, vv, new_v))
                        break;
                    relax_fence();
                }
            }
        }
    }

    void unlock_write() {
        assert(BV::is_locked());
        type new_v;
        if (!Adaptive) {
            new_v = v_ & ~(lock_bit | dirty_bit | opt_bit);
        } else {
            new_v = v_ & ~(lock_bit | dirty_bit);
            if (((new_v & opt_bit) != 0) && TThread::gen[TThread::id()].chance(50)) {
                new_v &= ~opt_bit;
            }
        }
        v_ = new_v;
        release_fence();
    }

    inline bool try_upgrade_with_spin();
    inline bool try_lock_write_with_spin();
    inline std::pair<LockResponse, type> try_lock_read_with_spin();
    inline bool lock_for_write(TransItem& item);
};

template <bool Opacity>
class TSwissVersion : public BasicVersion<TSwissVersion<Opacity>> {
public:
    typedef TransactionTid::type type;
    static constexpr bool is_opaque = Opacity;
    static constexpr type lock_bit = TransactionTid::lock_bit;
    static constexpr type threadid_mask = TransactionTid::threadid_mask;
    static constexpr type dirty_bit = TransactionTid::dirty_bit;

    using BV = BasicVersion<TSwissVersion<Opacity>>;
    using BV::v_;

    TSwissVersion()
            : BV(Opacity ? TransactionTid::nonopaque_bit : 0) {}
    explicit TSwissVersion(type v, bool insert = true)
            : BV(v | (insert ? (lock_bit | TThread::id()) : 0)) {
        if (Opacity)
            v_ |= TransactionTid::nonopaque_bit;
    }

    // commit-time lock for TSwissVersion is just setting the read-lock (dirty) bit
    bool cp_try_lock_impl(TransItem& item, int threadid) {
        (void)item;
        (void)threadid;
        assert(BV::is_locked_here(threadid));
        v_ |= dirty_bit;
        release_fence();
        return true;
    }
    void cp_unlock_impl(TransItem& item) {
        (void)item;
        assert(item.needs_unlock());
        if (BV::is_locked()) {
            TransactionTid::unlock_dirty(v_);
        }
    }
    bool cp_check_version_impl(Transaction& txn, TransItem& item) {
        (void)txn;
        type vv = v_;
        fence();
        if (TransactionTid::is_dirty(vv) && !TransactionTid::is_locked_here(vv))
            return false;
        return TransactionTid::check_version(vv, item.read_value<type>());
    }
    void cp_set_version_unlock_impl(type new_v) {
        TransactionTid::set_version_unlock_dirty(v_, new_v);
    }

    inline bool acquire_write_impl(TransItem& item);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, const T& wdata);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, T&& wdata);
    template <typename T, typename... Args>
    inline bool acquire_write_impl(TransItem& item, Args&&... args);

    inline bool observe_read_impl(TransItem& item, bool add_read);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);

private:
    type try_lock_val() {
        return TransactionTid::try_lock_val(v_);
    }
};
