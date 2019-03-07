#pragma once

#include "config.h"
#include "compiler.hh"
#include "small_vector.hh"
#include "TRcu.hh"
#include "ContentionManager.hh"
#include "TransScratch.hh"
#include "VersionBase.hh"
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>

//#include <coz.h>

#ifndef STO_PROFILE_COUNTERS
#define STO_PROFILE_COUNTERS 0
#endif
#ifndef STO_TSC_PROFILE
#define STO_TSC_PROFILE 0
#endif

#ifndef BILLION
#define BILLION 1000000000.0
#else
#error "BILLION already defined!"
#endif

#ifndef PROC_TSC_FREQ
#define PROC_TSC_FREQ 2.2 // Default processor base freq in GHz
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
#define STO_DEBUG_ABORTS_FRACTION 1
#endif

#ifndef STO_SORT_WRITESET
#define STO_SORT_WRITESET 0
#endif

#ifndef DEBUG_SKEW
#define DEBUG_SKEW 0
#endif

#ifndef CHECK_PROGRESS
#define CHECK_PROGRESS 0
#endif

#ifndef STO_SPIN_EXPBACKOFF
#define STO_SPIN_EXPBACKOFF 0
#endif

#ifndef STO_ABORT_ON_LOCKED
#define STO_ABORT_ON_LOCKED 1
#endif

#ifndef STO_SPIN_BOUND_WRITE
#if STO_SPIN_EXPBACKOFF
#define STO_SPIN_BOUND_WRITE 7
#else
#define STO_SPIN_BOUND_WRITE 3
#endif
#endif

#ifndef STO_SPIN_BOUND_WAIT
#if STO_SPIN_EXPBACKOFF
#define STO_SPIN_BOUND_WAIT 18
#else
#define STO_SPIN_BOUND_WAIT 20
#endif
#endif

#define CONSISTENCY_CHECK 0
#define ASSERT_TX_SIZE 0
#define TRANSACTION_HASHTABLE 1
//#define TRANSACTION_FILTER 0

#if ASSERT_TX_SIZE
#if STO_PROFILE_COUNTERS > 1
#    define TX_SIZE_LIMIT 20000
#    include <cassert>
#else
#    error "ASSERT_TX_SIZE requires STO_PROFILE_COUNTERS > 1!"
#endif
#endif

#include "config.h"

#define MAX_THREADS 128

// TRANSACTION macros that can be used to wrap transactional code
#define TRANSACTION                               \
    do {                                          \
        __label__ abort_in_progress;              \
        __label__ try_commit;                     \
        __label__ after_commit;                   \
        TransactionLoopGuard __txn_guard;         \
        while (1) {                               \
            __txn_guard.start();

#define RETRY(retry)                              \
            goto try_commit;                      \
abort_in_progress:                                \
            __txn_guard.silent_abort();           \
            goto after_commit;                    \
try_commit:                                       \
            if (__txn_guard.try_commit())         \
                break;                            \
after_commit:                                     \
            if (!(retry)) {                       \
                break;                            \
            }                                     \
        }                                         \
    } while (false)

#define TXN_DO(trans_op)     \
if (!(trans_op))             \
    goto abort_in_progress

// Transaction wrapper implemented using exception handling
#define TRANSACTION_E                             \
    do {                                          \
        TransactionLoopGuard __txn_guard;         \
        while (1) {                               \
            __txn_guard.start();                  \
            try {

#define RETRY_E(retry)                            \
                if (__txn_guard.try_commit())     \
                    break;                        \
            } catch (Transaction::Abort e) {      \
                __txn_guard.silent_abort();       \
            }                                     \
            if (!(retry))                         \
                throw Transaction::Abort();       \
        }                                         \
    } while (false)

// transaction performance counters
enum txp {
    // all logging levels
    txp_total_aborts = 0,
    txp_total_starts,
    txp_commit_time_nonopaque,
    txp_commit_time_aborts,
    txp_observe_lock_aborts,
    txp_lock_aborts,
    txp_aborted_by_others,
    txp_max_set,
    txp_csws,
    txp_cm_shouldabort,
    txp_cm_onrollback,
    txp_cm_onwrite,
    txp_cm_start,
    txp_allocate,
    txp_bv_hit,
    txp_tco,
    txp_hco,
    txp_hco_lock,
    txp_hco_invalid,
    txp_hco_abort,
    // STO_PROFILE_COUNTERS > 1 only
    txp_total_n,
    txp_total_r,
    txp_total_adaptive_opt,
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

// Profiling counters measuring run time breakdowns
enum TimingCounters {
    tc_commit = 0,
    tc_commit_wasted,
    tc_find_item,
    tc_abort,
    tc_cleanup,
    tc_opacity,
    tc_count
};

typedef uint64_t tc_counter_type;

template <int C, int N, bool Less = (C < N)>
struct tc_helper;

template <int C, int N>
struct tc_helper<C, N, true> {
    static bool counter_exists(int tc) {
        return tc < N;
    }
    static void account_array(tc_counter_type *tcs, tc_counter_type v) {
        tcs[C] += v;
    }
};

template <int C, int N>
struct tc_helper<C, N, false> {
    static bool counter_exists(int) { return false; }
    static void account_array(tc_counter_type*, tc_counter_type) {}
};

struct tc_counters {
    tc_counter_type tcs_[tc_count];
    tc_counters() { reset(); }
    tc_counter_type timing_counter(int name) {
        return tc_helper<0, tc_count>::counter_exists(name) ? tcs_[name] : 0;
    }
    double to_realtime(int name) {
        return (double)timing_counter(name) / BILLION / PROC_TSC_FREQ;
    }
    void reset() {
        for (int i = 0; i < tc_count; ++i)
            tcs_[i] = 0;
    }
};

#include "Interface.hh"
#include "TransItem.hh"

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
    tc_counters tcs_;
    threadinfo_t()
        : epoch(0) {
    }
};

template <int T, bool tmp_stats=false>
class TimeKeeper {
public:
    TimeKeeper() {
        init_tsc = read_tsc();
    }
    ~TimeKeeper() {
        if (tmp_stats)
            sync_thread_counter_tmp();
        else
            sync_thread_counter();
    }

    tc_counter_type init_tsc_val() const {
        return init_tsc;
    }

private:
    tc_counter_type init_tsc;

    inline void sync_thread_counter();
    inline void sync_thread_counter_tmp();
};

#define TSC_ACCOUNT(tc, ticks)                       \
    tc_helper<tc, tc_count>::account_array(          \
        Transaction::tinfo[TThread::id()].tcs_.tcs_, \
        ticks)

template <typename VersImpl>
class TicTocBase;

class Transaction {
public:
    static constexpr unsigned tset_initial_capacity = 512;

    static constexpr unsigned hash_size = 32768;
#ifdef TWO_PHASE_TRANSACTION
  static constexpr int subaction_capacity = 32768;
  struct subaction {
    int id;
    TransItem* first_item;
    int item_count;
    bool aborted;
  };

  static constexpr int subaction_hash_capacity = 64;
#endif
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

    static tc_counters tc_counters_combined() {
        tc_counters ret;
        for (int i = 0; i < MAX_THREADS; ++i) {
            for (int t = 0; t < tc_count; ++t) {
                ret.tcs_[t] += tinfo[i].tcs_.tcs_[t];
            }
        }
        return ret;
    }

    static void print_stats();

    static void clear_stats() {
        for (int i = 0; i != MAX_THREADS; ++i) {
            tinfo[i].p_.reset();
            tinfo[i].tcs_.reset();
        }
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
    template <unsigned P> static txp_counter_type txp_inspect() {
        return tinfo[TThread::id()].p_.p(P);
    }
#else
    template <unsigned P> static void txp_account(txp_counter_type) {
    }
    template <unsigned P> static txp_counter_type txp_inspect() {return 0;}
#endif

#define TXP_INCREMENT(p) Transaction::txp_account<(p)>(1)
#define TXP_ACCOUNT(p, n) Transaction::txp_account<(p)>((n))
#define TXP_INSPECT(p) Transaction::txp_inspect<(p)>()


private:
    static constexpr unsigned tset_chunk = 512;
    static constexpr unsigned tset_max_capacity = 32768;

    void initialize();

    Transaction()
        : threadid_(TThread::id()), is_test_(false) {
        initialize();
        start();
    }

    struct testing_type {};
    static testing_type testing;

    Transaction(int threadid, const testing_type&)
        : threadid_(threadid), is_test_(true), restarted(false) {
        initialize();
        start();
    }

    Transaction(bool)
        : threadid_(TThread::id()), is_test_(false), restarted(false) {
        initialize();
        state_ = s_aborted;
    }

    ~Transaction();

    void callCMstart();

    // reset data so we can be reused for another transaction
    void start() {
        threadinfo_t& thr = tinfo[TThread::id()];
        //if (isAborted_
        //   && tinfo[TThread::id()].p(txp_total_aborts) % 0x10000 == 0xFFFF)
           //print_stats();
#if STO_TSC_PROFILE
        start_tsc_ = read_tsc();
#endif
        thr.epoch = global_epochs.global_epoch;
        thr.rcu_set.clean_until(global_epochs.active_epoch);
        if (thr.trans_start_callback)
            thr.trans_start_callback();
        hash_base_ += tset_size_ + 1;
#ifdef TWO_PHASE_TRANSACTION
        two_phase_transaction_ = false;
        second_phase_ = false;
        subaction_next_ = 0;
        curr_subaction = NULL;
        curr_subaction_item_count = 0;
        first_subaction_started = false;
#ifndef TWO_PHASE_UNOPT        
        hash_location_next_ = 0;
#endif
#endif        
        tset_size_ = 0;
        tset_next_ = tset0_;
#if TRANSACTION_HASHTABLE
        if (hash_base_ >= 32768) {
            memset(hashtable_, 0, sizeof(hashtable_));
            /*if (TThread::always_allocate()) {
                memset(hashtable_1024_, 0, sizeof(hashtable_1024_)); 
            } else {
                memset(hashtable_32768_, 0, sizeof(hashtable_32768_));
            }*/
            hash_base_ = 0;
        }
#endif
        any_writes_ = any_nonopaque_ = may_duplicate_items_ = false;
        first_write_ = 0;
        start_tid_ = commit_tid_ = 0;
        tictoc_tid_ = 0;
        buf_.clear();
#if STO_DEBUG_ABORTS
        abort_item_ = nullptr;
        abort_reason_ = nullptr;
        abort_version_ = 0;
#endif
        TXP_INCREMENT(txp_total_starts);
        state_ = s_in_progress;
        callCMstart();
    }

#if TRANSACTION_HASHTABLE
    static int hash_int_array_index(const TObject* obj, void* key) {
	(void) obj;
	return (reinterpret_cast<uintptr_t>(key)) % hash_size;
    }

    static int hash_int_array_ptr(const TObject* obj, void* key) {
 	(void) obj;
	return (reinterpret_cast<uintptr_t>(key) >> 2) % hash_size;
    }

    static int hash_regular(const TObject* obj, void* key) {
        auto n = reinterpret_cast<uintptr_t>(key) + 0x4000000;
        n += -uintptr_t(n < 0x8000000) & (reinterpret_cast<uintptr_t>(obj) >> 4);
        //2654435761
        return (n + (n >> 16) * 9) % hash_size;
    }

    static int hash(const TObject* obj, void* key) {
        return hash_regular(obj, key);
    }
#endif

    void refresh_tset_chunk();

    void allocate_item_update_hash(const TObject* obj, void* xkey) {
#if TRANSACTION_HASHTABLE
        unsigned hi = hash(obj, xkey);
        //bitvector_[hi % bv_size] = true;
#if TRANSACTION_HASHTABLE > 1
        if (hashtable_[hi] > hash_base_)
            hi = (hi + hash_step) % hash_size;
#endif
        if (hashtable_[hi] <= hash_base_)
            hashtable_[hi] = hash_base_ + tset_size_;

#ifdef TWO_PHASE_TRANSACTION
#ifndef TWO_PHASE_UNOPT        
        // TODO: this is slightly inefficient because this method gets called in
        // Sto::_item, yet we have the same check done here. Try to remove the
        // check
        if (two_phase_transaction_ && !second_phase_ &&
            (curr_subaction != NULL || first_subaction_started) &&
            hash_location_next_ < subaction_hash_capacity) {
          subaction_hash_index_locations_[hash_location_next_++] = hi;
        }
#endif        
#endif
        
#endif
    }

    TransItem* allocate_item(const TObject* obj, void* xkey) {
	    //TXP_INCREMENT(txp_allocate);
        if (tset_size_ && tset_size_ % tset_chunk == 0)
            refresh_tset_chunk();
        ++tset_size_;
        new(reinterpret_cast<void*>(tset_next_)) TransItem(const_cast<TObject*>(obj), xkey);
        //tset_next_->s_ = reinterpret_cast<TransItem::ownerstore_type>(const_cast<TObject*>(obj));
        //tset_next_->key_ = xkey;
        allocate_item_update_hash(obj, xkey);
        return tset_next_++;
    }

   public:
    int threadid() const {
        return threadid_;
    }

    // adds item for a key that is known to be new (must NOT exist in the set)
    template <typename T>
    TransProxy new_item(const TObject* obj, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        return TransProxy(*this, *allocate_item(obj, xkey));
        }

    // adds item without checking its presence in the array
    template <typename T>
    TransProxy fresh_item(const TObject* obj, T key) {
        may_duplicate_items_ = tset_size_ > 0;
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        return TransProxy(*this, *allocate_item(obj, xkey));
       }

    template <typename T>
    TransProxy _item(const TObject* obj, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        TransItem* ti = find_item(const_cast<TObject*>(obj), xkey);
        if (!ti)
            ti = allocate_item(obj, xkey);
        return TransProxy(*this, *ti);
    }

#ifdef TWO_PHASE_TRANSACTION
  template <typename T>
  TransProxy item(const TObject* obj, T key) {

    // We will assume that when the TWO_PHASE_TRANSACTION flag is set, we will
    // only run two phase transactions.
    // if (two_phase_transaction_) {
      if (second_phase_) {
        TransProxy ti = _item(obj, key);

        // For the purposes of the 'red-blue' experiment, we do not invalidate
        // items when a subaction aborts, because we know the experiment never
        // references items in aborted subactions.
        
        // Assert that item requested was not invalidated in the first phase.
        // always_assert(!(ti.flags() & TransItem::two_pt_invalid_bit) &&
        //              "Accessing item that was invalidated in the first phase");
        
        return ti;
      } else if (curr_subaction != NULL || first_subaction_started) {

        // The following code is commented out because our 'blue-red' experiment
        // doesn't have items that overlap between more than one subaction.
        
        // We're in the first phase of a subaction and we need to check if the
        // item already existed, because it shouldn't if it's part of a
        // subaction.
        // always_assert(!check_item(obj,key));
        
        TransProxy ti = _item(obj, key);
#ifdef PHASE_TWO_VALIDATE
        // Set a bit in the TransItem to mark that it's added as part of a
        // subaction. This will be used in the 'phase_two_validate' method to
        // check that each item created in a subaction is actually part of a
        // subaction.
        ti->add_flags(TransItem::item_in_subaction_bit);
#endif
        // Update the number of items under the current subaction.
        curr_subaction_item_count++;
        return ti;
      }
      // TODO: you left off here. Before you proceed, you need to know how
      // default transaction will be supported.
    // } // endif (two_phase_transaction)
    return _item(obj, key);
  }
#else
  template <typename T>
  TransProxy item(const TObject* obj, T key) {
    return _item(obj, key);
  }
#endif


    template <typename T>
    TransProxy item_inlined(const TObject* obj, T key) {
        bool found = false;
        TransItem* ti;
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
#if TRANSACTION_HASHTABLE
        TXP_INCREMENT(txp_hash_find);
        unsigned hi = hash(obj, xkey);
        if (hashtable_[hi] > hash_base_) {
            unsigned tidx =  hashtable_[hi] - hash_base_ - 1;
            if (likely(tidx < tset_initial_capacity))
                ti = &tset0_[tidx];
            else
                ti = &tset_[tidx / tset_chunk][tidx % tset_chunk];
            if (ti->owner() == obj && ti->key_ == xkey) {
                found = true;
            } else {
#endif
	        //std::cout << "Hash not found!" << std::endl;
                for (unsigned tidx = 0; tidx != tset_size_; ++tidx) {
                    ti = (tidx % tset_chunk ? ti + 1 : tset_[tidx / tset_chunk]);
                    TXP_INCREMENT(txp_total_searched);
                    if (ti->owner() == obj && ti->key_ == xkey) {
                        found = true;
                        break;
                    }
                }
#if TRANSACTION_HASHTABLE
            }
        }
#endif
        
        if (!found) {
            if (tset_size_ && tset_size_ % tset_chunk == 0)
                refresh_tset_chunk();
            ++tset_size_;
            new(reinterpret_cast<void*>(tset_next_)) TransItem(const_cast<TObject*>(obj), xkey);
 #if TRANSACTION_HASHTABLE
            if (hashtable_[hi] <= hash_base_)
                hashtable_[hi] = hash_base_ + tset_size_;
#endif	
            ti = tset_next_++;
        }
 
        return TransProxy(*this, *ti);
    }

    // gets an item that is intended to be read only. this method essentially allows for duplicate items
    // in the set in some cases
    template <typename T>
    TransProxy read_item(const TObject* obj, T key) {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        TransItem* ti = nullptr;
        if (any_writes_) {
            ti = find_item(const_cast<TObject*>(obj), xkey);
        }
        else {
            may_duplicate_items_ = tset_size_ > 0;
        }
        if (!ti)
            ti = allocate_item(obj, xkey);
        return TransProxy(*this, *ti);
    }

    template <typename T>
    OptionalTransProxy check_item(const TObject* obj, T key) const {
        void* xkey = Packer<T>::pack_unique(buf_, std::move(key));
        TransItem* ti = find_item(const_cast<TObject*>(obj), xkey); 
        return OptionalTransProxy(const_cast<Transaction&>(*this), ti);
    }

private:
    TransItem* find_item_scan(TObject* obj, void* xkey) const {
        const TransItem* it = nullptr;
        for (unsigned tidx = 0; tidx != tset_size_; ++tidx) {
            it = (tidx % tset_chunk ? it + 1 : tset_[tidx / tset_chunk]);
            TXP_INCREMENT(txp_total_searched);
            if (it->owner() == obj && it->key_ == xkey)
                return const_cast<TransItem*>(it);
        }
        return nullptr;
    }
    // tries to find an existing item with this key, returns NULL if not found
    TransItem* find_item(TObject* obj, void* xkey) const {
#if STO_TSC_PROFILE
        TimeKeeper<tc_find_item> tk;
#endif
#if TRANSACTION_HASHTABLE
        TXP_INCREMENT(txp_hash_find);
        unsigned hi = hash(obj, xkey);
        for (int steps = 0; steps < TRANSACTION_HASHTABLE; ++steps) {
            if (hashtable_[hi] <= hash_base_)
                return nullptr;
            unsigned tidx = hashtable_[hi] - hash_base_ - 1;
            const TransItem* ti;
            if (likely(tidx < tset_initial_capacity))
                ti = &tset0_[tidx];
            else
                ti = &tset_[tidx / tset_chunk][tidx % tset_chunk];
            if (ti->owner() == obj && ti->key_ == xkey)
                return const_cast<TransItem*>(ti);
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
	// std::cout << "Hash not found!" << std::endl;
	return find_item_scan(obj, xkey); 
   }

    bool preceding_duplicate_read(TransItem *it) const;

#ifdef TWO_PHASE_TRANSACTION
  template <typename T>
  std::tuple<int, TransProxy> start_subaction(const TObject* obj, T key) {

  // Update the item count of the last subaction.
  update_current_subaction_item_count();

  // Set a bool to start the check that for every access to a record in a
  // subaction, it must not have been accessed already.
  if (curr_subaction == NULL) {
    first_subaction_started = true;
  }

#ifndef TWO_PHASE_UNOPT  
  // Reset the next location to place the hash of the subaction items since
  // we're starting a new subaction.
  hash_location_next_ = 0;
#endif
  
  // Try to find the item by calling Sto::item.
  TransProxy item_proxy = item(obj, key);
  
  always_assert(subaction_next_ < subaction_capacity);

#ifdef PHASE_TWO_VALIDATE
  // Retroactively set this item to be part of the new subaction, setting the
  // bit showing that it must be part of a subaction in general.
  item_proxy->add_flags(TransItem::item_in_subaction_bit);
#endif
  subactions_[subaction_next_] = { subaction_next_, &(item_proxy.item()), 1, false };
  curr_subaction = subactions_ + subaction_next_;
  curr_subaction_item_count = 1;
  return std::make_tuple(subaction_next_++, item_proxy);
}

  void update_current_subaction_item_count() {
    if (curr_subaction != NULL) {
      curr_subaction->item_count = curr_subaction_item_count;
    }
  }

  unsigned get_tset_size() {
    return tset_size_;
  }

  void abort_subaction(int i);
  void phase_two_validate();
#endif

public:
#if STO_DEBUG_ABORTS
    void mark_abort_because(TransItem* item, const char* reason, TransactionTid::type version = 0) const {
        abort_item_ = item;
        abort_reason_ = reason;
        if (version)
            abort_version_ = version;
    }
#else
    void mark_abort_because(TransItem*, const char*, TransactionTid::type = 0) const {
    }
#endif

    void abort_because(TransItem& item, const char* reason, TransactionTid::type version = 0) {
        mark_abort_because(&item, reason, version);
        abort();
    }

    void silent_abort() {
        if (in_progress())
            stop(false, nullptr, 0);
    }

    void abort() {
        silent_abort();
        throw Abort();
        //longjmp(env, 1);
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

    template <typename T>
    T *tx_alloc(const T *src) {
        return &scratch_.clone<T>(*src);
    }

    template <typename T>
    T *tx_alloc() {
        return &scratch_.allocate<T>();
    }

    // opacity checking
    // These function will eventually help us track the commit TID when we
    // have no opacity, or for GV7 opacity.
    template <typename VersImpl>
    inline bool try_lock(TransItem& item, VersionBase<VersImpl>& vers);

    template <typename VersImpl>
    static void unlock(TransItem& item, VersionBase<VersImpl>& vers) {
        vers.cp_unlock(item);
    }

    bool check_opacity(TransItem& item, TransactionTid::type v) __attribute__ ((warn_unused_result)) {
#if STO_TSC_PROFILE
        TimeKeeper<tc_opacity> tk;
#endif
        assert(state_ <= s_committing_locked);
        TXP_INCREMENT(txp_tco);
        if (!start_tid_)
            start_tid_ = _TID;
        if (!TransactionTid::try_check_opacity(start_tid_, v)
            && state_ < s_committing)
            return hard_check_opacity(&item, v);
        return true;
    }

    /*
    bool check_opacity(TransItem& item, TVersion v) {
        return check_opacity(item, v.value());
    }
    bool check_opacity(TransItem&, TNonopaqueVersion) {
        return true;
    }
    */

    bool check_opacity(TransactionTid::type v) {
        assert(state_ <= s_committing_locked);
        if (!start_tid_)
            start_tid_ = _TID;
        if (!TransactionTid::try_check_opacity(start_tid_, v)
            && state_ < s_committing)
            return hard_check_opacity(nullptr, v);
        return true;
    }

    bool check_opacity() {
        return check_opacity(_TID);
    }

    // committing
    tid_type commit_tid() const {
#if !CONSISTENCY_CHECK
        assert(state_ == s_committing_locked || state_ == s_committing);
#endif
        if (!commit_tid_)
            commit_tid_ = fetch_and_add(&_TID, TransactionTid::increment_value);
        return commit_tid_;
    }

    inline tid_type compute_tictoc_commit_ts() const;

    template <typename VersImpl>
    void set_version(VersionBase<VersImpl>& version, typename VersionBase<VersImpl>::type flags = 0) const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        tid_type v = version.cp_commit_tid(const_cast<Transaction &>(*this));
        version.cp_set_version(v | flags);
    }

    template <typename VersImpl>
    void set_version_unlock(VersionBase<VersImpl>& version, TransItem& item, typename VersionBase<VersImpl>::type flags = 0) const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        tid_type v = version.cp_commit_tid(const_cast<Transaction &>(*this));
        version.cp_set_version_unlock(v | flags);
        item.clear_needs_unlock();
    }

    template <typename VersImpl>
    void assign_version_unlock(VersionBase<VersImpl>& version, TransItem& item, typename VersionBase<VersImpl>::type flags = 0) const {
        tid_type v = version.cp_commit_tid(const_cast<Transaction &>(*this));
        version.value() = v | flags;
        item.clear_needs_unlock();
    }

    /*
    void set_version(TLockVersion& vers, TLockVersion::type flags = 0) const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        tid_type v = commit_tid_ ? commit_tid_ : TransactionTid::next_unflagged_version(vers.value());
        vers.set_version(v | flags);
    }
    void set_version_unlock(TLockVersion& vers, TransItem& item, TLockVersion::type flags = 0) const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        tid_type v = commit_tid_ ? commit_tid_ : TransactionTid::next_unflagged_version(vers.value());
        vers.set_version_unlock(v | flags);
        item.clear_needs_unlock();
    }
    void set_version(TVersion& vers, TVersion::type flags = 0) const {
        vers.set_version(commit_tid() | flags);
    }
    void set_version_unlock(TVersion& vers, TransItem& item, TVersion::type flags = 0) const {
        vers.set_version_unlock(commit_tid() | flags);
        item.clear_needs_unlock();
    }
    void assign_version_unlock(TVersion& vers, TransItem& item, TVersion::type flags = 0) const {
        vers = commit_tid() | flags;
        item.clear_needs_unlock();
    }
    void set_version(TNonopaqueVersion& vers, TNonopaqueVersion::type flags = 0) const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        tid_type v = commit_tid_ ? commit_tid_ : TransactionTid::next_unflagged_nonopaque_version(vers.value());
        vers.set_version(v | flags);
    }
    void set_version_unlock(TNonopaqueVersion& vers, TransItem& item, TNonopaqueVersion::type flags = 0) const {
        assert(state_ == s_committing_locked || state_ == s_committing);
        tid_type v = commit_tid_ ? commit_tid_ : TransactionTid::next_unflagged_nonopaque_version(vers.value());
        vers.set_version_unlock(v | flags);
        item.clear_needs_unlock();
    }
    void assign_version_unlock(TNonopaqueVersion& vers, TransItem& item, TNonopaqueVersion::type flags = 0) const {
        tid_type v = commit_tid_ ? commit_tid_ : TransactionTid::next_unflagged_nonopaque_version(vers.value());
        vers = v | flags;
        item.clear_needs_unlock();
    }

    template <bool Opaque>
    void set_version(TSwissVersion<Opaque>& vers, typename TSwissVersion<Opaque>::type flags = 0) const {
        tid_type v;
        if (Opaque) {
            v = commit_tid() | flags;
        } else {
            assert(state_ == s_committing_locked || state_ == s_committing);
            v = commit_tid_ ? commit_tid_ : TransactionTid::next_unflagged_nonopaque_version(vers.value());
            v |= flags;
        }
        vers.set_version(TSwissVersion<Opaque>(v, false));
    }
    template <bool Opaque>
    void set_version_unlock(TSwissVersion<Opaque>& vers, TransItem& item,
                            typename TSwissVersion<Opaque>::type flags = 0) const {
        tid_type v;
        if (Opaque) {
            v = commit_tid() | flags;
        } else {
            assert(state_ == s_committing_locked || state_ == s_committing);
            v = commit_tid_ ? commit_tid_ : TransactionTid::next_unflagged_nonopaque_version(vers.value());
            v |= flags;
        }
        vers.set_version_unlock(TSwissVersion<Opaque>(v, false));
        item.clear_needs_unlock();
    }
    */

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

    bool is_restarted() {
        return restarted;
    }
  
    void set_restarted(bool r) {
        restarted = r;
    }

#ifdef TWO_PHASE_TRANSACTION
  void set_two_phase_transaction(bool val) {
    two_phase_transaction_ = val;
  }

  void set_second_phase(bool val) {
    second_phase_ = val;
  }
#endif  

private:
    enum {
        s_in_progress = 0, s_opacity_check = 1, s_committing = 2,
        s_committing_locked = 3, s_aborted = 4, s_committed = 5
    };

    int threadid_;
    uint16_t hash_base_;
    uint16_t first_write_;
    uint8_t state_;
    bool any_writes_;
public:
    bool any_nonopaque_;
private:
    bool may_duplicate_items_;
    bool is_test_;
    bool restarted;
    TransItem* tset_next_;
    unsigned tset_size_;
    mutable tid_type start_tid_;
    mutable tid_type commit_tid_;
    mutable tid_type tictoc_tid_; // commit tid reserved for TicToc
public:
    mutable TransactionBuffer buf_;
    mutable TransScratch scratch_;
private:
    mutable uint32_t lrng_state_;
#if STO_DEBUG_ABORTS
    mutable TransItem* abort_item_;
    mutable const char* abort_reason_;
    mutable tid_type abort_version_;
#endif
#if STO_TSC_PROFILE
    mutable tc_counter_type start_tsc_;
#endif
    TransItem* tset_[tset_max_capacity / tset_chunk];
#if TRANSACTION_HASHTABLE
    uint16_t hashtable_[hash_size];
#endif
    TransItem tset0_[tset_initial_capacity];

#ifdef TWO_PHASE_TRANSACTION
  subaction subactions_[subaction_capacity];
  int subaction_next_;

  // Pointer to the current subaction. Gets updated whenever 'start_subaction' is
  // called.
  subaction* curr_subaction;
  int curr_subaction_item_count;
  bool first_subaction_started;

#ifndef TWO_PHASE_UNOPT  
  // Records the indices that items in the current subaction were hashed to in
  // 'hashtable_'. Used to optimize handling subaction aborts.
  unsigned subaction_hash_index_locations_[subaction_hash_capacity];
  unsigned hash_location_next_;
#endif
  
  // Whether the transaction is a two phase transaction or not.
  bool two_phase_transaction_;

  // Whether the transaction is in the second phase of a two phase transaction.
  bool second_phase_;
#endif

    bool hard_check_opacity(TransItem* item, TransactionTid::type t);
    void stop(bool committed, unsigned* writes, unsigned nwrites);

    friend class TransProxy;
    friend class TransItem;
    friend class Sto;
    friend class TestTransaction;

    friend class VersionDelegate;
};

template <int T, bool tmp_stats>
inline void TimeKeeper<T, tmp_stats>::sync_thread_counter() {
    tc_helper<T, tc_count>::account_array(
        Transaction::tinfo[TThread::id()].tcs_.tcs_,
        read_tsc() - init_tsc
    );
}

template <int T, bool tmp_stats>
inline void TimeKeeper<T, tmp_stats>::sync_thread_counter_tmp() {
    // not supported
    abort();
}

class Sto {
public:
    static Transaction* transaction() {
        if (!TThread::txn)
            TThread::txn = new Transaction(false);
        return TThread::txn;
    }

    static void start_transaction() {
        Transaction* t = transaction();
        always_assert(!t->in_progress());
        t->start();
    }

		static void delete_transaction() {
				delete TThread::txn;
				TThread::txn = nullptr;
		}

#ifdef TWO_PHASE_TRANSACTION
  // Starts a two phase transaction.  Call must be done inside a live
  // transaction and should be the first statement of a two phase transaction.
  // throws: error if the caller isn't in a transaction, or is already in a two
  // phase transaction, or if the transaction has a nonempty write set.
  static void start_two_phase_transaction() {
      always_assert(in_progress());
      always_assert(!TThread::txn->two_phase_transaction_);
      always_assert(!TThread::txn->any_writes_);

      TThread::txn->set_two_phase_transaction(true);
      TThread::txn->set_second_phase(false);
  }

  // Let's STO know that the second phase of a two phase transaction has begun,
  // marking only the objects that are of interest.
  // throws: error if the caller isn't in phase 1 of a two phase transaction at
  // the time this method is called.
  static void phase_two() {
      always_assert(in_progress());
      always_assert(TThread::txn->two_phase_transaction_ && !TThread::txn->second_phase_);
      always_assert(!TThread::txn->any_writes_);

      // Fill in the item count for the last subaction that was started.
      TThread::txn->update_current_subaction_item_count();

      // validate all items that are of interest.
#ifdef PHASE_TWO_VALIDATE
      TThread::txn->phase_two_validate();
#endif
      // TThread::second_phase_ = true;
      TThread::txn->set_second_phase(true);
  }

  // Begins a new subaction. Can only be called in the / first phase of a two
  // phase transaction.
  //'record': the record that will mark the first element of a subaction.
  // returns: an unique integer that identifies the subaction, or -1 if the
  // record already has a corresponding TItem associated with it.
  template <typename T>
  static std::tuple<int, TransProxy> start_subaction(const TObject* obj, T key) {
      always_assert(in_progress());
      always_assert(TThread::txn->two_phase_transaction_ && !TThread::txn->second_phase_);

      return TThread::txn->start_subaction(obj, key);
  }
 
  // Lets Sto know that the items encapsulated by the subaction are not to be
  // recorded during commit time. Can only be called in the first phase of a two
  // phase transaction.
  // 'i': unique identifier of a subaction to be told to abort.
  // throws: error if the index is past the total number of subactions created
  // already.
  static void abort_subaction(int i) {
    always_assert(in_progress());
    always_assert(TThread::txn->two_phase_transaction_ && !TThread::txn->second_phase_);

    // TODO: Transaction has Sto as a friend class, meaning Sto can access its
    // private variables. We could do the functionality in this method.
    TThread::txn->abort_subaction(i);
  }

  // TODO: are you doing this right?
  static unsigned get_tset_size() {
    return TThread::txn->get_tset_size();
  }
#endif

    static void update_threadid() {
        if (TThread::txn)
            TThread::txn->threadid_ = TThread::id();
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
        //always_assert(in_progress());
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
        //always_assert(in_progress());
        if (!in_progress())
            return false;
        return TThread::txn->try_commit();
    }

    static TransactionTid::type commit_tid() {
        return TThread::txn->commit_tid();
    }

    static TransactionTid::type recent_tid() {
        return Transaction::global_epochs.recent_tid;
    }

    static TransactionTid::type initialized_tid() {
        // XXX: we might want a nonopaque_bit in here too.
        return TransactionTid::increment_value;
    }

    template <typename T>
    static inline T* tx_alloc(const T* blob) {
        return TThread::txn->tx_alloc<T>(blob);
    }
    template <typename T>
    static inline T* tx_alloc() {
        return TThread::txn->tx_alloc<T>();
    }
};

class TestTransaction {
public:
    TestTransaction(int threadid)
        : t_(threadid, Transaction::testing) {
        use();
    }
    ~TestTransaction() {
        //if (base_ && !base_->is_test_) {
        //    TThread::txn = base_;
        //    TThread::set_id(base_->threadid_);
        //}
    }
    void use() {
        TThread::txn = &t_;
        TThread::set_id(t_.threadid_);
    }
    void print(std::ostream& w) const {
        t_.print(w);
    }
    bool try_commit() {
        use();
        auto r = t_.try_commit();
        TestTransaction::hard_reset();
        return r;
    }
    static void hard_reset() {
        TThread::txn = nullptr;
    }
    Transaction &get_tx() {
        return t_;
    }
private:
    Transaction t_;
    //Transaction* base_;
};

class TransactionGuard {
  public:
    TransactionGuard() {
        Sto::start_transaction();
    }
    ~TransactionGuard() {
        Sto::commit();
        Sto::delete_transaction();
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
        Transaction* t = Sto::transaction();
        t->set_restarted(false);
        //std::ostringstream buf;
        //buf << "Thread [" << TThread::id() << "] starts a new transaction" << std::endl;
        //std::cerr << buf.str();
        //std::cout << TThread::id() << " starts!" << std::endl;
    }

    ~TransactionLoopGuard() {
        if (TThread::txn->in_progress())
            TThread::txn->silent_abort();
    }
    void start() {
        Sto::start_transaction();
    }
    void silent_abort() {
        TThread::txn->silent_abort();
    }
    bool try_commit() {
        return TThread::txn->in_progress() && TThread::txn->try_commit();
    }
};


template <typename T>
inline bool TransProxy::add_read(T rdata) {
    assert(!has_stash());
    if (!has_read()) {
        item().__or_flags(TransItem::read_bit);
        item().rdata_.v = Packer<T>::pack(t()->buf_, std::move(rdata));
        t()->any_nonopaque_ = true;
    }
    return true;
}

// like add_read but checks opacity too.
// should be used by data structures that have non-TransactionTid
// versions and still need to respect opacity.
template <typename T>
inline bool TransProxy::add_read_opaque(T rdata) {
    assert(!has_stash());
    if (!t()->check_opacity()) {
        return false;
    }
    if (!has_read()) {
        item().__or_flags(TransItem::read_bit);
        item().rdata_.v = Packer<T>::pack(t()->buf_, std::move(rdata));
    }
    return true;
}

template <typename T>
inline TransProxy& TransProxy::update_read(T old_rdata, T new_rdata) {
    if (has_read() && this->read_value<T>() == old_rdata)
        item().rdata_.v = Packer<T>::repack(t()->buf_, item().rdata_.v, new_rdata);
    return *this;
}


inline TransProxy& TransProxy::set_predicate() {
    assert(!has_read());
    item().__or_flags(TransItem::predicate_bit);
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::set_predicate(T pdata) {
    assert(!has_read());
    item().__or_flags(TransItem::predicate_bit);
    item().rdata_.v = Packer<T>::pack(t()->buf_, std::move(pdata));
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
inline TransProxy& TransProxy::set_stash(T sdata) {
    assert(!has_read());
    if (!has_stash()) {
        item().__or_flags(TransItem::stash_bit);
        item().rdata_.v = Packer<T>::pack(t()->buf_, std::move(sdata));
    } else
        item().rdata_.v = Packer<T>::repack(t()->buf_, item().rdata_.v, std::move(sdata));
    return *this;
}

std::ostream& operator<<(std::ostream& w, const Transaction& txn);
std::ostream& operator<<(std::ostream& w, const TestTransaction& txn);
std::ostream& operator<<(std::ostream& w, const TransactionGuard& txn);
