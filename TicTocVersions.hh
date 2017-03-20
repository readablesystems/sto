#pragma once
#include "Interface.hh"

#ifndef TICTOC_IMPL_COMPOUND
#define TICTOC_IMPL_COMPOUND 0
#endif

// Transaction timestamp (TID) as described in the TicToc concurrency control paper

class LockableTid {
public:
    typedef uint64_t type;
    typedef int64_t signed_type;
    // lower 8 bits are defined for common synchronization purposes
    static constexpr type threadid_mask = type(0x1f);
    static constexpr type lock_bit = type(0x1<<5);      // bit 5
    static constexpr type nonopaque_bit = type(0x1<<6); // bit 6
    static constexpr type user_bit = type(0x1<<7);      // bit 7
    static constexpr type lockable_mask = type(0xff);

    // MEAT
    type t_;

    type value() const {
        return t_;
    }

    LockableTid(type t) : t_(t) {}
    LockableTid() : t_() {}

    bool is_locked() const {
        return t_ & lock_bit;
    }
    bool is_locked_here() const {
        return (t_ & (lock_bit | threadid_mask)) == (lock_bit | TThread::id());
    }
    bool is_locked_here(int here) const {
        return (t_ & (lock_bit | threadid_mask)) == (lock_bit | here);
    }
    bool is_locked_elsewhere() const {
        type m = t_ & (lock_bit | threadid_mask);
        return m != 0 && m != (lock_bit | TThread::id());
    }
    bool is_locked_elsewhere(int here) const {
        type m = t_ & (lock_bit | threadid_mask);
        return m != 0 && m != (lock_bit | here);
    }

    bool try_lock() {
        type vv = t_;
        return bool_cmpxchg(&t_, vv & ~lock_bit, vv | lock_bit | TThread::id());
    }
    bool try_lock(int here) {
        type vv = t_;
        return bool_cmpxchg(&t_, vv & ~lock_bit, vv | lock_bit | here);
    }
    void lock() {
        while (!try_lock())
            relax_fence();
        acquire_fence();
    }
    void lock(int here) {
        while (!try_lock(here))
            relax_fence();
        acquire_fence();
    }
    void unlock() {
        assert(is_locked_here());
        type new_v = t_ & ~(lock_bit | threadid_mask);
        release_fence();
        t_ = new_v;
    }
    void unlock(int here) {
        (void) here;
        assert(is_locked_here(here));
        type new_v = t_ & ~(lock_bit | threadid_mask);
        release_fence();
        t_ = new_v;
    }
    LockableTid unlocked() const {
        type t = t_;
        t &= ~(lock_bit | threadid_mask);
        return LockableTid(t);
    }

    bool has_user_bit_set() const {
        return t_ & user_bit;
    }
    void set_user_bit() {
        t_ |= user_bit;
    }
    void clear_user_bit() {
        t_ &= ~user_bit;
    }
    void set_nonopaque() {
        t_ |= nonopaque_bit;
    }
    bool is_nonopaque() const {
        return t_ & nonopaque_bit;
    }

    friend std::ostream& operator<<(std::ostream& w, LockableTid t) {
        auto f = w.flags();
        auto& v = t.t_;
        w << std::hex;
        v &= lockable_mask;
        if (v & ~(user_bit - 1))
            w << "U" << (v & ~(user_bit - 1));
        if (v & nonopaque_bit)
            w << "!";
        if (v & lock_bit)
            w << "L" << std::dec << (v & (lock_bit - 1));
        w.flags(f);
        return w;
    }
};

class RawTid : public LockableTid {
public:
    static constexpr type ts_shift = type(8);
    static constexpr type increment_value = type(0x1<<ts_shift);

    RawTid(type x) : LockableTid(x) {};
    RawTid() : LockableTid() {};
    RawTid& operator=(const LockableTid& t) {
        t_ = t.t_;
        return *this;
    }
    bool operator==(const RawTid& rhs) const {
        return t_ == rhs.t_;
    }
    bool operator!=(const RawTid& rhs) const {
        return t_ != rhs.t_;
    }

    type timestamp() const {
        return t_ >> ts_shift;
    }

    void set_timestamp(type new_ts, type flags) {
        assert(is_locked_here());
        new_ts = (new_ts << ts_shift) | lock_bit | TThread::id();
        release_fence();
        t_ = new_ts | flags;
    }
    void set_timestamp_unlock(type new_ts, type flags) {
        assert(is_locked_here());
        new_ts <<= ts_shift;
        release_fence();
        t_ = new_ts | flags;
    }
    void set_read_timestamp(type new_ts, type flags) {
        new_ts <<= ts_shift;
        release_fence();
        t_ = new_ts | flags;
    }

    static bool validate_timestamps(RawTid& tuple_wts, RawTid& tuple_rts, RawTid read_wts, RawTid read_rts, type commit_ts) {
        while (1) {
            auto t_wts = tuple_wts;
            acquire_fence();
            auto t_rts = tuple_rts;
            acquire_fence();
            if (read_rts.timestamp() >= commit_ts)
                return true;
            else {
                if (t_wts.timestamp() != read_wts.timestamp()
                    || ((t_rts.timestamp() < commit_ts) && t_wts.is_locked_elsewhere()))
                    return false;
                if (t_rts.timestamp() < commit_ts) {
                    type v = commit_ts << ts_shift;
                    if (bool_cmpxchg(&tuple_rts.t_, t_rts.t_, v))
                        return true;
                } else {
                    return true;
                }
            }
        }
    }

    static bool validate_timestamps_noext(const RawTid& tuple_wts, const RawTid& tuple_rts, RawTid read_wts, RawTid read_rts, type commit_ts) {
        auto t_wts = tuple_wts;
        acquire_fence();
        auto t_rts = tuple_rts;
        acquire_fence();
        if (read_rts.timestamp() >= commit_ts)
            return true;
        else {
            if (t_wts.timestamp() != read_wts.timestamp()
                || ((t_rts.timestamp() < commit_ts) && t_wts.is_locked_elsewhere()))
                return false;
            else
                return true;
        }
    }
};

class CompoundTid : public LockableTid {
public:
    static constexpr type delta_shift = type(8);
    static constexpr type wts_shift = type(16);
    static constexpr type delta_mask = type(0xff<<delta_shift);   // bits 8-15 hold "delta"
    static constexpr type increment_value = type(0x1<<wts_shift); // bits 16-63 hold wts

    CompoundTid(type x) : LockableTid(x) {};
    CompoundTid() : LockableTid() {};

    CompoundTid& operator=(const LockableTid& t) {
        t_ = t.t_;
        return *this;
    }
    bool operator==(const CompoundTid& rhs) const {
        return t_ == rhs.t_;
    }
    bool operator!=(const CompoundTid& rhs) const {
        return t_ != rhs.t_;
    }

    type wts_value() const {
        return t_ >> wts_shift;
    }
    type rts_value() const {
        type delta = (t_ & delta_mask) >> delta_shift;
        return wts_value() + delta;
    }

    void set_timestamps(type commit_ts, type flags) {
        assert(is_locked_here());
        commit_ts = (commit_ts << wts_shift) | lock_bit | TThread::id();
        release_fence();
        t_ = commit_ts | flags;
    }
    void set_timestamps_unlock(type commit_ts, type flags) {
        commit_ts <<= wts_shift;
        release_fence();
        t_ = commit_ts | flags;
    }
    bool validate_timestamps(CompoundTid read_tss, type commit_ts) {
        while (1) {
            type v = t_;
            acquire_fence();
            CompoundTid ct(v);
            if (read_tss.rts_value() >= commit_ts)
                return true;
            else {
                if (ct.wts_value() != read_tss.wts_value() ||
                    ((ct.rts_value() < commit_ts) && is_locked_elsewhere()))
                    return false;
                if (ct.rts_value() < commit_ts) {
                    type vv = v;
                    type delta = commit_ts - ct.wts_value();
                    type shift = delta - (delta & 0xff);
                    vv += (shift << wts_shift);
                    vv &= ~delta_mask;
                    vv |= (delta & 0xff << delta_shift);
                    if (bool_cmpxchg(&t_, v, vv))
                        return true;
                } else {
                     return true;
                }
            }
        }
    }

    bool validate_timestamps_noext(CompoundTid read_tss, type commit_ts) const {
        type v = t_;
        acquire_fence();
        CompoundTid ct(v);
        if (read_tss.rts_value() >= commit_ts)
            return true;
        else {
            if (ct.wts_value() != read_tss.wts_value() ||
                ((ct.rts_value() < commit_ts) && is_locked_elsewhere()))
                return false;
            else
                return true;
        }
    }
};

typedef LockableTid TicTocTid;

class TicTocVersion {
public:
    typedef LockableTid::type type;
    static constexpr type user_bit = LockableTid::user_bit;

    // constructors
    inline TicTocVersion();
    inline TicTocVersion(const TicTocVersion& rhs);
    inline TicTocVersion(type ts);

    // operators
    inline bool operator==(const TicTocVersion& x) const;
    inline bool operator!=(const TicTocVersion& x) const;
    inline TicTocVersion& operator=(const TicTocVersion& rhs);

    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    inline LockableTid& get_lockable();
    inline const LockableTid& get_lockable() const;

    // locking
    bool is_locked() const {
        return get_lockable().is_locked();
    }
    bool is_locked_here() const {
        return get_lockable().is_locked_here();
    }
    bool is_locked_here(int here) const {
        return get_lockable().is_locked_here(here);
    }
    bool is_locked_elsewhere() const {
        return get_lockable().is_locked_elsewhere();
    }
    bool is_locked_elsewhere(int here) const {
        return get_lockable().is_locked_elsewhere(here);
    }
    bool has_user_bit_set() const {
        return get_lockable().has_user_bit_set();
    }
    void set_user_bit() {
        get_lockable().set_user_bit();
    }
    void clear_user_bit() {
        get_lockable().clear_user_bit();
    }
    bool is_nonopaque() const {
        return get_lockable().is_nonopaque();
    }
    void set_nonopaque() {
        return get_lockable().set_nonopaque();
    }

    bool try_lock() {
        return get_lockable().try_lock();
    }
    bool try_lock(int here) {
        return get_lockable().try_lock(here);
    }
    void lock() {
        get_lockable().lock();
    }
    void lock(int here) {
        get_lockable().lock(here);
    }
    void unlock() {
        get_lockable().unlock();
    }
    void unlock(int here) {
        get_lockable().unlock(here);
    }

    inline bool is_locked_here(const Transaction& txn) const;
    inline bool is_locked_elsewhere(const Transaction& txn) const;
    inline TicTocVersion unlocked() const;

    inline type read_timestamp() const;
    inline type write_timestamp() const;

    inline void set_timestamps(type commit_ts, type flags = 0);
    inline void set_timestamps_unlock(type commit_ts, type flags = 0);
    inline bool validate_timestamps(const TicTocVersion& old_tss, type commit_ts);
    inline bool validate_timestamps_noext(const TicTocVersion& old_tss, type commit_ts) const;

    friend std::ostream& operator<<(std::ostream& w, TicTocVersion v) {
        LockableTid l = v.get_lockable();
        w << l;
        w << " w:" << v.write_timestamp();
        w << "r:" << v.read_timestamp();
        return w;
    }

    template <typename Exception>
    static inline void opaque_throw(const Exception& exception);
protected:
#if TICTOC_IMPL_COMPOUND == 1
    CompoundTid tss_;
#else
    RawTid wts_;
    RawTid rts_;
#endif
};

class TicTocNonopaqueVersion : public TicTocVersion {
public:
    static constexpr type nonopaque_bit = LockableTid::nonopaque_bit;
    TicTocNonopaqueVersion() : TicTocVersion(nonopaque_bit) {}
    TicTocNonopaqueVersion(const TicTocNonopaqueVersion& rhs) : TicTocVersion(rhs) {}
    TicTocNonopaqueVersion(type ts) : TicTocVersion(ts | nonopaque_bit) {}
};

// The guts of TicTocVersion depend on the actual underlying
// implementation (compound or separate words)

#if TICTOC_IMPL_COMPOUND == 1

// Implementing TicTocVersion using a compound word: wts + delta

TicTocVersion::TicTocVersion() : tss_() {}
TicTocVersion::TicTocVersion(const TicTocVersion& rhs) : tss_(rhs.tss_) {}
TicTocVersion::TicTocVersion(type ts) : tss_(ts) {}

bool TicTocVersion::operator==(const TicTocVersion& x) const {
    return tss_ == x.tss_;
}
bool TicTocVersion::operator!=(const TicTocVersion& x) const {
    return tss_ != x.tss_;
}
TicTocVersion& TicTocVersion::operator=(const TicTocVersion& rhs) {
    tss_ = rhs.tss_;
    return *this;
}

LockableTid& TicTocVersion::get_lockable() {
    return tss_;
}
const LockableTid& TicTocVersion::get_lockable() const {
    return tss_;
}

TicTocVersion TicTocVersion::unlocked() const {
    auto ts = *this;
    acquire_fence();
    ts.tss_ = ts.tss_.unlocked();
    return ts;
}

TicTocVersion::type TicTocVersion::read_timestamp() const {
    return tss_.rts_value();
}
TicTocVersion::type TicTocVersion::write_timestamp() const {
    return tss_.wts_value();
}

void TicTocVersion::set_timestamps(type commit_ts, type flags) {
    tss_.set_timestamps(commit_ts, flags);
}
void TicTocVersion::set_timestamps_unlock(type commit_ts, type flags) {
    tss_.set_timestamps_unlock(commit_ts, flags);
}
bool TicTocVersion::validate_timestamps(const TicTocVersion& old_tss, type commit_ts) {
    return tss_.validate_timestamps(old_tss.tss_, commit_ts);
}
bool TicTocVersion::validate_timestamps_noext(const TicTocVersion& old_tss, type commit_ts) const {
    return tss_.validate_timestamps_noext(old_tss.tss_, commit_ts);
}

#else

// Implementing TicTocVersion as two separate words: wts ++ rts

TicTocVersion::TicTocVersion() : wts_(), rts_() {}
TicTocVersion::TicTocVersion(const TicTocVersion& rhs) : wts_(rhs.wts_), rts_(rhs.rts_) {}
TicTocVersion::TicTocVersion(type ts) : wts_(ts), rts_(ts) {}

bool TicTocVersion::operator==(const TicTocVersion& x) const {
    return ((wts_ == x.wts_) && (rts_ == x.rts_));
}
bool TicTocVersion::operator!=(const TicTocVersion& x) const {
    return ((wts_ != x.wts_) || (rts_ != x.rts_));
}
TicTocVersion& TicTocVersion::operator=(const TicTocVersion& rhs) {
    wts_ = rhs.wts_;
    fence(); // XXX the correct fence?
    rts_ = rhs.rts_;
    return *this;
}

LockableTid& TicTocVersion::get_lockable() {
    return wts_;
}
const LockableTid& TicTocVersion::get_lockable() const {
    return wts_;
}

TicTocVersion TicTocVersion::unlocked() const {
    auto ts = *this;
    acquire_fence();
    ts.wts_ = ts.wts_.unlocked();
    return ts;
}

TicTocVersion::type TicTocVersion::read_timestamp() const {
    return rts_.timestamp();
}
TicTocVersion::type TicTocVersion::write_timestamp() const {
    return wts_.timestamp();
}

void TicTocVersion::set_timestamps(type commit_ts, type flags) {
    wts_.set_timestamp(commit_ts, flags);
    rts_.set_read_timestamp(commit_ts, flags);
}
void TicTocVersion::set_timestamps_unlock(type commit_ts, type flags) {
    rts_.set_read_timestamp(commit_ts, flags);
    wts_.set_timestamp_unlock(commit_ts, flags);
}
bool TicTocVersion::validate_timestamps(const TicTocVersion& old_tss, type commit_ts) {
    return RawTid::validate_timestamps(wts_, rts_, old_tss.wts_, old_tss.rts_, commit_ts);
}
bool TicTocVersion::validate_timestamps_noext(const TicTocVersion& old_tss, type commit_ts) const {
    return RawTid::validate_timestamps_noext(wts_, rts_, old_tss.wts_, old_tss.rts_, commit_ts);
}
#endif

template <typename Exception>
inline void TicTocVersion::opaque_throw(const Exception& exception) {
    throw exception;
}

