#pragma once

#include <stdint.h>
#include <assert.h>

class Transaction;
class TransItem;

class TransactionTid {
public:
    typedef uint64_t type;
    typedef int64_t signed_type;

    static constexpr type lock_bit = type(1);
    static constexpr type valid_bit = type(2);
    static constexpr type user_bit1 = type(4);
    static constexpr type user_bit2 = type(8);
    static constexpr type increment_value = type(32);

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
    
    static bool try_lock(type& v) {
        type vv = v;
        if (!is_locked(vv)) {
            return bool_cmpxchg(&v, vv, vv | lock_bit);
        }
        return false;
    }
    
    static void lock(type& v) {
        while (1) {
            type vv = v;
            if (!is_locked(vv) && bool_cmpxchg(&v, vv, vv | lock_bit))
                break;
            relax_fence();
        }
        acquire_fence();
    }
    static void unlock(type& v) {
        assert(is_locked(v));
        type new_v = v - lock_bit;
        release_fence();
        v = new_v;
    }
    static type unlocked(type v) {
      return v & ~lock_bit;
    }

    static void set_version(type& v, type new_v) {
        assert(is_locked(v));
        new_v |= lock_bit;
        release_fence();
        v = new_v;
    }
    static void inc_invalid_version(type& v) {
        assert(is_locked(v));
        type new_v = (v + increment_value) & ~valid_bit;
        release_fence();
        v = new_v;
    }
    static void atomic_inc_version(type& v) {
        fetch_and_add(&v, increment_value);
    }

    static bool same_version(type v1, type v2) {
        return (v1 ^ v2) <= lock_bit;
    }
    static bool try_check_opacity(type start_tid, type v) {
        signed_type delta = start_tid - v;
        return (delta > 0 && !(delta & (lock_bit | valid_bit))) || !v;
    }
};

class Shared {
public:
    virtual ~Shared() {}

    virtual bool check(const TransItem& item, const Transaction& t) = 0;
    virtual void lock(TransItem& item) = 0;
    virtual void unlock(TransItem& item) = 0;
    virtual void install(TransItem& item, const Transaction& t) = 0;

    virtual void cleanup(TransItem& item, bool committed) {
        (void) item, (void) committed;
    }
};
