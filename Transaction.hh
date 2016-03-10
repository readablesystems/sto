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

#define CONSISTENCY_CHECK 1
#define PERF_LOGGING 0
#define DETAILED_LOGGING 0
#define ASSERT_TX_SIZE 0
#define TRANSACTION_HASHTABLE 1

#if ASSERT_TX_SIZE
#if DETAILED_LOGGING
#    define TX_SIZE_LIMIT 20000
#    include <cassert>
#else
#    error "ASSERT_TX_SIZE requires DETAILED_LOGGING!"
#endif
#endif

#if DETAILED_LOGGING
#if PERF_LOGGING
#else
#    error "DETAILED_LOGGING requires PERF_LOGGING!"
#endif
#endif

#include "config.h"

#define NOSORT 0

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
    // DETAILED_LOGGING only
    txp_total_n,
    txp_total_r,
    txp_total_w,
    txp_max_rdata_size,
    txp_max_wdata_size,
    txp_max_sdata_size,
    txp_total_searched,
    txp_push_abort,
    txp_pop_abort,
    txp_total_check_read,
    txp_total_check_predicate,
    txp_hash_collision,
#if !PERF_LOGGING
    txp_count = 0
#elif !DETAILED_LOGGING
    txp_count = txp_hco_abort + 1
#else
    txp_count = txp_hash_collision + 1
#endif
};

template <int N> struct has_txp_struct {
    static constexpr bool test(int p) {
        return unsigned(p) < unsigned(N);
    }
};
template <> struct has_txp_struct<0> {
    static constexpr bool test(int) {
        return false;
    }
};
inline constexpr bool has_txp(int p) {
    return has_txp_struct<txp_count>::test(p);
}
struct txp_counters {
    uint64_t p_[txp_count];
    txp_counters() {
        for (unsigned i = 0; i != txp_count; ++i)
            p_[i] = 0;
    }
    static bool p_combine_by_max(int p) {
        return p == txp_max_set;
    }
    unsigned long long p(int p) {
        return has_txp(p) ? p_[p] : 0;
    }
    void inc_p(int p) {
        add_p(p, 1);
    }
    void add_p(int p, uint64_t n) {
        if (has_txp(p))
            p_[p] += n;
    }
    void max_p(int p, unsigned long long n) {
        if (has_txp(p) && n > p_[p])
            p_[p] = n;
    }
    void combine_p(int p, unsigned long long n) {
        if (has_txp(p)) {
            if (!p_combine_by_max(p))
                p_[p] += n;
            else if (n > p_[p])
                p_[p] = n;
        }
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
    static constexpr unsigned hashtable_size = 1024;
    using epoch_type = TRcuSet::epoch_type;
    using signed_epoch_type = TRcuSet::signed_epoch_type;

    static threadinfo_t tinfo[MAX_THREADS];
    static struct epoch_state {
        epoch_type global_epoch; // != 0
        epoch_type active_epoch; // no thread is before this epoch
        bool run;
    } global_epochs;
    typedef TransactionTid::type tid_type;
private:
    static TransactionTid::type _TID;
public:

    static std::function<void(threadinfo_t::epoch_type)> epoch_advance_callback;

    static txp_counters txp_counters_combined() {
        txp_counters out;
        for (int i = 0; i != MAX_THREADS; ++i) {
            for (int p = 0; p != txp_count; ++p)
                out.combine_p(p, tinfo[i].p_.p(p));
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

#if PERF_LOGGING
    static void inc_p(int p) {
        add_p(p, 1);
    }
    static void add_p(int p, uint64_t n) {
        tinfo[TThread::id()].p_.add_p(p, n);
    }
    static void max_p(int p, unsigned long long n) {
        tinfo[TThread::id()].p_.max_p(p, n);
    }
#endif

#if PERF_LOGGING
#define INC_P(p) Transaction::inc_p((p))
#define ADD_P(p, n) Transaction::add_p((p), (n))
#define MAX_P(p, n) Transaction::max_p((p), (n))
#else
#define INC_P(p) do {} while (0)
#define ADD_P(p, n) do {} while (0)
#define MAX_P(p, n) do {} while (0)
#endif


  private:
    Transaction()
        : hash_base_(32768), is_test_(false) {
        start();
    }

    struct testing_type {};
    static testing_type testing;

    Transaction(const testing_type&)
        : hash_base_(32768), is_test_(true) {
        start();
    }

    struct uninitialized {};
    Transaction(uninitialized)
        : hash_base_(32768), is_test_(false) {
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
        INC_P(txp_total_starts);
        state_ = s_in_progress;
    }

#if TRANSACTION_HASHTABLE
    static int hash(const TObject* obj, void* key) {
        auto n = reinterpret_cast<uintptr_t>(key) + 0x4000000;
        n += -uintptr_t(n < 0x8000000) & (reinterpret_cast<uintptr_t>(obj) >> 4);
        //2654435761
        return (n + (n >> 16) * 9) % hashtable_size;
    }
#endif

    TransItem* allocate_item(const TObject* obj, void* xkey) {
        transSet_.emplace_back(const_cast<TObject*>(obj), xkey);
#if TRANSACTION_HASHTABLE
        uint16_t& h = hashtable_[hash(obj, xkey)];
        if (h <= hash_base_)
            h = hash_base_ + transSet_.size();
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
        uint16_t idx = hashtable_[hash(obj, xkey)];
        if (idx <= hash_base_)
            return nullptr;
        TransItem* ti = &transSet_[idx - hash_base_ - 1];
        if (ti->owner() == obj && ti->key_ == xkey)
            return ti;
        INC_P(txp_hash_collision);
        //std::cerr << *ti << " X " << (void*) obj << "," << xkey << '\n';
#endif
        for (auto it = transSet_.begin(); it != transSet_.end(); ++it) {
            INC_P(txp_total_searched);
            if (it->owner() == obj && it->key_ == xkey)
                return &*it;
        }
        return nullptr;
    }

    bool preceding_read_exists(TransItem& ti) const {
        if (may_duplicate_items_)
            for (auto it = transSet_.begin(); it != &ti; ++it)
                if (it->owner() == ti.owner() && it->key_ == ti.key_)
                    return true;
        return false;
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
    bool try_lock(TVersion& vers) {
        // This function will eventually help us track the commit TID when we
        // have no opacity, or for GV7 opacity.
        return vers.try_lock();
    }
    bool try_lock(TNonopaqueVersion& vers) {
        return vers.try_lock();
    }

    void check_opacity(TransactionTid::type t) {
        assert(state_ <= s_committing);
        if (!start_tid_)
            start_tid_ = _TID;
        if (!TransactionTid::try_check_opacity(start_tid_, t))
            hard_check_opacity(t);
    }

    void check_opacity() {
        check_opacity(_TID);
    }

    tid_type commit_tid() const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        if (!commit_tid_)
            commit_tid_ = fetch_and_add(&_TID, TransactionTid::increment_value);
        return commit_tid_;
    }

    void print() const;
    void print(std::ostream& w) const;

    class Abort {};

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
    bool is_test_;
#if TRANSACTION_HASHTABLE
    uint16_t hashtable_[hashtable_size];
#endif

    void hard_check_opacity(TransactionTid::type t);
    void stop(bool committed);

    friend class TransProxy;
    friend class TransItem;
    friend class Sto;
    friend class TestTransaction;
};


class Sto {
public:
    static void start_transaction() {
        if (!TThread::txn)
            TThread::txn = new Transaction(Transaction::uninitialized());
        always_assert(!TThread::txn->in_progress());
        TThread::txn->start();
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
#if DETAILED_LOGGING
        Transaction::max_p(txp_max_rdata_size, sizeof(T));
#endif
        item().__or_flags(TransItem::read_bit);
        item().rdata_ = Packer<T>::pack(t()->buf_, std::move(rdata));
    }
    return *this;
}

inline TransProxy& TransProxy::observe(TVersion version) {
    assert(!has_stash());
    if (version.is_locked_elsewhere())
        t()->abort();
    t()->check_opacity(version.value());
    if (!has_read()) {
#if DETAILED_LOGGING
        Transaction::max_p(txp_max_rdata_size, sizeof(TVersion));
#endif
        item().__or_flags(TransItem::read_bit);
        item().rdata_ = Packer<TVersion>::pack(t()->buf_, std::move(version));
    }
    return *this;
}

inline TransProxy& TransProxy::observe(TNonopaqueVersion version) {
    assert(!has_stash());
    if (version.is_locked_elsewhere())
        t()->abort();
    if (!has_read()) {
#if DETAILED_LOGGING
        Transaction::max_p(txp_max_rdata_size, sizeof(TNonopaqueVersion));
#endif
        item().__or_flags(TransItem::read_bit);
        item().rdata_ = Packer<TNonopaqueVersion>::pack(t()->buf_, std::move(version));
    }
    return *this;
}

inline TransProxy& TransProxy::observe_opacity(TVersion version) {
    if (version.is_locked_elsewhere())
        t()->abort();
    t()->check_opacity(version.value());
    return *this;
}

inline TransProxy& TransProxy::observe_opacity(TNonopaqueVersion version) {
    if (version.is_locked_elsewhere())
        t()->abort();
    return *this;
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
#if DETAILED_LOGGING
    Transaction::max_p(txp_max_rdata_size, sizeof(T));
#endif
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

template <typename T>
inline TransProxy& TransProxy::add_write(const T& wdata) {
#if DETAILED_LOGGING
    Transaction::max_p(txp_max_wdata_size, sizeof(T));
#endif
    if (!has_write()) {
        item().__or_flags(TransItem::write_bit);
        item().wdata_ = Packer<T>::pack(t()->buf_, wdata);
        t()->any_writes_ = true;
    } else
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        item().wdata_ = Packer<T>::repack(t()->buf_, item().wdata_, wdata);
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::add_write(T&& wdata) {
    typedef typename std::decay<T>::type V;
#if DETAILED_LOGGING
    Transaction::max_p(txp_max_wdata_size, sizeof(V));
#endif
    if (!has_write()) {
        item().__or_flags(TransItem::write_bit);
        item().wdata_ = Packer<V>::pack(t()->buf_, std::move(wdata));
        t()->any_writes_ = true;
    } else
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        item().wdata_ = Packer<V>::repack(t()->buf_, item().wdata_, std::move(wdata));
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::set_stash(T sdata) {
    assert(!has_read());
    if (!has_stash()) {
#if DETAILED_LOGGING
        Transaction::max_p(txp_max_sdata_size, sizeof(T));
#endif
        item().__or_flags(TransItem::stash_bit);
        item().rdata_ = Packer<T>::pack(t()->buf_, std::move(sdata));
    } else
        item().rdata_ = Packer<T>::repack(t()->buf_, item().rdata_, std::move(sdata));
    return *this;
}
