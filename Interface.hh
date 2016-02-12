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
    // Used for data structures that don't use opacity. When they increment
    // a version they unset the valid_bit, forcing any opacity check to be
    // hard (checking the full read set).
    static constexpr type valid_bit = type(2);
    static constexpr type user_bit1 = type(4);
    static constexpr type user_bit2 = type(8);
    static constexpr type user_bit3 = type(16);
    static constexpr type user_bit4 = type(32);
    static constexpr type user_bit5 = type(64);
    static constexpr type user_shift = type(2);
    static constexpr type user_mask = type(user_bit1|user_bit2|user_bit3|user_bit4|user_bit5);
    static constexpr type increment_value = type(128);

    static type make_user_bits(type user_bits) {
        return (user_bits << user_shift) & user_mask;
    }

    static type user_bits(type v) {
        return (v & user_mask) >> user_shift;
    }

    static type add_user_bits(type v, type user_bits) {
        return (v & ~user_mask)  | make_user_bits(user_bits);
    }

    static bool is_locked(type v) {
        return v & lock_bit;
    }
    
    static bool try_lock(type& v, type user_bits = 0) {
        type vv = v;
        if (!is_locked(vv)) {
            return bool_cmpxchg(&v, vv, vv | lock_bit | make_user_bits(user_bits));
        }
        return false;
    }
    
    static void lock(type& v, type user_bits = 0) {
        while (1) {
            type vv = v;
            if (!is_locked(vv) && bool_cmpxchg(&v, vv, vv | lock_bit | make_user_bits(user_bits)))
                break;
            relax_fence();
        }
        acquire_fence();
    }
    static void unlock(type& v) {
        assert(is_locked(v));
        type new_v = (v - lock_bit) & ~user_mask;
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

    virtual void lock(TransItem& item) = 0;
    virtual bool check_predicate(TransItem& item, Transaction& t) {
        (void) item, (void) t;
        always_assert(false);
        return false;
    }
    virtual bool check(const TransItem& item, const Transaction& t) = 0;
    virtual void install(TransItem& item, const Transaction& t) = 0;
    virtual void cleanup(TransItem& item, bool committed) {
        (void) item, (void) committed;
    }
    virtual void print(FILE* f, const TransItem& item) const;
};
