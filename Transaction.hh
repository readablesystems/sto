#pragma once

#include "config.h"
#include "compiler.hh"
#include "small_vector.hh"
#include "TRcu.hh"
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <unistd.h>
#include <iostream>
#include <sstream>

#ifndef STO_PROFILE_COUNTERS
#define STO_PROFILE_COUNTERS 0
#endif

#ifndef STO_DEBUG_HASH_COLLISIONS
#define STO_DEBUG_HASH_COLLISIONS 0
#endif
#ifndef STO_DEBUG_HASH_COLLISIONS_FRACTION
#define STO_DEBUG_HASH_COLLISIONS_FRACTION 0.00001
#endif

#ifndef STO_DEBUG_ABORTS
#define STO_DEBUG_ABORTS 0
#endif
#ifndef STO_DEBUG_ABORTS_FRACTION
#define STO_DEBUG_ABORTS_FRACTION 0.0001
#endif

#ifndef STO_SORT_WRITESET
#define STO_SORT_WRITESET 0
#endif

#define CONSISTENCY_CHECK 0
#define ASSERT_TX_SIZE 0
#define TRANSACTION_HASHTABLE 1

#if ASSERT_TX_SIZE
#if STO_PROFILE_COUNTERS > 1
#    define TX_SIZE_LIMIT 20000
#    include <cassert>
#else
#    error "ASSERT_TX_SIZE requires STO_PROFILE_COUNTERS > 1!"
#endif
#endif

#include "config.h"

#define MAX_THREADS 32

// TRANSACTION macros that can be used to wrap transactional code
#define TRANSACTION                               \
    do {                                          \
        TransactionLoopGuard __txn_guard;         \
        while (1) {                               \
            __txn_guard.start();                  \
            try {
#define RETRY(retry)                              \
                if (__txn_guard.try_commit())     \
                    break;                        \
            } catch (Transaction::Abort e) {      \
            }                                     \
            if (!(retry))                         \
                throw Transaction::Abort();       \
        }                                         \
    } while (0)

// transaction performance counters
enum txp {
    // all logging levels
    txp_total_aborts = 0,
    txp_total_starts,
    txp_commit_time_aborts,
    txp_max_set,
    txp_hco,
    txp_hco_lock,
    txp_hco_invalid,
    txp_hco_abort,
    // STO_PROFILE_COUNTERS > 1 only
    txp_total_n,
    txp_total_r,
    txp_total_w,
    txp_max_transbuffer,
    txp_total_transbuffer,
    txp_push_abort,
    txp_pop_abort,
    txp_total_check_read,
    txp_total_check_predicate,
    txp_hash_find,
    txp_hash_collision,
    txp_hash_collision2,
    txp_total_searched,
#if !STO_PROFILE_COUNTERS
    txp_count = 0
#elif STO_PROFILE_COUNTERS == 1
    txp_count = txp_hco_abort + 1
#else
    txp_count
#endif
};
typedef uint64_t txp_counter_type;

inline constexpr bool txp_is_max(unsigned p) {
    return p == txp_max_set || p == txp_max_transbuffer;
}

template <unsigned P, unsigned N, bool Less = (P < N)> struct txp_helper;
template <unsigned P, unsigned N> struct txp_helper<P, N, true> {
    static bool counter_exists(unsigned p) {
        return p < N;
    }
    static void account_array(txp_counter_type* p, txp_counter_type v) {
        if (txp_is_max(P))
            p[P] = std::max(p[P], v);
        else
            p[P] += v;
    }
};
template <unsigned P, unsigned N> struct txp_helper<P, N, false> {
    static bool counter_exists(unsigned) {
        return false;
    }
    static void account_array(txp_counter_type*, txp_counter_type) {
    }
};

struct txp_counters {
    txp_counter_type p_[txp_count];
    txp_counters() {
        for (unsigned i = 0; i != txp_count; ++i)
            p_[i] = 0;
    }
    unsigned long long p(int p) {
        return txp_helper<0, txp_count>::counter_exists(p) ? p_[p] : 0;
    }
    void reset() {
        for (int i = 0; i != txp_count; ++i)
            p_[i] = 0;
    }
};


#include "Interface.hh"
#include "TransItem.hh"

#define INIT_SET_SIZE 512

void reportPerf();
#define STO_SHUTDOWN() reportPerf()

struct __attribute__((aligned(128))) threadinfo_t {
    using epoch_type = TRcuSet::epoch_type;
    epoch_type epoch;
    TRcuSet rcu_set;
    // XXX(NH): these should be vectors so multiple data structures can register
    // callbacks for these
    std::function<void(void)> trans_start_callback;
    std::function<void(void)> trans_end_callback;
    txp_counters p_;
    threadinfo_t()
        : epoch(0) {
    }
};


class Transaction {
public:
    static constexpr unsigned hash_size = 1024;
    static constexpr unsigned hash_step = 5;
    using epoch_type = TRcuSet::epoch_type;
    using signed_epoch_type = TRcuSet::signed_epoch_type;

    static threadinfo_t tinfo[MAX_THREADS];
    static struct epoch_state {
        epoch_type global_epoch; // != 0
        epoch_type active_epoch; // no thread is before this epoch
        TransactionTid::type recent_tid;
        bool run;
    } global_epochs;
    typedef TransactionTid::type tid_type;
private:
    static TransactionTid::type _TID;
public:

    static std::function<void(threadinfo_t::epoch_type)> epoch_advance_callback;

    static txp_counters txp_counters_combined() {
        txp_counters out;
        for (int i = 0; i != MAX_THREADS; ++i)
            for (int p = 0; p != txp_count; ++p) {
                if (txp_is_max(p))
                    out.p_[p] = std::max(out.p_[p], tinfo[i].p_.p_[p]);
                else
                    out.p_[p] += tinfo[i].p_.p_[p];
            }
        return out;
    }

    static void print_stats();

    static void clear_stats() {
        for (int i = 0; i != MAX_THREADS; ++i)
            tinfo[i].p_.reset();
    }

    static void* epoch_advancer(void*);
    template <typename T>
    static void rcu_delete(T* x) {
        auto& thr = tinfo[TThread::id()];
        thr.rcu_set.add(thr.epoch, ObjectDestroyer<T>::destroy_and_free, x);
    }
    template <typename T>
    static void rcu_delete_array(T* x) {
        auto& thr = tinfo[TThread::id()];
        thr.rcu_set.add(thr.epoch, ObjectDestroyer<T>::destroy_and_free_array, x);
    }
    static void rcu_free(void* ptr) {
        auto& thr = tinfo[TThread::id()];
        thr.rcu_set.add(thr.epoch, ::free, ptr);
    }
    static void rcu_call(void (*function)(void*), void* argument) {
        auto& thr = tinfo[TThread::id()];
        thr.rcu_set.add(thr.epoch, function, argument);
    }
    static void rcu_quiesce() {
        tinfo[TThread::id()].epoch = 0;
    }

#if STO_PROFILE_COUNTERS
    template <unsigned P> static void txp_account(txp_counter_type n) {
        txp_helper<P, txp_count>::account_array(tinfo[TThread::id()].p_.p_, n);
    }
#else
    template <unsigned P> static void txp_account(txp_counter_type) {
    }
#endif

#define TXP_INCREMENT(p) Transaction::txp_account<(p)>(1)
#define TXP_ACCOUNT(p, n) Transaction::txp_account<(p)>((n))


  private:
    Transaction()
        : hash_base_(32768), lrng_state_(12897), is_test_(false) {
        start();
    }

    struct testing_type {};
    static testing_type testing;

    Transaction(const testing_type&)
        : hash_base_(32768), lrng_state_(12897), is_test_(true) {
        start();
    }

    struct uninitialized {};
    Transaction(uninitialized)
        : hash_base_(32768), lrng_state_(12897), is_test_(false) {
        state_ = s_aborted;
    }

    ~Transaction() { /* XXX should really be private */
        if (in_progress())
            silent_abort();
    }

    // reset data so we can be reused for another transaction
    void start() {
        threadinfo_t& thr = tinfo[TThread::id()];
        //if (isAborted_
        //   && tinfo[TThread::id()].p(txp_total_aborts) % 0x10000 == 0xFFFF)
           //print_stats();
        thr.epoch = global_epochs.global_epoch;
        thr.rcu_set.clean_until(global_epochs.active_epoch);
        if (thr.trans_start_callback)
            thr.trans_start_callback();
        hash_base_ += transSet_.size() + 1;
        transSet_.clear();
#if TRANSACTION_HASHTABLE
        if (hash_base_ >= 32768) {
            memset(hashtable_, 0, sizeof(hashtable_));
            hash_base_ = 0;
        }
#endif
        any_writes_ = may_duplicate_items_ = false;
        first_write_ = 0;
        start_tid_ = commit_tid_ = 0;
        buf_.clear();
#if STO_DEBUG_ABORTS
        abort_item_ = nullptr;
        abort_reason_ = nullptr;
#endif
        TXP_INCREMENT(txp_total_starts);
        state_ = s_in_progress;
    }

#if TRANSACTION_HASHTABLE
    static int hash(const TObject* obj, void* key) {
        auto n = reinterpret_cast<uintptr_t>(key) + 0x4000000;
        n += -uintptr_t(n < 0x8000000) & (reinterpret_cast<uintptr_t>(obj) >> 4);
        //2654435761
        return (n + (n >> 16) * 9) % hash_size;
    }
#endif

    TransItem* allocate_item(const TObject* obj, void* xkey) {
        transSet_.emplace_back(const_cast<TObject*>(obj), xkey);
#if TRANSACTION_HASHTABLE
        unsigned hi = hash(obj, xkey);
# if TRANSACTION_HASHTABLE > 1
        if (hashtable_[hi] > hash_base_)
            hi = (hi + hash_step) % hash_size;
# endif
        if (hashtable_[hi] <= hash_base_)
            hashtable_[hi] = hash_base_ + transSet_.size();
#endif
        return &transSet_.back();
    }

public:
    // adds item for a key that is known to be new (must NOT exist in the set)
    template <typename T>
    TransProxy new_item(const TObject* obj, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        return TransProxy(*this, allocate_item(obj, xkey) - transSet_.begin());
    }

    // adds item without checking its presence in the array
    template <typename T>
    TransProxy fresh_item(const TObject* obj, T key) {
        may_duplicate_items_ = !transSet_.empty();
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        return TransProxy(*this, allocate_item(obj, xkey) - transSet_.begin());
    }

    // tries to find an existing item with this key, otherwise adds it
    template <typename T>
    TransProxy item(const TObject* obj, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        TransItem* ti = find_item(const_cast<TObject*>(obj), xkey);
        if (!ti)
            ti = allocate_item(obj, xkey);
        return TransProxy(*this, ti - transSet_.begin());
    }

    // gets an item that is intended to be read only. this method essentially allows for duplicate items
    // in the set in some cases
    template <typename T>
    TransProxy read_item(const TObject* obj, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        TransItem* ti = nullptr;
        if (any_writes_)
            ti = find_item(const_cast<TObject*>(obj), xkey);
        else
            may_duplicate_items_ = !transSet_.empty();
        if (!ti)
            ti = allocate_item(obj, xkey);
        return TransProxy(*this, ti - transSet_.begin());
    }

    template <typename T>
    OptionalTransProxy check_item(const TObject* obj, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        TransItem* ti = find_item(const_cast<TObject*>(obj), xkey);
        return OptionalTransProxy(*this, ti ? ti - transSet_.begin() : unsigned(-1));
    }

private:
    // tries to find an existing item with this key, returns NULL if not found
    TransItem* find_item(TObject* obj, void* xkey) {
#if TRANSACTION_HASHTABLE
        TXP_INCREMENT(txp_hash_find);
        unsigned hi = hash(obj, xkey);
        for (int steps = 0; steps < TRANSACTION_HASHTABLE; ++steps) {
            if (hashtable_[hi] <= hash_base_)
                return nullptr;
            TransItem* ti = &transSet_[hashtable_[hi] - hash_base_ - 1];
            if (ti->owner() == obj && ti->key_ == xkey)
                return ti;
            if (!steps) {
                TXP_INCREMENT(txp_hash_collision);
# if STO_DEBUG_HASH_COLLISIONS
                if (local_random() <= uint32_t(0xFFFFFFFF * STO_DEBUG_HASH_COLLISIONS_FRACTION)) {
                    std::ostringstream buf;
                    TransItem fake_item(obj, xkey);
                    buf << "$ STO hash collision: search " << fake_item << ", find " << *ti << '\n';
                    std::cerr << buf.str();
                }
# endif
            } else
                TXP_INCREMENT(txp_hash_collision2);
            hi = (hi + hash_step) % hash_size;
        }
#endif
        for (auto it = transSet_.begin(); it != transSet_.end(); ++it) {
            TXP_INCREMENT(txp_total_searched);
            if (it->owner() == obj && it->key_ == xkey)
                return &*it;
        }
        return nullptr;
    }

    bool preceding_read_exists(TransItem& item) const {
        if (may_duplicate_items_)
            for (auto it = transSet_.begin(); it != &item; ++it)
                if (it->owner() == item.owner() && it->key_ == item.key_)
                    return true;
        return false;
    }

#if STO_DEBUG_ABORTS
    void mark_abort_because(TransItem* item, const char* reason) const {
        abort_item_ = item;
        abort_reason_ = reason;
    }
#else
    void mark_abort_because(TransItem*, const char*) const {
    }
#endif

    void abort_because(TransItem& item, const char* reason) {
        mark_abort_because(&item, reason);
        abort();
    }

public:
    void silent_abort() {
        if (in_progress())
            stop(false);
    }

    void abort() {
        silent_abort();
        throw Abort();
    }

    bool try_commit();

    void commit() {
        if (!try_commit())
            throw Abort();
    }

    bool aborted() {
        return state_ == s_aborted;
    }

    bool in_progress() {
        return state_ < s_aborted;
    }

    // opacity checking
    // These function will eventually help us track the commit TID when we
    // have no opacity, or for GV7 opacity.
    bool try_lock(TransItem& item, TVersion& vers) {
        return try_lock(item, const_cast<TransactionTid::type&>(vers.value()));
    }
    bool try_lock(TransItem& item, TNonopaqueVersion& vers) {
        return try_lock(item, const_cast<TransactionTid::type&>(vers.value()));
    }
    bool try_lock(TransItem& item, TransactionTid::type& vers) {
#if STO_SORT_WRITESET
        TransactionTid::lock(vers);
        return true;
#else
        // This function will eventually help us track the commit TID when we
        // have no opacity, or for GV7 opacity.
        int i = 0;
        while (1) {
            if (TransactionTid::try_lock(vers))
                return true;
            if (i > (!item.has_read() << 3))
                return false;
            ++i;
            relax_fence();
        }
#endif
    }

    void check_opacity(TransItem& item, TransactionTid::type v) {
        assert(state_ <= s_committing);
        if (!start_tid_)
            start_tid_ = _TID;
        if (!TransactionTid::try_check_opacity(start_tid_, v))
            hard_check_opacity(&item, v);
    }

    void check_opacity(TransactionTid::type v) {
        assert(state_ <= s_committing);
        if (!start_tid_)
            start_tid_ = _TID;
        if (!TransactionTid::try_check_opacity(start_tid_, v))
            hard_check_opacity(nullptr, v);
    }

    void check_opacity() {
        check_opacity(_TID);
    }

    // committing
    tid_type commit_tid() const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        if (!commit_tid_)
            commit_tid_ = fetch_and_add(&_TID, TransactionTid::increment_value);
        return commit_tid_;
    }
    void set_version(TVersion& vers) const {
        vers.set_version(commit_tid());
    }
    void set_version_unlock(TVersion& vers, TransItem& item) const {
        vers.set_version_unlock(commit_tid());
        item.clear_needs_unlock();
    }
    void set_version(TNonopaqueVersion& vers) const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        tid_type v = commit_tid_ ? commit_tid_ : TransactionTid::next_nonopaque_version(vers.value());
        vers.set_version(v);
    }
    void set_version_unlock(TNonopaqueVersion& vers, TransItem& item) const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        tid_type v = commit_tid_ ? commit_tid_ : TransactionTid::next_nonopaque_version(vers.value());
        vers.set_version_unlock(v);
        item.clear_needs_unlock();
    }

    static const char* state_name(int state);
    void print() const;
    void print(std::ostream& w) const;

    class Abort {};

    uint32_t local_random() const {
        lrng_state_ = lrng_state_ * 1664525 + 1013904223;
        return lrng_state_;
    }
    void local_srandom(uint32_t state) {
        lrng_state_ = state;
    }

private:
    enum {
        s_in_progress = 0, s_committing = 1, s_committing_locked = 2,
        s_aborted = 3, s_committed = 4
    };

    uint16_t hash_base_;
    uint16_t first_write_;
    uint8_t state_;
    bool any_writes_;
    bool may_duplicate_items_;
    small_vector<TransItem, INIT_SET_SIZE> transSet_;
    mutable tid_type start_tid_;
    mutable tid_type commit_tid_;
    TransactionBuffer buf_;
    mutable uint32_t lrng_state_;
    mutable TransItem* abort_item_;
    mutable const char* abort_reason_;
    bool is_test_;
#if TRANSACTION_HASHTABLE
    uint16_t hashtable_[hash_size];
#endif

    void hard_check_opacity(TransItem* item, TransactionTid::type t);
    void stop(bool committed);

    friend class TransProxy;
    friend class TransItem;
    friend class Sto;
    friend class TestTransaction;
};


class Sto {
public:
    static Transaction* transaction() {
        if (!TThread::txn)
            TThread::txn = new Transaction(Transaction::uninitialized());
        return TThread::txn;
    }

    static void start_transaction() {
        Transaction* t = transaction();
        always_assert(!t->in_progress());
        t->start();
    }

    static bool in_progress() {
        return TThread::txn && TThread::txn->in_progress();
    }

    static void abort() {
        always_assert(in_progress());
        TThread::txn->abort();
    }

    static void silent_abort() {
        if (in_progress())
            TThread::txn->silent_abort();
    }

    template <typename T>
    static TransProxy item(const TObject* s, T key) {
        always_assert(in_progress());
        return TThread::txn->item(s, key);
    }

    static void check_opacity(TransactionTid::type t) {
        always_assert(in_progress());
        TThread::txn->check_opacity(t);
    }

    static void check_opacity() {
        always_assert(in_progress());
        TThread::txn->check_opacity();
    }

    template <typename T>
    static OptionalTransProxy check_item(const TObject* s, T key) {
        always_assert(in_progress());
        return TThread::txn->check_item(s, key);
    }

    template <typename T>
    static TransProxy new_item(const TObject* s, T key) {
        always_assert(in_progress());
        return TThread::txn->new_item(s, key);
    }

    template <typename T>
    static TransProxy read_item(const TObject* s, T key) {
        always_assert(in_progress());
        return TThread::txn->read_item(s, key);
    }

    template <typename T>
    static TransProxy fresh_item(const TObject* s, T key) {
        always_assert(in_progress());
        return TThread::txn->fresh_item(s, key);
    }

    static void commit() {
        always_assert(in_progress());
        TThread::txn->commit();
    }

    static bool try_commit() {
        always_assert(in_progress());
        return TThread::txn->try_commit();
    }

    static TransactionTid::type commit_tid() {
        return TThread::txn->commit_tid();
    }

    static TransactionTid::type recent_tid() {
        return Transaction::global_epochs.recent_tid;
    }

    static TransactionTid::type initialized_tid() {
        return TransactionTid::increment_value;
    }
};

class TestTransaction {
public:
    TestTransaction(int threadid)
        : t_(Transaction::testing), base_(TThread::txn), threadid_(threadid) {
        use();
    }
    ~TestTransaction() {
        if (base_ && !base_->is_test_)
            TThread::txn = base_;
    }
    void use() {
        TThread::txn = &t_;
        TThread::set_id(threadid_);
    }
    void print(std::ostream& w) const {
        t_.print(w);
    }
    bool try_commit() {
        use();
        return t_.try_commit();
    }
private:
    Transaction t_;
    Transaction* base_;
    int threadid_;
};

class TransactionGuard {
  public:
    TransactionGuard() {
        Sto::start_transaction();
    }
    ~TransactionGuard() {
        Sto::commit();
    }
    typedef void (TransactionGuard::* unspecified_bool_type)(std::ostream&) const;
    operator unspecified_bool_type() const {
        return &TransactionGuard::print;
    }
    void print(std::ostream& w) const {
        TThread::txn->print(w);
    }
};

class TransactionLoopGuard {
  public:
    TransactionLoopGuard() {
    }
    ~TransactionLoopGuard() {
        if (TThread::txn->in_progress())
            TThread::txn->silent_abort();
    }
    void start() {
        Sto::start_transaction();
    }
    bool try_commit() {
        return TThread::txn->try_commit();
    }
};


inline TransProxy::TransProxy(Transaction& t, TransItem& item)
    : t_(&t), idx_(&item - t.transSet_.begin()) {
    assert(&t == TThread::txn);
}

inline TransItem& TransProxy::item() const {
    return t()->transSet_[idx_];
}

template <typename T>
inline TransProxy& TransProxy::add_read(T rdata) {
    assert(!has_stash());
    if (!has_read()) {
        item().__or_flags(TransItem::read_bit);
        item().rdata_ = Packer<T>::pack(t()->buf_, std::move(rdata));
    }
    return *this;
}

inline TransProxy& TransProxy::observe(TVersion version, bool add_read) {
    assert(!has_stash());
    if (version.is_locked_elsewhere())
        t()->abort_because(item(), "locked");
    t()->check_opacity(item(), version.value());
    if (add_read && !has_read()) {
        item().__or_flags(TransItem::read_bit);
        item().rdata_ = Packer<TVersion>::pack(t()->buf_, std::move(version));
    }
    return *this;
}

inline TransProxy& TransProxy::observe(TNonopaqueVersion version, bool add_read) {
    assert(!has_stash());
    if (version.is_locked_elsewhere())
        t()->abort_because(item(), "locked");
    if (add_read && !has_read()) {
        item().__or_flags(TransItem::read_bit);
        item().rdata_ = Packer<TNonopaqueVersion>::pack(t()->buf_, std::move(version));
    }
    return *this;
}

inline TransProxy& TransProxy::observe(TVersion version) {
    return observe(version, true);
}

inline TransProxy& TransProxy::observe(TNonopaqueVersion version) {
    return observe(version, true);
}

inline TransProxy& TransProxy::observe_opacity(TVersion version) {
    return observe(version, false);
}

inline TransProxy& TransProxy::observe_opacity(TNonopaqueVersion version) {
    return observe(version, false);
}

template <typename T>
inline TransProxy& TransProxy::update_read(T old_rdata, T new_rdata) {
    if (has_read() && this->read_value<T>() == old_rdata)
        item().rdata_ = Packer<T>::repack(t()->buf_, item().rdata_, new_rdata);
    return *this;
}


template <typename T>
inline TransProxy& TransProxy::set_predicate(T pdata) {
    assert(!has_read());
    item().__or_flags(TransItem::predicate_bit);
    item().rdata_ = Packer<T>::pack(t()->buf_, std::move(pdata));
    return *this;
}

template <typename T>
inline T& TransProxy::predicate_value(T default_pdata) {
    assert(!has_read());
    if (!has_predicate())
        set_predicate(default_pdata);
    return this->template predicate_value<T>();
}

inline TransProxy& TransProxy::add_write() {
    if (!has_write()) {
        item().__or_flags(TransItem::write_bit);
        t()->any_writes_ = true;
    }
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::add_write(const T& wdata) {
    return add_write<T, const T&>(wdata);
}

template <typename T>
inline TransProxy& TransProxy::add_write(T&& wdata) {
    typedef typename std::decay<T>::type V;
    return add_write<V, V&&>(std::move(wdata));
}

template <typename T, typename... Args>
inline TransProxy& TransProxy::add_write(Args&&... args) {
    if (!has_write()) {
        item().__or_flags(TransItem::write_bit);
        item().wdata_ = Packer<T>::pack(t()->buf_, std::forward<Args>(args)...);
        t()->any_writes_ = true;
    } else
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        item().wdata_ = Packer<T>::repack(t()->buf_, item().wdata_, std::forward<Args>(args)...);
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::set_stash(T sdata) {
    assert(!has_read());
    if (!has_stash()) {
        item().__or_flags(TransItem::stash_bit);
        item().rdata_ = Packer<T>::pack(t()->buf_, std::move(sdata));
    } else
        item().rdata_ = Packer<T>::repack(t()->buf_, item().rdata_, std::move(sdata));
    return *this;
}
