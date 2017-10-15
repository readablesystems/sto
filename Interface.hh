#pragma once
#include <cstdint>
#include <cassert>
#include <iostream>
#include <random>

#include "config.h"
#include "compiler.hh"

class Transaction;
class TransItem;
class TransProxy;

struct PercentGen {
    //std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<int> dist;

    PercentGen() : gen(0), dist(0,99) {}
    ~PercentGen() {}

    int sample() {
        return dist(gen);
    }
    bool chance(int percent) {
        return (sample() < percent);
    }
};

class TThread {
    static __thread int the_id;
    static __thread bool always_allocate_;
    static __thread int hashsize_;
public:
    static __thread Transaction* txn;
    static PercentGen gen[];

    static int id() {
        return the_id;
    }
    static void set_id(int id) {
        assert(id >= 0 && id < 32);
        the_id = id;
    }
    static bool always_allocate() {
        return always_allocate_;
    }
    static void set_always_allocate(bool always_allocate) {
        always_allocate_ = always_allocate;
    }
    static int hashsize() {
        return hashsize_;
    }
    static void set_hashsize(int hashsize) {
        hashsize_ = hashsize;
    }
};

enum class LockResponse : int {locked, failed, optmistic, spin};

class TransactionTid {
public:
    typedef uint64_t type;
    typedef int64_t signed_type;

    static constexpr type threadid_mask = type(0x1F);
    static constexpr type rlock_cnt_max = type(0x10);
    static constexpr type lock_bit = type(0x20);
    // Used for data structures that don't use opacity. When they increment
    // a version they set the nonopaque_bit, forcing any opacity check to be
    // hard (checking the full read set).
    static constexpr type nonopaque_bit = type(0x40);
    static constexpr type user_bit = type(0x80);
    static constexpr type read_lock_bit = type(0x100);
    static constexpr type opt_bit = type(0x200);
    static constexpr type increment_value = type(0x400);

#if 0
    // TODO: probably remove these once RBTree stops referencing them.
    static void lock_read(type& v) {
        while (1) {
            type vv = v;
            if (!(vv & lock_bit) && bool_cmpxchg(&v, vv, vv+1))
                break;
            relax_fence();
        }
        acquire_fence();
    }
    static void lock_write(type& v) {
        while (1) {
            type vv = v;
            if ((vv & threadid_mask) == 0 && !(vv & lock_bit) && bool_cmpxchg(&v, vv, vv | lock_bit))
                break;
            relax_fence();
        }
        acquire_fence();
    }
    // for use with lock_write() and lock_read() (should probably be a different class altogether...)
    // it could theoretically be useful to have an opacity-safe reader-writer lock (which this is not currently)
    static void inc_write_version(type& v) {
        assert(is_locked(v));
        type new_v = (v + increment_value);
        release_fence();
        v = new_v;
    }
    static void unlock_read(type& v) {
        auto prev = __sync_fetch_and_add(&v, -1);
        (void)prev;
        assert(prev > 0);
    }
    static void unlock_write(type& v) {
        v = v & ~lock_bit;
    }
#endif

    // hacky state used by adaptive read/write lock
    static int unlock_opt_chance;

    // read/write/optimistic combined lock
    static std::pair<LockResponse, type> try_lock_read(type& v) {
        while (1) {
            type vv = v;
            bool write_locked = vv & lock_bit;
            type rlock_cnt = vv & threadid_mask;
            bool rlock_avail = rlock_cnt < rlock_cnt_max;
            if (write_locked)
                return std::make_pair(LockResponse::spin, type());
            if (!rlock_avail)
                return std::make_pair(LockResponse::optmistic, vv);
            if (bool_cmpxchg(&v, vv, (vv & ~threadid_mask) | (rlock_cnt+1)))
                return std::make_pair(LockResponse::locked, type());
            else
                relax_fence();
        }
    }

    static LockResponse try_lock_write(type& v) {
        while (1) {
            type vv = v;
            bool write_locked = vv & lock_bit;
            bool read_locked = vv & threadid_mask;
            if (write_locked || read_locked)
                return LockResponse::spin;
            //if (read_locked)
            //    return LockResponse::spin;
            if (bool_cmpxchg(&v, vv,
#if ADAPTIVE_RWLOCK == 0
                (vv | lock_bit)
#else
                (vv | lock_bit) & ~opt_bit
#endif
            )) {
                return LockResponse::locked;
            }
            else
                relax_fence();
        }
    }

    static LockResponse rwlock_try_upgrade(type& v) {
        // XXX not used
        type vv = v;
        type rlock_cnt = vv & threadid_mask;
        assert(!is_locked(vv));
        assert(rlock_cnt >= 1);
        if ((rlock_cnt == 1) && bool_cmpxchg(&v, vv, (vv - 1) | lock_bit))
            return LockResponse::locked;
        else
            return LockResponse::spin;
    }

    static void unlock_read(type& v) {
#if ADAPTIVE_RWLOCK == 0
        type vv = __sync_fetch_and_add(&v, -1);
        (void)vv;
        assert((vv & threadid_mask) >= 1);
#else
        while (1) {
            type vv = v;
            assert((vv & threadid_mask) >= 1);
            type new_v = TThread::gen[TThread::id()].chance(unlock_opt_chance) ? ((vv - 1) | opt_bit) : (vv - 1);
            if (bool_cmpxchg(&v, vv, new_v))
                break;
            relax_fence();
        }
#endif
    }

    static void unlock_write(type& v) {
        assert(is_locked(v));
#if ADAPTIVE_RWLOCK == 0
        type new_v = v & ~lock_bit;
#else
        type new_v = TThread::gen[TThread::id()].chance(unlock_opt_chance) ? ((v & ~lock_bit) | opt_bit) : (v & ~lock_bit);
#endif
        release_fence();
        v = new_v;
    }

    static bool is_locked(type v) {
        return v & lock_bit;
    }
    static bool is_optimistic(type v) {
        return v & opt_bit;
    }
    static bool is_locked_here(type v) {
        return (v & (lock_bit | threadid_mask)) == (lock_bit | TThread::id());
    }
    static bool is_locked_here(type v, int here) {
        return (v & (lock_bit | threadid_mask)) == (lock_bit | here);
    }
    static bool is_locked_elsewhere(type v) {
        type m = v & lock_bit;
        type t = v & threadid_mask;
        return m != 0 && (t != (type)TThread::id());
    }
    static bool is_locked_elsewhere(type v, int here) {
        type m = v & lock_bit;
        type t = v & threadid_mask;
        return m != 0 && t != (type)here;
    }
    static bool set_lock(type& v) {
        v = v | lock_bit | TThread::id();
        return true;
    }
    static bool try_lock(type& v) {
        type vv = v;
        return bool_cmpxchg(&v, vv & ~lock_bit, vv | lock_bit | TThread::id());
    }
    static bool try_lock(type& v, int here) {
        type vv = v;
        return bool_cmpxchg(&v, vv & ~lock_bit, vv | lock_bit | here);
    }
    static void lock(type& v) {
        while (!try_lock(v))
            relax_fence();
        acquire_fence();
    }
    static void lock(type& v, int here) {
        while (!try_lock(v, here))
            relax_fence();
        acquire_fence();
    }
    static void unlock(type& v) {
        //assert(is_locked_here(v));
        type new_v = v & ~(lock_bit | threadid_mask);
        release_fence();
        v = new_v;
    }
    static void unlock(type& v, int here) {
        (void) here;
        assert(is_locked_here(v, here));
        type new_v = v & ~(lock_bit | threadid_mask);
        release_fence();
        v = new_v;
    }
    static type unlocked(type v) {
      return v & ~(lock_bit | threadid_mask);
    }

    static void set_version(type& v, type new_v) {
        assert(is_locked_here(v));
        assert(!(new_v & (lock_bit | threadid_mask)));
        new_v |= lock_bit | TThread::id();
        release_fence();
        v = new_v;
    }
    static void set_version(type& v, type new_v, int here) {
        assert(is_locked_here(v, here));
        assert(!(new_v & (lock_bit | threadid_mask)));
        new_v |= lock_bit | here;
        release_fence();
        v = new_v;
    }
    static void set_version_locked(type& v, type new_v) {
        assert(is_locked_here(v));
        assert(is_locked_here(new_v));
        release_fence();
        v = new_v;
    }
    static void set_version_locked(type& v, type new_v, int here) {
        (void) here;
        assert(is_locked_here(v, here));
        assert(is_locked_here(new_v, here));
        release_fence();
        v = new_v;
    }
    static void set_version_unlock(type& v, type new_v) {
        //assert(is_locked_here(v));
        //assert(!is_locked(new_v) || is_locked_here(new_v));
        new_v &= ~(lock_bit | threadid_mask);
        release_fence();
        v = new_v;
    }
    static void set_version_unlock(type& v, type new_v, int here) {
        (void) here;
        assert(is_locked_here(v, here));
        assert(!is_locked(new_v) || is_locked_here(new_v, here));
        new_v &= ~(lock_bit | threadid_mask);
        release_fence();
        v = new_v;
    }

    static void set_nonopaque(type& v) {
        v |= nonopaque_bit;
    }
    static type next_nonopaque_version(type v) {
        return (v + increment_value) | nonopaque_bit;
    }
    static type next_unflagged_version(type v) {
        return ((v + increment_value) & ~(increment_value - 1));
    }
    static type next_unflagged_nonopaque_version(type v) {
        return next_unflagged_version(v) | nonopaque_bit;
    }
    static void inc_nonopaque_version(type& v) {
        assert(is_locked_here(v));
        type new_v = (v + increment_value) | nonopaque_bit;
        release_fence();
        v = new_v;
    }

    static bool check_version(type cur_vers, type old_vers) {
        //assert(!is_locked_elsewhere(old_vers));
        // cur_vers allowed to be locked by us
        //if ((cur_vers & lock_bit) && ((cur_vers & threadid_mask) != (type)TThread::id()))
        //    return false;
        return (cur_vers & ~(increment_value - 1)) == (old_vers & ~(increment_value - 1));
        //return cur_vers == old_vers || cur_vers == (old_vers | lock_bit | TThread::id());
    }
    static bool check_version(type cur_vers, type old_vers, int here) {
        assert(!is_locked_elsewhere(old_vers));
        // cur_vers allowed to be locked by us
        if ((cur_vers & lock_bit) && ((cur_vers & threadid_mask) != (type)here))
            return false;
        return (cur_vers & ~(increment_value - 1)) == (old_vers & ~(increment_value - 1));
        //return cur_vers == old_vers || cur_vers == (old_vers | lock_bit | here);
    }
    static bool try_check_opacity(type start_tid, type v) {
        signed_type delta = start_tid - v;
        return delta > 0 && !(v & (lock_bit | nonopaque_bit));
    }

    static void print(type v, std::ostream& w) {
        auto f = w.flags();
        w << std::hex << (v & ~(increment_value - 1));
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

static constexpr TransactionTid::type initialized_tid = TransactionTid::increment_value;

class TLockVersion {
public:
    typedef TransactionTid::type type;
    static constexpr type user_bit = TransactionTid::user_bit;

    TLockVersion()
        : v_() {
    }
    TLockVersion(type v)
        : v_(v) {
    }

    type value() const {
        return v_;
    }
    volatile type& value() {
        return v_;
    }
    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    bool is_locked() const {
        return TransactionTid::is_locked(v_);
    }
    bool is_optimistic() const {
        return TransactionTid::is_optimistic(v_);
    }
    bool bool_cmpxchg(TLockVersion expected, TLockVersion desired) {
        return ::bool_cmpxchg(&v_, expected.v_, desired.v_);
    }

    std::pair<LockResponse, type> try_lock_read() {
        return TransactionTid::try_lock_read(v_);
    }
    LockResponse try_lock_write() {
        return TransactionTid::try_lock_write(v_);
    }
    LockResponse try_upgrade() {
        return TransactionTid::rwlock_try_upgrade(v_);
    }
    void unlock_read() {
        TransactionTid::unlock_read(v_);
    }
    void unlock_write() {
        TransactionTid::unlock_write(v_);
    }
    type unlocked() const {
        return TransactionTid::unlocked(v_);
    }

    bool operator==(TLockVersion x) const {
        return v_ == x.v_;
    }
    bool operator!=(TLockVersion x) const {
        return v_ != x.v_;
    }
    TLockVersion operator|(TLockVersion x) const {
        return TLockVersion(v_ | x.v_);
    }

    void set_version(TLockVersion new_v) {
        TransactionTid::set_version(v_, new_v.v_);
    }
    void set_version_unlock(TLockVersion new_v) {
        TransactionTid::set_version_unlock(v_, new_v.v_);
    }

    bool check_version(TLockVersion old_vers) const {
        // XXX opacity <- THis comment is irrelevant on new API
        return TransactionTid::check_version(v_, old_vers.v_);
    }
    bool check_version(TLockVersion old_vers, int here) const {
        return TransactionTid::check_version(v_, old_vers.v_, here);
    }

    friend std::ostream& operator<<(std::ostream& w, TLockVersion v) {
        TransactionTid::print(v.value(), w);
        return w;
    }

    template <typename Exception>
    static inline void opaque_throw(const Exception& exception);
private:
    type v_;
};

template <bool Opacity>
class TSwissVersion {
public:
    typedef TransactionTid::type type;
    typedef TransactionTid::signed_type signed_type;
    static constexpr type read_lock_bit = TransactionTid::user_bit;

    TSwissVersion()
            : v_(initialized_tid | (Opacity ? 0 : TransactionTid::nonopaque_bit)) {}
    TSwissVersion(type v)
            : v_(v) {}

    type value() const {
        return v_;
    }
    volatile type& value() {
        return v_;
    }
    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    bool is_locked() const {
        return TransactionTid::is_locked(v_);
    }
    bool is_locked_here() const {
        return TransactionTid::is_locked_here(v_);
    }
    bool is_locked_here(int here) const {
        return TransactionTid::is_locked_here(v_, here);
    }
    inline bool is_locked_here(const Transaction& txn) const;
    bool is_locked_elsewhere() const {
        return TransactionTid::is_locked_elsewhere(v_);
    }
    bool is_locked_elsewhere(int here) const {
        return TransactionTid::is_locked_elsewhere(v_, here);
    }
    inline bool is_locked_elsewhere(const Transaction& txn) const;

    bool bool_cmpxchg(TSwissVersion expected, TSwissVersion desired) {
        return ::bool_cmpxchg(&v_, expected.v_, desired.v_);
    }
    bool set_lock() {
        return TransactionTid::set_lock(v_);
    }
    bool try_lock() {
        return TransactionTid::try_lock(v_);
    }
    bool try_lock(int here) {
        return TransactionTid::try_lock(v_, here);
    }
    void lock() {
        TransactionTid::lock(v_);
    }
    void lock(int here) {
        TransactionTid::lock(v_, here);
    }
    void unlock() {
        TransactionTid::unlock(v_);
    }
    void unlock(int here) {
        TransactionTid::unlock(v_, here);
    }
    type unlocked() const {
        return TransactionTid::unlocked(v_);
    }

    void set_read_lock() {
        v_ |= read_lock_bit;
        fence();
    }
    bool is_read_locked_elsewhere() const {
        return (v_ & read_lock_bit) != 0;
    }

    bool operator==(TSwissVersion x) const {
        return v_ == x.v_;
    }
    bool operator!=(TSwissVersion x) const {
        return v_ != x.v_;
    }
    TSwissVersion operator|(TSwissVersion x) const {
        return TSwissVersion(v_ | x.v_);
    }
    type operator&(TransactionTid::type x) const {
        return (v_ & x);
    }

    void set_version(TSwissVersion new_v) {
        TransactionTid::set_version(v_, new_v.v_);
    }
    void set_version(TSwissVersion new_v, int here) {
        TransactionTid::set_version(v_, new_v.v_, here);
    }
    void set_version_locked(TSwissVersion new_v) {
        TransactionTid::set_version_locked(v_, new_v.v_);
    }
    void set_version_locked(TSwissVersion new_v, int here) {
        TransactionTid::set_version_locked(v_, new_v.v_, here);
    }
    void set_version_unlock(TSwissVersion new_v) {
        TransactionTid::set_version_unlock(v_, new_v.v_);
    }
    void set_version_unlock(TSwissVersion new_v, int here) {
        TransactionTid::set_version_unlock(v_, new_v.v_, here);
    }
    void inc_nonopaque_version() {
        TransactionTid::inc_nonopaque_version(v_);
    }

    bool check_version(TSwissVersion old_vers) const {
        return TransactionTid::check_version(v_, old_vers.v_);
    }
    bool check_version(TSwissVersion old_vers, int here) const {
        return TransactionTid::check_version(v_, old_vers.v_, here);
    }

    friend std::ostream& operator<<(std::ostream& w, TSwissVersion v) {
        TransactionTid::print(v.value(), w);
        return w;
    }

    template <typename Exception>
    static inline void opaque_throw(const Exception&);

private:
    type v_;
};

class TVersion {
public:
    typedef TransactionTid::type type;
    typedef TransactionTid::signed_type signed_type;
    static constexpr type user_bit = TransactionTid::user_bit;

    TVersion()
        : v_() {
    }
    TVersion(type v)
        : v_(v) {
    }

    type value() const {
        return v_;
    }
    volatile type& value() {
        return v_;
    }
    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    bool is_locked() const {
        return TransactionTid::is_locked(v_);
    }
    bool is_locked_here() const {
        return TransactionTid::is_locked_here(v_);
    }
    bool is_locked_here(int here) const {
        return TransactionTid::is_locked_here(v_, here);
    }
    inline bool is_locked_here(const Transaction& txn) const;
    bool is_locked_elsewhere() const {
        return TransactionTid::is_locked_elsewhere(v_);
    }
    bool is_locked_elsewhere(int here) const {
        return TransactionTid::is_locked_elsewhere(v_, here);
    }
    inline bool is_locked_elsewhere(const Transaction& txn) const;

    bool bool_cmpxchg(TVersion expected, TVersion desired) {
        return ::bool_cmpxchg(&v_, expected.v_, desired.v_);
    }
    bool set_lock() {
        return TransactionTid::set_lock(v_);
    }
    bool try_lock() {
        return TransactionTid::try_lock(v_);
    }
    bool try_lock(int here) {
        return TransactionTid::try_lock(v_, here);
    }
    void lock() {
        TransactionTid::lock(v_);
    }
    void lock(int here) {
        TransactionTid::lock(v_, here);
    }
    void unlock() {
        TransactionTid::unlock(v_);
    }
    void unlock(int here) {
        TransactionTid::unlock(v_, here);
    }
    type unlocked() const {
        return TransactionTid::unlocked(v_);
    }

    bool operator==(TVersion x) const {
        return v_ == x.v_;
    }
    bool operator!=(TVersion x) const {
        return v_ != x.v_;
    }
    TVersion operator|(TVersion x) const {
        return TVersion(v_ | x.v_);
    }
    type operator&(TransactionTid::type x) const {
        return (v_ & x);
    }

    void set_version(TVersion new_v) {
        TransactionTid::set_version(v_, new_v.v_);
    }
    void set_version(TVersion new_v, int here) {
        TransactionTid::set_version(v_, new_v.v_, here);
    }
    void set_version_locked(TVersion new_v) {
        TransactionTid::set_version_locked(v_, new_v.v_);
    }
    void set_version_locked(TVersion new_v, int here) {
        TransactionTid::set_version_locked(v_, new_v.v_, here);
    }
    void set_version_unlock(TVersion new_v) {
        TransactionTid::set_version_unlock(v_, new_v.v_);
    }
    void set_version_unlock(TVersion new_v, int here) {
        TransactionTid::set_version_unlock(v_, new_v.v_, here);
    }

    void set_nonopaque() {
        TransactionTid::set_nonopaque(v_);
    }
    void inc_nonopaque_version() {
        TransactionTid::inc_nonopaque_version(v_);
    }

    bool check_version(TVersion old_vers) const {
        // XXX opacity <- THis comment is irrelevant on new API
        return TransactionTid::check_version(v_, old_vers.v_);
    }
    bool check_version(TVersion old_vers, int here) const {
        return TransactionTid::check_version(v_, old_vers.v_, here);
    }

    friend std::ostream& operator<<(std::ostream& w, TVersion v) {
        TransactionTid::print(v.value(), w);
        return w;
    }

    template <typename Exception>
    static inline void opaque_throw(const Exception& exception);

private:
    type v_;
};

class TNonopaqueVersion {
public:
    typedef TransactionTid::type type;
    typedef TransactionTid::signed_type signed_type;
    static constexpr type user_bit = TransactionTid::user_bit;

    TNonopaqueVersion()
        : v_(TransactionTid::nonopaque_bit) {
    }
    TNonopaqueVersion(type v)
        : v_(v) {
    }

    type value() const {
        return v_;
    }
    volatile type& value() {
        return v_;
    }
    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    bool is_locked() const {
        return TransactionTid::is_locked(v_);
    }
    bool is_locked_here() const {
        return TransactionTid::is_locked_here(v_);
    }
    bool is_locked_here(int here) const {
        return TransactionTid::is_locked_here(v_, here);
    }
    inline bool is_locked_here(const Transaction& txn) const;
    bool is_locked_elsewhere() const {
        return TransactionTid::is_locked_elsewhere(v_);
    }
    bool is_locked_elsewhere(int here) const {
        return TransactionTid::is_locked_elsewhere(v_, here);
    }
    inline bool is_locked_elsewhere(const Transaction& txn) const;

    bool bool_cmpxchg(TNonopaqueVersion expected, TNonopaqueVersion desired) {
        return ::bool_cmpxchg(&v_, expected.v_, desired.v_);
    }
    bool set_lock() {
        return TransactionTid::set_lock(v_);
    }
    bool try_lock() {
        return TransactionTid::try_lock(v_);
    }
    bool try_lock(int here) {
        return TransactionTid::try_lock(v_, here);
    }
    void lock() {
        TransactionTid::lock(v_);
    }
    void lock(int here) {
        TransactionTid::lock(v_, here);
    }
    void unlock() {
        TransactionTid::unlock(v_);
    }
    void unlock(int here) {
        TransactionTid::unlock(v_, here);
    }
    type unlocked() const {
        return TransactionTid::unlocked(v_);
    }

    bool operator==(TNonopaqueVersion x) const {
        return v_ == x.v_;
    }
    bool operator!=(TNonopaqueVersion x) const {
        return v_ != x.v_;
    }
    TNonopaqueVersion operator|(TNonopaqueVersion x) const {
        return TNonopaqueVersion(v_ | x.v_);
    }
    type operator&(TransactionTid::type x) const {
        return (v_ & x);
    }

    void set_version(TNonopaqueVersion new_v) {
        TransactionTid::set_version(v_, new_v.v_);
    }
    void set_version(TNonopaqueVersion new_v, int here) {
        TransactionTid::set_version(v_, new_v.v_, here);
    }
    void set_version_locked(TNonopaqueVersion new_v) {
        TransactionTid::set_version_locked(v_, new_v.v_);
    }
    void set_version_locked(TNonopaqueVersion new_v, int here) {
        TransactionTid::set_version_locked(v_, new_v.v_, here);
    }
    void set_version_unlock(TNonopaqueVersion new_v) {
        TransactionTid::set_version_unlock(v_, new_v.v_);
    }
    void set_version_unlock(TNonopaqueVersion new_v, int here) {
        TransactionTid::set_version_unlock(v_, new_v.v_, here);
    }
    void inc_nonopaque_version() {
        TransactionTid::inc_nonopaque_version(v_);
    }

    bool check_version(TNonopaqueVersion old_vers) const {
        return TransactionTid::check_version(v_, old_vers.v_);
    }
    bool check_version(TNonopaqueVersion old_vers, int here) const {
        return TransactionTid::check_version(v_, old_vers.v_, here);
    }

    friend std::ostream& operator<<(std::ostream& w, TNonopaqueVersion v) {
        TransactionTid::print(v.value(), w);
        return w;
    }

    template <typename Exception>
    static inline void opaque_throw(const Exception&);

private:
    type v_;
};

class TCommutativeVersion {
public:
    typedef TransactionTid::type type;
    typedef TransactionTid::signed_type signed_type;
    // we don't store thread ids and instead use those bits as a count of
    // how many threads own the lock (aka a read lock)
    static constexpr type lock_mask = TransactionTid::threadid_mask;
    static constexpr type user_bit = TransactionTid::user_bit;
    static constexpr type nonopaque_bit = TransactionTid::nonopaque_bit;

    TCommutativeVersion()
        : v_() {
    }
    TCommutativeVersion(type v)
        : v_(v) {
    }

    type value() const {
        return v_;
    }
    volatile type& value() {
        return v_;
    }

    bool is_locked() const {
        return (v_ & lock_mask) > 0;
    }
    unsigned num_locks() const {
        return v_ & lock_mask;
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
        while (1) {
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
            if (bool_cmpxchg(&v_, cur, new_v.v_ | cur_acquired))
                break;
            relax_fence();
        }
    }

    void inc_nonopaque_version() {
        assert(is_locked());
        // can't quite fetch and add here either because we need to both
        // increment and OR in the nonopaque_bit.
        while (1) {
            type cur = v_;
            fence();
            type new_v = (cur + TransactionTid::increment_value) | nonopaque_bit;
            release_fence();
            if (bool_cmpxchg(&v_, cur, new_v))
                break;
            relax_fence();
        }
    }

    bool check_version(TCommutativeVersion old_vers, bool locked_by_us = false) const {
        int lock = locked_by_us ? 1 : 0;
        return v_ == (old_vers.v_ | lock);
    }

    friend std::ostream& operator<<(std::ostream& w, TCommutativeVersion v) {
        // XXX: not super accurate for this but meh
        TransactionTid::print(v.value(), w);
        return w;
    }

private:
    type v_;
};

template <typename Exception>
inline void TVersion::opaque_throw(const Exception& exception) {
    throw exception;
}


class TObject {
public:
    virtual ~TObject() {}

    virtual bool lock(TransItem& item, Transaction& txn) = 0;
    virtual bool check_predicate(TransItem& item, Transaction& txn, bool committing) {
        (void) item, (void) txn, (void) committing;
        // always_assert(false);
        return false;
    }
    virtual bool check(TransItem& item, Transaction& txn) = 0;
    virtual void install(TransItem& item, Transaction& txn) = 0;
    virtual void unlock(TransItem& item) = 0;
    virtual void cleanup(TransItem& item, bool committed) {
        (void) item, (void) committed;
    }
    virtual void print(std::ostream& w, const TransItem& item) const;
};

typedef TObject Shared;
