#pragma once

#include <cstdint>
#include <cassert>
#include <iostream>

#include "compiler.hh"
#include "TransItem.hh"
#include "TThread.hh"

class TransactionTid {
public:
    typedef WideTid::single_type type;
    typedef WideTid::signed_single_type signed_type;

    // Common layout definition

    // |-----VALUE-----|O|D|U|N|L|--MASK--|
    //       52 bits    1 1 1 1 1  7 bits

    static constexpr signed_type mask_width = 7;

    // bits holding thread id of the thread holding the exclusive lock
    static constexpr type threadid_mask = type(0x7F);
    // the exclusive lock bit, used for write locks
    static constexpr type lock_bit = type(0x1 << mask_width);
    // Used for data structures that don't use opacity. When they increment
    // a version they set the nonopaque_bit, forcing any opacity check to be
    // hard (checking the full read set)
    static constexpr type nonopaque_bit = type(0x1 << (mask_width + 1));
    // Reserved user bit, usually used for eager write-write conflict detection
    // in various data types (the so-called "invalid bit")
    static constexpr type user_bit = type(0x1 << (mask_width + 2));
    // Dirty bit, set while the version is being modified
    static constexpr type dirty_bit = type(0x1 << (mask_width + 3));
    // Bit signaling optimistic concurrency control for readers
    // used in adaptive reader/write lock version
    static constexpr type opt_bit = type(0x1 << (mask_width + 4));
    // start of te actual version (Tid) value
    static constexpr type increment_value = type(0x1 << (mask_width + 5));
    // Max Tid value, absent any other bits
    static constexpr type max_value = type((~0x0ULL) << (mask_width + 5));

    static bool is_locked(type v) {
        return (v & lock_bit) != 0;
    }
    static bool is_dirty(type v) {
        return (v & dirty_bit) != 0;
    }
    static bool is_optimistic(type v) {
        return (v & opt_bit) != 0;
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
    static type try_lock_val(type& v) {
        type vv = v;
        type expected = vv & ~lock_bit;
        type desired = vv | lock_bit | TThread::id();
        type old = cmpxchg(&v, expected, desired);
        if (old == expected)
            return desired;
        else
            return old;
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
    static void unlock_dirty(type& v) {
        type new_v = v & ~(lock_bit | dirty_bit | threadid_mask);
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
    static void set_version_unlock_dirty(type&v, type new_v) {
        new_v &= ~(lock_bit | dirty_bit | threadid_mask);
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
        if (v & opt_bit)
            w << "O";
        if (v & dirty_bit)
            w << "D";
        if (v & user_bit)
            w << "U";
        if (v & nonopaque_bit)
            w << "!";
        if (v & lock_bit)
            w << "L";
        // always print out mask
        w << ".0x" << std::hex << (v & threadid_mask);
        w.flags(f);
    }
};

static constexpr TransactionTid::type initialized_tid = TransactionTid::increment_value;

// This is the interface of all concurrency control "versions numbers" in STO
template <typename VersImpl>
class VersionBase {
public:
    typedef TransactionTid::type type;

    VersionBase() : v_(initialized_tid) {}
    explicit VersionBase(type v) : v_(v) {}

    type value() const {
        return v_;
    }
    volatile type& value() {
        return v_;
    }

    // Interface exposed to STO the commit protocol (cp)
    // Logical try-lock/unlock at commit time
    bool cp_try_lock(TransItem& item, int thread_id) {
        return impl().cp_try_lock_impl(item, thread_id);
    }
    void cp_unlock(TransItem& item) {
        impl().cp_unlock_impl(item);
    }

    // access/compute commit tid (used by GV/TicToc)
    static type& cp_access_tid(Transaction& txn) {
        return VersImpl::cp_access_tid_impl(txn);
    }
    type cp_commit_tid(Transaction& txn) {
        return impl().cp_commit_tid_impl(txn);
    }

    // Logical updates to transaction version number/timestamps
    void cp_set_version_unlock(type new_v) {
        impl().cp_set_version_unlock_impl(new_v);
    }
    void cp_set_version(type new_v) {
        impl().cp_set_version_impl(new_v);
    }

    bool cp_check_version(Transaction& txn, TransItem& item) {
        return impl().cp_check_version_impl(txn, item);
    }

    // Interface exposed to STO runtime tracking set management (TItem)
    bool acquire_write(TransItem& item) {
        return impl().acquire_write_impl(item);
    }
    template <typename T>
    bool acquire_write(TransItem& item, const T& wdata) {
        return impl().acquire_write_impl(item, wdata);
    }
    template <typename T>
    bool acquire_write(TransItem& item, T&& wdata) {
        return impl().acquire_write_impl(item, wdata);
    }
    template <typename T, typename... Args>
    bool acquire_write(TransItem& item, Args&&... args) {
        return impl().acquire_write_impl(item, std::forward<Args>(args)...);
    }

    bool observe_read(TransItem& item) {
        return observe_read(item, true);
    }
    bool observe_read(TransItem& item, bool add_read) {
        return impl().observe_read_impl(item, add_read);
    }
    bool observe_read(TransItem& item, VersImpl& snapshot) {
        return impl().observe_read_impl(item, snapshot);
    }

    // Optional interface exposed to data types so that things like bucket/node
    // version are easier to handle
    void inc_nonopaque() {
        impl().inc_nonopaque_impl();
    }
    void lock_exclusive() {
        impl().lock_exclusive_impl();
    }
    void unlock_exclusive() {
        impl().unlock_exclusive_impl();
    }
    type unlocked_value() const {
        return impl().unlocked_value_impl();
    }

    void compute_commit_ts_step(type& commit_ts, bool is_write) {
        impl().compute_commit_ts_step_impl(commit_ts, is_write);
    }

protected:
    type v_;

    VersImpl& impl() {
        return static_cast<VersImpl&>(*this);
    }
    const VersImpl& impl() const {
        return static_cast<const VersImpl&>(*this);
    }
};

template <typename VersImpl>
class BasicVersion : public VersionBase<VersImpl> {
public:
    typedef TransactionTid::type type;

    using Base = VersionBase<VersImpl>;
    using Base::v_;

    BasicVersion() = default;
    explicit BasicVersion(type v)
            : Base(v) {}

    bool operator==(BasicVersion x) const {
        return v_ == x.v_;
    }
    bool operator!=(BasicVersion x) const {
        return v_ != x.v_;
    }
    BasicVersion operator|(BasicVersion x) const {
        return BasicVersion(v_ | x.v_);
    }
    type operator&(TransactionTid::type x) const {
        return (v_ & x);
    }

    bool bool_cmpxchg(BasicVersion expected, BasicVersion desired) {
        return ::bool_cmpxchg(&v_, expected.v_, desired.v_);
    }

    //inline type snapshot(const TransItem& item, const Transaction& txn);
    //inline type snapshot(TransProxy& item);

    bool cp_try_lock_impl(TransItem& item, int threadid) {
        (void)item;
        return TransactionTid::try_lock(v_, threadid);
    }
    void cp_unlock_impl(TransItem& item) {
        (void)item;
        TransactionTid::unlock(v_);
    }
    void cp_set_version_unlock_impl(type new_v) {
        TransactionTid::set_version_unlock(v_, new_v);
    }
    void cp_set_version_impl(type new_v) {
        TransactionTid::set_version(v_, new_v);
    }

    void compute_commit_ts_step_impl(type& ts, bool is_write) {
        (void)ts; (void)is_write;
    }

    void inc_nonopaque_impl() {
        TransactionTid::inc_nonopaque_version(v_);
    }
    void lock_exclusive_impl() {
        TransactionTid::lock(v_);
    }
    void unlock_exclusive_impl() {
        TransactionTid::unlock(v_);
    }
    type unlocked_value_impl() const {
        return TransactionTid::unlocked(v_);
    }

    inline bool acquire_write_impl(TransItem& item);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, const T& wdata);
    template <typename T>
    inline bool acquire_write_impl(TransItem& item, T&& wdata);
    template <typename T, typename... Args>
    inline bool acquire_write_impl(TransItem& item, Args&&... args);

    friend std::ostream& operator<<(std::ostream& w, BasicVersion v) {
        TransactionTid::print(v.value(), w);
        return w;
    }

    template <typename Exception>
    static inline void opaque_throw(const Exception& exception) {
        throw exception;
    }

    bool is_locked() const {
        return TransactionTid::is_locked(v_);
    }
    bool is_locked_here() const {
        return TransactionTid::is_locked_here(v_);
    }
    bool is_locked_here(int here) const {
        return TransactionTid::is_locked_here(v_, here);
    }
    bool is_locked_elsewhere() const {
        return TransactionTid::is_locked_elsewhere(v_);
    }
    bool is_locked_elsewhere(int here) const {
        return TransactionTid::is_locked_elsewhere(v_, here);
    }
    bool is_dirty() const {
        return TransactionTid::is_dirty(v_);
    }
    void set_nonopaque() {
        TransactionTid::set_nonopaque(v_);
    }

    bool check_version(BasicVersion old_vers) const {
        return TransactionTid::check_version(v_, old_vers.v_);
    }
    bool check_version(BasicVersion old_vers, int here) const {
        return TransactionTid::check_version(v_, old_vers.v_, here);
    }
};
