#pragma once
#include "Interface.hh"

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

    // MEAT
    type t_;

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
};

class RawTid : public LockableTid {
public:
    static constexpr type ts_shift = type(8);
    static constexpr type increment_value = type(0x1<<ts_shift);

    RawTid(type x) : LockableTid(x) {};
    RawTid() : LockableTid() {};
    void operator=(const LockableTid& t) {
        t_ = t.t_;
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
        auto t_wts = tuple_wts;
        acquire_fence();
        auto t_rts = tuple_rts;
        acquire_fence();
        if (t_wts.timestamp() != read_wts.timestamp()
            || ((t_rts.timestamp() <= commit_ts) && t_wts.is_locked_elsewhere()))
            return false;
        if (read_rts.timestamp() <= commit_ts) {
            type v = std::max(t_rts.t_, commit_ts);
            bool_cmpxchg(&tuple_rts.t_, t_rts.t_, v); // should be fine if CAS fails (?)
        }
        return true;
    }

    static void print(RawTid t, std::ostream& w) {
        auto f = w.flags();
        auto& v = t.t_;
        w << "ts:" << std::hex << t.timestamp();
        v &= increment_value - 1;
        if (v & ~(user_bit - 1))
            w << "U" << (v & ~(user_bit - 1));
        if (v & nonopaque_bit)
            w << "!";
        if (v & lock_bit)
            w << "L" << std::dec << (v & (lock_bit - 1));
        w.flags(f);
    }
};

class CompoundTid : public LockableTid {
public:
    static constexpr type delta_shift = type(8);
    static constexpr type wts_shift = type(16);
    static constexpr type delta_mask = type(0xff<<delta_shift);   // bits 8-15 hold "delta"
    static constexpr type increment_value = type(0x1<<wts_shift); // bits 16-63 hold wts

    CompoundTid(uint64_t x) : t_(x) {};
    CompoundTid() : t_() {};

    uint64_t wts_value() const;
    uint64_t rts_value() const;

    void set_timestamps(uint64_t commit_ts);
    bool validate_timestamps(CompoundTid read_tss, uint64_t commit_ts);
private:
    type t_;
};

typedef RawTid TicTocTid;

class TicTocVersion {
public:
    typedef LockableTid::type type;
    static constexpr type user_bit = LockableTid::user_bit;

    TicTocVersion() : wts_(), rts_() {}
    TicTocVersion(const TicTocVersion& rhs) : wts_(rhs.wts_), rts_(rhs.rts_) {}
    TicTocVersion(type ts) : wts_(ts), rts_(ts) {}
    TicTocVersion(type wts, type rts) : wts_(wts), rts_(rts) {}

    bool operator==(const TicTocVersion& x) const {
        return ((wts_ == x.wts_) && (rts_ == x.rts_));
    }
    bool operator!=(const TicTocVersion& x) const {
        return ((wts_ != x.wts_) || (rts_ != x.rts_));
    }
    TicTocVersion& operator=(const TicTocVersion& rhs) {
        wts_ = rhs.wts_;
        fence(); // XXX the correct fence?
        rts_ = rhs.rts_;
        return *this;
    }

    LockableTid& get_lockable() {
        return wts_;
    }

    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    bool is_locked() const {
        return wts_.is_locked();
    }
    bool is_locked_here() const {
        return wts_.is_locked_here();
    }
    bool is_locked_here(int here) const {
        return wts_.is_locked_here(here);
    }
    inline bool is_locked_here(const Transaction& txn) const;
    bool is_locked_elsewhere() const {
        return wts_.is_locked_elsewhere();
    }
    bool is_locked_elsewhere(int here) const {
        return wts_.is_locked_elsewhere(here);
    }
    inline bool is_locked_elsewhere(const Transaction& txn) const;
    bool has_user_bit_set() const {
        return wts_.has_user_bit_set();
    }
    void set_user_bit() {
        wts_.set_user_bit();
    }
    void clear_user_bit() {
        wts_.clear_user_bit();
    }

    bool try_lock() {
        return wts_.try_lock();
    }
    bool try_lock(int here) {
        return wts_.try_lock(here);
    }
    void lock() {
        wts_.lock();
    }
    void lock(int here) {
        wts_.lock(here);
    }
    void unlock() {
        wts_.unlock();
    }
    void unlock(int here) {
        wts_.unlock(here);
    }
    TicTocVersion unlocked() const {
        auto ts = *this;
        acquire_fence();
        ts.wts_ = ts.wts_.unlocked();
        return ts;
    }

    type read_timestamp() const {
        return rts_.timestamp();
    }
    type write_timestamp() const {
        return wts_.timestamp();
    }

    void set_timestamps(type commit_ts, type flags = 0) {
        wts_.set_timestamp(commit_ts, flags);
        rts_.set_read_timestamp(commit_ts, flags);
    }
    void set_timestamps_unlock(type commit_ts, type flags = 0) {
        rts_.set_read_timestamp(commit_ts, flags);
        wts_.set_timestamp_unlock(commit_ts, flags);
    }
    bool validate_timestamps(const TicTocVersion& old_tss, type commit_ts) {
        return RawTid::validate_timestamps(wts_, rts_, old_tss.wts_, old_tss.rts_, commit_ts);
    }

    friend std::ostream& operator<<(std::ostream& w, TicTocVersion v) {
        w << "w:";
        RawTid::print(v.wts_, w);
        w << "r:";
        RawTid::print(v.rts_, w);
        return w;
    }

    template <typename Exception>
    static inline void opaque_throw(const Exception& exception);
private:
    RawTid wts_;
    RawTid rts_;
};

template <typename Exception>
inline void TicTocVersion::opaque_throw(const Exception& exception) {
    throw exception;
}

