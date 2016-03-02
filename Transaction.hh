#pragma once

#include "small_vector.hh"
#include "config.h"
#include "compiler.hh"
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <unistd.h>
#include <iostream>

#define CONSISTENCY_CHECK 1
#define PERF_LOGGING 1
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

#define HASHTABLE_SIZE 512
#define HASHTABLE_THRESHOLD 16

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
#if !PERF_LOGGING
    txp_count = 0
#elif !DETAILED_LOGGING
    txp_count = txp_hco_abort + 1
#else
    txp_count = txp_total_check_predicate + 1
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


#include "Interface.hh"
#include "TransItem.hh"

#define INIT_SET_SIZE 512

void reportPerf();
#define STO_SHUTDOWN() reportPerf()

struct __attribute__((aligned(128))) threadinfo_t {
    typedef unsigned epoch_type;
    typedef int signed_epoch_type;
    epoch_type epoch;
    unsigned spin_lock;
    small_vector<std::pair<epoch_type, std::function<void(void)>>, 8> callbacks;
    small_vector<std::pair<epoch_type, void*>, 8> needs_free;
    // XXX(NH): these should be vectors so multiple data structures can register
    // callbacks for these
    std::function<void(void)> trans_start_callback;
    std::function<void(void)> trans_end_callback;
    uint64_t p_[txp_count];
    threadinfo_t() : epoch(), spin_lock() {
#if PERF_LOGGING
        for (int i = 0; i != txp_count; ++i)
            p_[i] = 0;
#endif
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
    void reset_p(int p) {
        p_[p] = 0;
    }
};


class Transaction {
public:
    static threadinfo_t tinfo[MAX_THREADS];
    static threadinfo_t::epoch_type global_epoch;
    static bool run_epochs;
    typedef TransactionTid::type tid_type;
private:
    static TransactionTid::type _TID;
public:

    static std::function<void(threadinfo_t::epoch_type)> epoch_advance_callback;

    static threadinfo_t tinfo_combined() {
        threadinfo_t out;
        for (int i = 0; i != MAX_THREADS; ++i) {
            for (int p = 0; p != txp_count; ++p)
                out.combine_p(p, tinfo[i].p(p));
        }
        return out;
    }

    static void print_stats();

    static void clear_stats() {
        for (int i = 0; i != MAX_THREADS; ++i) {
            for (int p = 0; p!= txp_count; ++p)
                tinfo[i].reset_p(p);
        }
    }

    static void acquire_spinlock(unsigned& spin_lock) {
        unsigned cur;
        while (1) {
            cur = spin_lock;
            if (cur == 0 && bool_cmpxchg(&spin_lock, cur, 1))
                break;
            relax_fence();
        }
    }
    static void release_spinlock(unsigned& spin_lock) {
        spin_lock = 0;
        fence();
    }

    static void* epoch_advancer(void*);
    static void rcu_cleanup(std::function<void(void)> callback) {
        acquire_spinlock(tinfo[TThread::id()].spin_lock);
        tinfo[TThread::id()].callbacks.emplace_back(global_epoch, callback);
        release_spinlock(tinfo[TThread::id()].spin_lock);
    }

    static void rcu_free(void *ptr) {
        acquire_spinlock(tinfo[TThread::id()].spin_lock);
        tinfo[TThread::id()].needs_free.emplace_back(global_epoch, ptr);
        release_spinlock(tinfo[TThread::id()].spin_lock);
    }

#if PERF_LOGGING
    static void inc_p(int p) {
        add_p(p, 1);
    }
    static void add_p(int p, uint64_t n) {
        tinfo[TThread::id()].add_p(p, n);
    }
    static void max_p(int p, unsigned long long n) {
        tinfo[TThread::id()].max_p(p, n);
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
        : is_test_(false) {
        start();
    }

    struct testing_type {};
    static testing_type testing;

    Transaction(const testing_type&)
        : is_test_(true) {
        start();
    }

    struct uninitialized {};
    Transaction(uninitialized)
        : is_test_(false) {
        state_ = s_aborted;
    }

    ~Transaction() { /* XXX should really be private */
        if (in_progress())
            silent_abort();
    }

    // reset data so we can be reused for another transaction
    void start() {
        //if (isAborted_
        //   && tinfo[TThread::id()].p(txp_total_aborts) % 0x10000 == 0xFFFF)
           //print_stats();
        tinfo[TThread::id()].epoch = global_epoch;
        if (tinfo[TThread::id()].trans_start_callback)
            tinfo[TThread::id()].trans_start_callback();
        transSet_.clear();
        nhashed_ = 0;
        may_duplicate_items_ = false;
        firstWrite_ = -1;
        start_tid_ = commit_tid_ = 0;
        buf_.clear();
        INC_P(txp_total_starts);
        state_ = s_in_progress;
    }

  public:
    // adds item for a key that is known to be new (must NOT exist in the set)
    template <typename T>
    TransProxy new_item(const TObject* s, T key) {
        void *xkey = Packer<T>::pack_unique(buf_, std::move(key));
        transSet_.emplace_back(const_cast<TObject*>(s), xkey);
        return TransProxy(*this, transSet_.back());
    }

    // adds item without checking its presence in the array
    template <typename T>
    TransProxy fresh_item(const TObject* s, T key) {
        may_duplicate_items_ = !transSet_.empty();
        transSet_.emplace_back(const_cast<TObject*>(s), Packer<T>::pack_unique(buf_, std::move(key)));
        return TransProxy(*this, transSet_.back());
    }

    // tries to find an existing item with this key, otherwise adds it
    template <typename T>
    TransProxy item(const TObject* s, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        TransItem* ti = find_item(const_cast<TObject*>(s), xkey, 0);
        if (!ti) {
            transSet_.emplace_back(const_cast<TObject*>(s), xkey);
            ti = &transSet_.back();
        }
        return TransProxy(*this, *ti);
    }

    // gets an item that is intended to be read only. this method essentially allows for duplicate items
    // in the set in some cases
    template <typename T>
    TransProxy read_item(const TObject* s, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        TransItem* ti = nullptr;
        if (firstWrite_ >= 0)
            ti = find_item(const_cast<TObject*>(s), xkey, firstWrite_);
        if (!ti) {
            may_duplicate_items_ = !transSet_.empty();
            transSet_.emplace_back(const_cast<TObject*>(s), xkey);
            ti = &transSet_.back();
        }
        return TransProxy(*this, *ti);
    }

    template <typename T>
    OptionalTransProxy check_item(const TObject* s, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        return OptionalTransProxy(*this, find_item(const_cast<TObject*>(s), xkey, 0));
    }

private:
    static int hash(TObject* s, void* key) {
        auto n = (uintptr_t) key;
        n += -(n <= 0xFFFF) & reinterpret_cast<uintptr_t>(s);
        //2654435761
        return ((n >> 4) ^ (n & 15)) % HASHTABLE_SIZE;
    }

    // tries to find an existing item with this key, returns NULL if not found
    TransItem* find_item(TObject* s, void* key, int delta) {
#if TRANSACTION_HASHTABLE
        if (transSet_.size() > HASHTABLE_THRESHOLD) {
            if (nhashed_ < transSet_.size())
                update_hash();
            uint16_t idx = hashtable_[hash(s, key)];
            if (!idx)
                return NULL;
            else if (transSet_[idx - 1].owner() == s && transSet_[idx - 1].key_ == key)
                return &transSet_[idx - 1];
        }
#endif
        for (auto it = transSet_.begin() + delta; it != transSet_.end(); ++it) {
            INC_P(txp_total_searched);
            if (it->owner() == s && it->key_ == key)
                return &*it;
        }
        return NULL;
    }

private:
    typedef int item_index_type;
    item_index_type item_index(TransItem& ti) {
        return &ti - transSet_.begin();
    }

    void mark_write(TransItem& ti) {
        item_index_type idx = item_index(ti);
        if (firstWrite_ < 0 || idx < firstWrite_)
            firstWrite_ = idx;
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

    void print(std::ostream& w) const;

    class Abort {};

private:
    void stop(bool committed);

private:
    enum {
        s_in_progress = 0, s_committing = 1, s_committing_locked = 2,
        s_aborted = 3, s_committed = 4
    };

    int firstWrite_;
    uint8_t state_;
    bool may_duplicate_items_;
    uint16_t nhashed_;
    small_vector<TransItem, INIT_SET_SIZE> transSet_;
    mutable tid_type start_tid_;
    mutable tid_type commit_tid_;
    uint16_t hashtable_[HASHTABLE_SIZE];
    TransactionBuffer buf_;
    bool is_test_;

    friend class TransProxy;
    friend class TransItem;
    friend class Sto;
    friend class TestTransaction;
    void hard_check_opacity(TransactionTid::type t);
    void update_hash();
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


template <typename T>
inline TransProxy& TransProxy::add_read(T rdata) {
    assert(!has_stash());
    if (!has_read()) {
#if DETAILED_LOGGING
        Transaction::max_p(txp_max_rdata_size, sizeof(T));
#endif
        i_->__or_flags(TransItem::read_bit);
        i_->rdata_ = Packer<T>::pack(t()->buf_, std::move(rdata));
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
        i_->__or_flags(TransItem::read_bit);
        i_->rdata_ = Packer<TVersion>::pack(t()->buf_, std::move(version));
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
        i_->__or_flags(TransItem::read_bit);
        i_->rdata_ = Packer<TNonopaqueVersion>::pack(t()->buf_, std::move(version));
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
        i_->rdata_ = Packer<T>::repack(t()->buf_, i_->rdata_, new_rdata);
    return *this;
}


template <typename T>
inline TransProxy& TransProxy::set_predicate(T pdata) {
    assert(!has_read());
#if DETAILED_LOGGING
    Transaction::max_p(txp_max_rdata_size, sizeof(T));
#endif
    i_->__or_flags(TransItem::predicate_bit);
    i_->rdata_ = Packer<T>::pack(t()->buf_, std::move(pdata));
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
        i_->__or_flags(TransItem::write_bit);
        i_->wdata_ = Packer<T>::pack(t()->buf_, wdata);
        t()->mark_write(*i_);
    } else
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        i_->wdata_ = Packer<T>::repack(t()->buf_, i_->wdata_, wdata);
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::add_write(T&& wdata) {
    typedef typename std::decay<T>::type V;
#if DETAILED_LOGGING
    Transaction::max_p(txp_max_wdata_size, sizeof(V));
#endif
    if (!has_write()) {
        i_->__or_flags(TransItem::write_bit);
        i_->wdata_ = Packer<V>::pack(t()->buf_, std::move(wdata));
        t()->mark_write(*i_);
    } else
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        i_->wdata_ = Packer<V>::repack(t()->buf_, i_->wdata_, std::move(wdata));
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::set_stash(T sdata) {
    assert(!has_read());
    if (!has_stash()) {
#if DETAILED_LOGGING
        Transaction::max_p(txp_max_sdata_size, sizeof(T));
#endif
        i_->__or_flags(TransItem::stash_bit);
        i_->rdata_ = Packer<T>::pack(t()->buf_, std::move(sdata));
    } else
        i_->rdata_ = Packer<T>::repack(t()->buf_, i_->rdata_, std::move(sdata));
    return *this;
}
