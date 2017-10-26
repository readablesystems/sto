#pragma once

#include "VersionBase.hh"

class TicTocTid : public TransactionTid {
public:
    using TransactionTid::is_locked_elsewhere;

    static constexpr type ts_shift = type(10);

    static type timestamp(type ts) {
        return ts >> ts_shift;
    }

    static void set_timestamp_locked(type& ts, type new_ts, type flags) {
        assert(is_locked_here(ts));
        new_ts = (new_ts << ts_shift) | lock_bit | TThread::id();
        ts = new_ts;
        release_fence();
    }

    static void set_timestamp_unlock(type& ts, type new_ts, type flags) {
        assert(is_locked_here(ts));
        new_ts <<= ts_shift;
        ts = new_ts | flags;
        release_fence();
    }

    static void set_timestamp(type& ts, type new_ts, type flags) {
        new_ts <<= ts_shift;
        release_fence();
        ts = new_ts | flags;
    }

    static bool validate_timestamps(type& tuple_wts, type& tuple_rts, type read_wts, type read_rts, type commit_ts) {
        while (true) {
            if (timestamp(read_rts) >= commit_ts)
                return true;
            else {
                auto t_wts = tuple_wts;
                acquire_fence();
                auto t_rts = tuple_rts;
                acquire_fence();
                if (timestamp(t_wts) != timestamp(read_wts)
                    || ((timestamp(t_rts) < commit_ts) && is_locked_elsewhere(t_rts)))
                    return false;
                if (timestamp(t_rts) < commit_ts) {
                    type v = commit_ts << ts_shift | (t_rts & (increment_value - 1));
                    if (bool_cmpxchg(&tuple_rts, t_rts, v))
                        return true;
                } else {
                    return true;
                }
            }
        }
    }

    static bool validate_timestamps_noext(const type& tuple_wts, const type& tuple_rts, type read_wts, type read_rts, type commit_ts) {
        if (timestamp(read_rts) >= commit_ts)
            return true;
        else {
            auto t_wts = tuple_wts;
            acquire_fence();
            auto t_rts = tuple_rts;
            acquire_fence();
            return !(timestamp(t_wts) != timestamp(read_wts)
                     || ((timestamp(t_rts) < commit_ts) && is_locked_elsewhere(t_rts)));
        }
    }

    static void pack_wide(WideTid& ts, const type& rts, const type& wts) {
        ts.v0 = rts;
        release_fence();
        ts.v1 = wts;
        release_fence();
    }

    static std::pair<type, type> unpack_wide(const WideTid& ts) {
        type r, w;
        r = ts.v0;
        acquire_fence();
        w = ts.v1;
        acquire_fence();
        return {r, w};
    };
};

class TicTocCompressedTid : public TransactionTid {
public:

    // Layout of TicToc Compressed TID:
    // TTid bits: compatibility bits as defined in TransactionTid

    // |-----WTS value-----|-delta-|--TTid bits--|
    //        46 bits       8 bits    10 bits

    static constexpr type delta_shift = type(10);
    static constexpr type wts_shift = type(18);
    static constexpr type delta_mask = type(0xff) << delta_shift;

    static type wts_value(type t) {
        return t >> wts_shift;
    }
    static type rts_value(type t) {
        type delta = (t & delta_mask) >> delta_shift;
        return wts_value(t) + delta;
    }

    static void set_timestamps(type& t, type commit_ts, type flags) {
        assert(is_locked_here(t));
        commit_ts = (commit_ts << wts_shift) | lock_bit | TThread::id();
        t = commit_ts | flags;
        release_fence();
    }
    static void set_timestamps_unlock(type& t, type commit_ts, type flags) {
        commit_ts <<= wts_shift;
        t = commit_ts | flags;
        release_fence();
    }
    static bool validate_timestamps(type& t, type read_tss, type commit_ts) {
        while (true) {
            type v = t;
            acquire_fence();
            if (rts_value(read_tss) >= commit_ts)
                return true;
            else {
                if (wts_value(v) != wts_value(read_tss) ||
                    ((rts_value(v) < commit_ts) && is_locked_elsewhere(t)))
                    return false;
                if (rts_value(v) < commit_ts) {
                    type vv = v;
                    type delta = commit_ts - wts_value(v);
                    type shift = delta - (delta & 0xff);
                    vv += (shift << wts_shift);
                    vv &= ~delta_mask;
                    vv |= (delta & 0xff << delta_shift);
                    if (bool_cmpxchg(&t, v, vv))
                        return true;
                } else {
                    return true;
                }
            }
        }
    }

    static bool validate_timestamps_noext(const type& t, type read_tss, type commit_ts) {
        type v = t;
        acquire_fence();
        if (rts_value(read_tss) >= commit_ts)
            return true;
        else {
            return !(wts_value(v) != wts_value(read_tss) ||
                     ((rts_value(v) < commit_ts) && is_locked_elsewhere(t)));
        }
    }
};

template <typename VersImpl>
class TicTocBase : public VersionBase<VersImpl> {
public:
    using BV = VersionBase<VersImpl>;
    using BV::v_;
    using BV::impl;

    type read_timestamp() const {
        return impl().read_timestamp_impl();
    }
    type write_timestamp() const {
        return impl().write_timestamp_impl();
    }
};

template <bool Opaque, bool Extend>
class TicTocVersion : public TicTocBase<TicTocVersion<Opaque, Extend>> {
public:
    using BV = TicTocBase<TicTocVersion<Opaque, Extend>>;

    type read_timestamp_impl() const {
        return TicTocTid::timestamp(BV::v_);
    }
    type write_timestamp_impl() const {
        return TicTocTid::timestamp(wts_);
    }

    bool cp_try_lock_impl(int threadid) {
        return TransactionTid::try_lock(BV::v_, threadid);
    }
    void cp_unlock_impl(TransItem& item) {
        (void)item;
        TransactionTid::unlock(BV::v_);
    }

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);

    void cp_set_version_unlock_impl(type new_ts) {
        TicTocTid::set_timestamp(wts_, new_ts, 0);
        TicTocTid::set_timestamp_unlock(BV::v_, new_ts, 0);
    }
    void cp_set_version_impl(type new_ts) {
        TicTocTid::set_timestamp_locked(BV::v_, new_ts, 0);
        TicTocTid::set_timestamp(wts_, new_ts, 0);
    }

    bool cp_check_version_impl(Transaction& txn, TransItem& item) {
        type read_rts, read_wts;
        std::tie(read_rts, read_wts) = TicTocTid::unpack_wide(item.wide_read_value());
        type current_cts = cp_commit_tid_impl(txn);

        if (Extend)
            return TicTocTid::validate_timestamps(wts_, BV::v_, read_wts, read_rts, current_cts);
        else
            return TicTocTid::validate_timestamps_noext(wts_, BV::v_, read_wts, read_rts, current_cts);
    }

    inline bool acquire_write_impl(TransItem& item);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, const T& wdata);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, T&& wdata);
    template <typename T, typename... Args>
    inline bool acquire_write_impl(TransItem& item, Args&&... args);

    inline bool observe_read_impl(TransItem& item, bool add_read);

private:
    // BV::v_ is used as rts
    type wts_;
};

template <bool Opaque, bool Extend>
class TicTocCompressedVersion : public VersionBase<TicTocCompressedVersion<Opaque, Extend>> {
public:
    using BV = VersionBase<TicTocCompressedVersion<Opaque, Extend>>;
    using BV::v_;

    bool cp_try_lock_impl(int threadid) {
        return TicTocCompressedTid::try_lock(v_, threadid);
    }
    void cp_unlock_impl(TransItem& item) {
        TicTocCompressedTid::unlock(v_);
    }

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);

    void cp_set_version_unlock(type new_ts) {
        TicTocCompressedTid::set_timestamps_unlock(v_, new_ts, 0);
    }
    void cp_set_version(type new_ts) {
        TicTocCompressedTid::set_timestamps(v_, new_ts, 0);
    }

    bool cp_check_version_impl(Transaction& txn, TransItem& item) {
        type read_tss = item.read_value<type>();
        type current_cts = cp_commit_tid_impl(txn);

        if (Extend)
            return TicTocCompressedTid::validate_timestamps(v_, read_tss, current_cts);
        else
            return TicTocCompressedTid::validate_timestamps_noext(v_, read_tss, current_cts);
    }

    inline bool acquire_write_impl(TransItem& item);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, const T& wdata);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, T&& wdata);
    template <typename T, typename... Args>
    inline bool acquire_write_impl(TransItem& item, Args&&... args);

    inline bool observe_read_impl(TransItem& item, bool add_read);
};