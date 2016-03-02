#pragma once
#include <stdint.h>
#include <assert.h>
#include <iostream>

class Transaction;
class TransItem;

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
    // a version they unset the valid_bit, forcing any opacity check to be
    // hard (checking the full read set).
    static constexpr type valid_bit = type(0x40);
    static constexpr type user_bit = type(0x80);
    static constexpr type increment_value = type(0x400);

    static void lock_read(signed_type& v) {
        while (1) {
            signed_type vv = v;
            if (vv >= 0 && bool_cmpxchg(&v, vv, vv+1))
                break;
            relax_fence();
        }
        acquire_fence();
    }

    static void lock_write(signed_type& v) {
        while (1) {
            signed_type vv = v;
            if (vv == 0 && bool_cmpxchg(&v, vv, vv-1))
                break;
            relax_fence();
        }
        acquire_fence();
    }

    static void unlock_read(signed_type& v) {
        assert(__sync_fetch_and_add(&v, -1) > 0);
    }

    static void unlock_write(signed_type& v) {
        assert(__sync_fetch_and_add(&v, 1) < 0);
    }

    static bool is_locked(type v) {
        return v & lock_bit;
    }
    static bool is_locked_here(type v) {
        return (v & (lock_bit | threadid_mask)) == (lock_bit | TThread::id());
    }
    static bool is_locked_elsewhere(type v) {
        type m = v & (lock_bit | threadid_mask);
        return m != 0 && m != (lock_bit | TThread::id());
    }

    static bool try_lock(type& v) {
        type vv = v;
        return bool_cmpxchg(&v, vv & ~lock_bit, vv | lock_bit | TThread::id());
    }
    static void lock(type& v) {
        while (!try_lock(v))
            relax_fence();
        acquire_fence();
    }
    static void unlock(type& v) {
        assert(is_locked_here(v));
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
    static void set_version_unlock(type& v, type new_v) {
        assert(is_locked_here(v));
        assert(!is_locked(new_v) || is_locked_here(new_v));
        new_v &= ~(lock_bit | threadid_mask);
        release_fence();
        v = new_v;
    }

    static type next_invalid_version(type v) {
        return (v + increment_value) & ~valid_bit;
    }
    static void inc_invalid_version(type& v) {
        assert(is_locked_here(v));
        type new_v = (v + increment_value) & ~valid_bit;
        release_fence();
        v = new_v;
    }

    static bool check_version(type cur_vers, type old_vers) {
        assert(!is_locked_elsewhere(old_vers));
        // cur_vers allowed to be locked by us
        return cur_vers == old_vers || cur_vers == (old_vers | lock_bit | TThread::id());
    }
    static bool try_check_opacity(type start_tid, type v) {
        signed_type delta = start_tid - v;
        return (delta > 0 && !(delta & (lock_bit | valid_bit))) || !v;
    }
};

class TVersion {
public:
    typedef TransactionTid::type type;
    typedef TransactionTid::signed_type signed_type;

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

    bool is_locked() const {
        return TransactionTid::is_locked(v_);
    }
    bool is_locked_here() const {
        return TransactionTid::is_locked_here(v_);
    }
    bool is_locked_elsewhere() const {
        return TransactionTid::is_locked_elsewhere(v_);
    }

    bool try_lock() {
        return TransactionTid::try_lock(v_);
    }
    void lock() {
        TransactionTid::lock(v_);
    }
    void unlock() {
        TransactionTid::unlock(v_);
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

    void set_version(TVersion new_v) {
        TransactionTid::set_version(v_, new_v.v_);
    }
    void set_version_unlock(TVersion new_v) {
        TransactionTid::set_version_unlock(v_, new_v.v_);
    }

    bool check_version(TVersion old_vers) const {
        // XXX opacity
        return TransactionTid::check_version(v_, old_vers.v_);
    }

    friend std::ostream& operator<<(std::ostream& w, TVersion v) {
        return w << v.value();
    }

private:
    type v_;
};

class TNonopaqueVersion {
public:
    typedef TransactionTid::type type;
    typedef TransactionTid::signed_type signed_type;

    TNonopaqueVersion()
        : v_() {
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

    bool is_locked() const {
        return TransactionTid::is_locked(v_);
    }
    bool is_locked_here() const {
        return TransactionTid::is_locked_here(v_);
    }
    bool is_locked_elsewhere() const {
        return TransactionTid::is_locked_elsewhere(v_);
    }

    bool try_lock() {
        return TransactionTid::try_lock(v_);
    }
    void lock() {
        TransactionTid::lock(v_);
    }
    void unlock() {
        TransactionTid::unlock(v_);
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

    void set_version(TNonopaqueVersion new_v) {
        TransactionTid::set_version(v_, new_v.v_);
    }
    void set_version_unlock(TNonopaqueVersion new_v) {
        TransactionTid::set_version_unlock(v_, new_v.v_);
    }

    bool check_version(TNonopaqueVersion old_vers) const {
        // XXX opacity
        return TransactionTid::check_version(v_, old_vers.v_);
    }

    friend std::ostream& operator<<(std::ostream& w, TNonopaqueVersion v) {
        return w << v.value();
    }

private:
    type v_;
};

class TObject {
public:
    virtual ~TObject() {}

    virtual bool lock(TransItem& item, Transaction& txn) = 0;
    virtual bool check_predicate(TransItem& item, Transaction& txn, bool committing) {
        (void) item, (void) txn, (void) committing;
        always_assert(false);
        return false;
    }
    virtual bool check(const TransItem& item, const Transaction& txn) = 0;
    virtual void install(TransItem& item, const Transaction& txn) = 0;
    virtual void unlock(TransItem& item) = 0;
    virtual void cleanup(TransItem& item, bool committed) {
        (void) item, (void) committed;
    }
    virtual void print(std::ostream& w, const TransItem& item) const;
};

typedef TObject Shared;
