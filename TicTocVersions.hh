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

    void set_timestamp_unlock(type& ts, type new_ts, type flags) {
        assert(is_locked_here(ts));
        new_ts <<= ts_shift;
        ts = new_ts | flags;
        release_fence();
    }

    void set_timestamp(type& ts, type new_ts, type flags) {
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

template <bool Opaque>
class TicTocVersion : public VersionBase<TicTocVersion<Opaque>> {
public:
    using BV = VersionBase<TicTocVersion<Opaque>>;

private:
    // BV::v_ is used as rts
    type wts_;
};

template <bool Opaque>
class TicTocCompressedVersion : public VersionBase<TicTocCompressedVersion<Opaque>> {
public:
    using BV = VersionBase<TicTocCompressedVersion<Opaque>>;
    using BV::v_;

private:

};