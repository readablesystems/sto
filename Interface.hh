#pragma once
#include <stdint.h>
#include <assert.h>
#include <iostream>
#include <bitset>

#include "config.h"
#include "compiler.hh"

class Transaction;
class TransItem;
class TransProxy;

class TThread {
    static __thread int the_id;
public:
    static __thread Transaction* txn;

    static int id() {
        return the_id;
    }
    static void set_id(int id) {
        assert(id >= 0 && id < 32);
        the_id = id;
    }
};

class TransactionTid {
public:
    typedef uint64_t type;
    typedef int64_t signed_type;

    static constexpr type threadid_mask = type(0x1F);
    static constexpr type lock_bit = type(0x20);
    // Used for data structures that don't use opacity. When they increment
    // a version they set the nonopaque_bit, forcing any opacity check to be
    // hard (checking the full read set).
    static constexpr type nonopaque_bit = type(0x40);
    static constexpr type user_bit = type(0x80);
    static constexpr type increment_value = type(0x400);

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


    static bool is_locked(type v) {
        return v & lock_bit;
    }
    static bool is_locked_here(type v) {
        return (v & (lock_bit | threadid_mask)) == (lock_bit | TThread::id());
    }
    static bool is_locked_here(type v, int here) {
        return (v & (lock_bit | threadid_mask)) == (lock_bit | here);
    }
    static bool is_locked_elsewhere(type v) {
        type m = v & (lock_bit | threadid_mask);
        return m != 0 && m != (lock_bit | TThread::id());
    }
    static bool is_locked_elsewhere(type v, int here) {
        type m = v & (lock_bit | threadid_mask);
        return m != 0 && m != (lock_bit | here);
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
        assert(is_locked_here(v));
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
        assert(is_locked_here(v));
        assert(!is_locked(new_v) || is_locked_here(new_v));
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
    static type next_unflagged_nonopaque_version(type v) {
        return ((v + increment_value) & ~(increment_value - 1)) | nonopaque_bit;
    }
    static void inc_nonopaque_version(type& v) {
        assert(is_locked_here(v));
        type new_v = (v + increment_value) | nonopaque_bit;
        release_fence();
        v = new_v;
    }

    static bool check_version(type cur_vers, type old_vers) {
        assert(!is_locked_elsewhere(old_vers));
        // cur_vers allowed to be locked by us
        return cur_vers == old_vers || cur_vers == (old_vers | lock_bit | TThread::id());
    }
    static bool check_version(type cur_vers, type old_vers, int here) {
        assert(!is_locked_elsewhere(old_vers));
        // cur_vers allowed to be locked by us
        return cur_vers == old_vers || cur_vers == (old_vers | lock_bit | here);
    }
    static bool try_check_opacity(type start_tid, type v) {
        signed_type delta = start_tid - v;
        return delta > 0 && !(v & (lock_bit | nonopaque_bit));
    }

    static void print(type v, std::ostream& w) {
        w << "Binary: " << std::bitset<64> (v) << std::endl;
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
	//        always_assert(false);
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
