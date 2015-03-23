#pragma once

#include "local_vector.hh"
#include "compiler.hh"
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <unistd.h>
#include <iostream>


#define PERF_LOGGING 1
#define DETAILED_LOGGING 0
#define ASSERT_TX_SIZE 0

#if ASSERT_TX_SIZE
#if DETAILED_LOGGING
#  define TX_SIZE_LIMIT 20000
#  include <cassert>
#else
#  error "ASSERT_TX_SIZE requires DETAILED_LOGGING!"
#endif
#endif

#if DETAILED_LOGGING
#if PERF_LOGGING
#else
#  error "DETAILED_LOGGING requires PERF_LOGGING!"
#endif
#endif

#include "config.h"

#define NOSORT 0

#define MAX_THREADS 24

// transaction performance counters
enum txp {
    // all logging levels
    txp_total_aborts = 0, txp_total_starts = 1,
    txp_commit_time_aborts = 2, txp_max_set = 3,
    // DETAILED_LOGGING only
    txp_total_n = 4, txp_total_r = 5, txp_total_w = 6, txp_total_searched = 7,
    txp_total_check_read = 8,
#if !PERF_LOGGING
    txp_count = 0
#elif !DETAILED_LOGGING
    txp_count = 4
#else
    txp_count = 9
#endif
};

template <int N> struct has_txp_struct {
    static bool test(int p) {
        return unsigned(p) < unsigned(N);
    }
};
template <> struct has_txp_struct<0> {
    static bool test(int) {
        return false;
    }
};
inline bool has_txp(int p) {
    return has_txp_struct<txp_count>::test(p);
}


#include "Interface.hh"
#include "TransItem.hh"

#define INIT_SET_SIZE 512

void reportPerf();
#define STO_SHUTDOWN() reportPerf()

struct __attribute__((aligned(128))) threadinfo_t {
  unsigned epoch;
  unsigned spin_lock;
  local_vector<std::pair<unsigned, std::function<void(void)>>, 8> callbacks;
  local_vector<std::pair<unsigned, void*>, 8> needs_free;
  std::function<void(void)> trans_start_callback;
  std::function<void(void)> trans_end_callback;
#if PERF_LOGGING
  uint64_t p_[txp_count];
#endif
  threadinfo_t() : epoch(), spin_lock() {
#if PERF_LOGGING
      for (int i = 0; i != txp_count; ++i)
          p_[i] = 0;
#endif
  }
#if PERF_LOGGING
  static bool p_is_max(int p) {
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
          if (!p_is_max(p))
              p_[p] += n;
          else if (n > p_[p])
              p_[p] = n;
      }
  }
#endif
};


// TransactionBuffer
template <typename T> struct ObjectDestroyer {
    static void destroy(void* object) {
        ((T*) object)->~T();
    }
};

class TransactionBuffer {
    struct elt;
    struct item;

  public:
    TransactionBuffer()
        : e_() {
    }
    ~TransactionBuffer() {
        if (e_)
            hard_clear(true);
    }

    static constexpr size_t aligned_size(size_t x) {
        return (x + 7) & ~7;
    }

    template <typename T>
    void* pack_unique(T x) {
        return pack_unique(std::move(x), (typename Packer<T>::is_simple_type) 0);
    }
    template <typename T>
    void* pack(T x) {
        return pack(std::move(x), (typename Packer<T>::is_simple_type) 0);
    }

    void clear() {
        if (e_)
            hard_clear(false);
    }

  private:
    struct itemhdr {
        void (*destroyer)(void*);
        size_t size;
    };
    struct item : public itemhdr {
        char buf[0];
    };
    struct elthdr {
        elt* next;
        size_t pos;
        size_t size;
    };
    struct elt : public elthdr {
        char buf[0];
        void clear() {
            size_t off = 0;
            while (off < pos) {
                itemhdr* i = (itemhdr*) &buf[off];
                i->destroyer(i + 1);
                off += i->size;
            }
            pos = 0;
        }
    };
    elt* e_;

    item* get_space(size_t needed) {
        if (!e_ || e_->pos + needed > e_->size) {
            size_t s = std::max(needed, e_ ? e_->size * 2 : 256);
            elt* ne = (elt*) new char[sizeof(elthdr) + s];
            ne->next = e_;
            ne->pos = 0;
            ne->size = s;
            e_ = ne;
        }
        e_->pos += needed;
        return (item*) &e_->buf[e_->pos - needed];
    }

    template <typename T>
    void* pack(T x, int) {
        return Packer<T>::pack(x);
    }
    template <typename T>
    void* pack(T x, void*) {
        size_t isize = aligned_size(sizeof(itemhdr) + sizeof(T));
        item* space = this->get_space(isize);
        space->destroyer = ObjectDestroyer<T>::destroy;
        space->size = isize;
        new (&space->buf[0]) T(std::move(x));
        return &space->buf[0];
    }

    template <typename T>
    void* pack_unique(T x, int) {
        return Packer<T>::pack(x);
    }
    template <typename T>
    void* pack_unique(T x, void*);

    void hard_clear(bool delete_all);
};

template <typename T>
void* TransactionBuffer::pack_unique(T x, void*) {
    void (*destroyer)(void*) = ObjectDestroyer<T>::destroy;
    for (elt* e = e_; e; e = e->next) {
        size_t off = 0;
        while (off < e->pos) {
            item* i = (item*) &e->buf[off];
            if (i->destroyer == destroyer
                && ((Aliasable<T>*) &i->buf[0])->x == x)
                return &i->buf[0];
            off += i->size;
        }
    }
    return pack(std::move(x));
}



class Transaction {
public:
  static threadinfo_t tinfo[MAX_THREADS];
  static __thread int threadid;
  static unsigned global_epoch;
#if PERF_LOGGING
  static __thread Transaction* __transaction;
#endif
  typedef TransactionTid tid_type;
  static tid_type _TID;

#if PERF_LOGGING
  static Transaction& get_transaction() {
    if (!__transaction)
      __transaction = new Transaction();
    else
      __transaction->reset();
    return *__transaction;
  }
#endif

  static std::function<void(unsigned)> epoch_advance_callback;

#if PERF_LOGGING
  static threadinfo_t tinfo_combined() {
    threadinfo_t out;
    for (int i = 0; i != MAX_THREADS; ++i) {
        for (int p = 0; p != txp_count; ++p)
            out.combine_p(p, tinfo[i].p(p));
    }
    return out;
  }

  static void print_stats();
#endif


  static void acquire_spinlock(unsigned& spin_lock) {
    unsigned cur;
    while (1) {
      cur = spin_lock;
      if (cur == 0 && bool_cmpxchg(&spin_lock, cur, 1)) {
        break;
      }
      relax_fence();
    }
  }
  static void release_spinlock(unsigned& spin_lock) {
    spin_lock = 0;
    fence();
  }

  static void* epoch_advancer(void*) {
    while (1) {
      usleep(100000);
      auto g = global_epoch;
      for (auto&& t : tinfo) {
        if (t.epoch != 0 && t.epoch < g)
          g = t.epoch;
      }

      global_epoch = ++g;

      if (epoch_advance_callback)
        epoch_advance_callback(global_epoch);

      for (auto&& t : tinfo) {
        acquire_spinlock(t.spin_lock);
        auto deletetil = t.callbacks.begin();
        for (auto it = t.callbacks.begin(); it != t.callbacks.end(); ++it) {
          // TODO: check for overflow
          if ((int)it->first <= (int)g-2) {
            it->second();
            ++deletetil;
          } else {
            // callbacks are in ascending order so if this one is too soon of an epoch the rest will be too
            break;
          }
        }
        if (t.callbacks.begin() != deletetil) {
          t.callbacks.erase(t.callbacks.begin(), deletetil);
        }
        auto deletetil2 = t.needs_free.begin();
        for (auto it = t.needs_free.begin(); it != t.needs_free.end(); ++it) {
          // TODO: overflow
          if ((int)it->first <= (int)g-2) {
            free(it->second);
            ++deletetil2;
          } else {
            break;
          }
        }
        if (t.needs_free.begin() != deletetil2) {
          t.needs_free.erase(t.needs_free.begin(), deletetil2);
        }
        release_spinlock(t.spin_lock);
      }
    }
    return NULL;
  }

  static void rcu_cleanup(std::function<void(void)> callback) {
    acquire_spinlock(tinfo[threadid].spin_lock);
    tinfo[threadid].callbacks.emplace_back(global_epoch, callback);
    release_spinlock(tinfo[threadid].spin_lock);
  }

  static void rcu_free(void *ptr) {
    acquire_spinlock(tinfo[threadid].spin_lock);
    tinfo[threadid].needs_free.emplace_back(global_epoch, ptr);
    release_spinlock(tinfo[threadid].spin_lock);
  }

#if PERF_LOGGING
  static void inc_p(int p) {
      add_p(p, 1);
  }
  static void add_p(int p, uint64_t n) {
      tinfo[threadid].add_p(p, n);
  }
  static void max_p(int p, unsigned long long n) {
      tinfo[threadid].max_p(p, n);
  }
#endif

#if PERF_LOGGING
#define INC_P(p) inc_p((p))
#define ADD_P(p, n) add_p((p), (n))
#define MAX_P(p, n) max_p((p), (n))
#else
#define INC_P(p) /**/
#define ADD_P(p, n) /**/
#define MAX_P(p, n) /**/
#endif

  static tid_type incTid() {
    tid_type t_old = _TID;
    tid_type t_new = t_old + 1;
    tid_type t = cmpxchg(&_TID, t_old, t_new);
    if (t != t_old) {
      t_new = t;
    }
    return t_new;
  }


  Transaction() : transSet_() {
    reset();
  }

  ~Transaction() {
    if (!isAborted_ && !transSet_.empty()) {
      silent_abort();
    }
    end_trans();
  }

  void end_trans() {
    // TODO: this will probably mess up with nested transactions
    tinfo[threadid].epoch = 0;
    if (tinfo[threadid].trans_end_callback) tinfo[threadid].trans_end_callback();
  }

  // reset data so we can be reused for another transaction
  void reset() {
     //if (isAborted_
     //   && tinfo[threadid].p(txp_total_aborts) % 0x10000 == 0xFFFF)
        //print_stats();
    tinfo[threadid].epoch = global_epoch;
    if (tinfo[threadid].trans_start_callback) tinfo[threadid].trans_start_callback();
    transSet_.clear();
    writeset_ = NULL;
    nwriteset_ = 0;
    may_duplicate_items_ = false;
    isAborted_ = false;
    firstWrite_ = -1;
    start_tid_ = 0;
    buf_.clear();
    INC_P(txp_total_starts);
  }

private:
  void consolidateReads() {
    // TODO: should be stable sort technically, but really we want to use insertion sort
    auto first = transSet_.begin();
    auto last = transSet_.end();
    std::sort(first, last);
    // takes the first element of any duplicates which is what we want. that is, we want to verify
    // only the oldest read
    transSet_.erase(std::unique(first, last), last);
    may_duplicate_items_ = false;
  }

public:
  // adds item for a key that is known to be new (must NOT exist in the set)
  template <typename T>
  TransProxy new_item(Shared* s, T key) {
      transSet_.emplace_back(s, buf_.pack(std::move(key)));
      return TransProxy(*this, transSet_.back());
  }

  // adds item without checking its presence in the array
  template <typename T>
  TransProxy fresh_item(Shared *s, T key) {
      may_duplicate_items_ = !transSet_.empty();
      transSet_.emplace_back(s, buf_.pack_unique(std::move(key)));
      return TransProxy(*this, transSet_.back());
  }

  // tries to find an existing item with this key, otherwise adds it
  template <typename T>
  TransProxy item(Shared* s, T key) {
      void* xkey = buf_.pack_unique(std::move(key));
      TransItem* ti = find_item(s, xkey);
      if (!ti) {
          transSet_.emplace_back(s, xkey);
          ti = &transSet_.back();
      }
      return TransProxy(*this, *ti);
  }

  // gets an item that is intended to be read only. this method essentially allows for duplicate items
  // in the set in some cases
  template <typename T>
  TransProxy read_item(Shared *s, T key) {
      void* xkey = buf_.pack_unique(std::move(key));
      TransItem* ti = 0;
      if (firstWrite_ >= 0)
          for (auto it = transSet_.begin() + firstWrite_;
               it != transSet_.end(); ++it) {
              INC_P(txp_total_searched);
              if (it->sharedObj() == s && it->key_ == xkey) {
                  ti = &*it;
                  break;
              }
          }
      if (!ti) {
          may_duplicate_items_ = !transSet_.empty();
          transSet_.emplace_back(s, xkey);
          ti = &transSet_.back();
      }
      return TransProxy(*this, *ti);
  }

  template <typename T>
  OptionalTransProxy check_item(Shared* s, T key) {
      void* xkey = buf_.pack_unique(std::move(key));
      return OptionalTransProxy(*this, find_item(s, xkey));
  }

private:
  // tries to find an existing item with this key, returns NULL if not found
  TransItem* find_item(Shared *s, void* key) {
      for (auto it = transSet_.begin(); it != transSet_.end(); ++it) {
          INC_P(txp_total_searched);
          if (it->sharedObj() == s && it->key_ == key)
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

  bool check_for_write(const TransItem& item) const {
    // if writeset_ is NULL, we're not in commit (just an opacity check), so no need to check our writes (we
    // haven't locked anything yet)
    if (!writeset_)
      return false;
    auto it = &item;
    bool has_write = it->has_write();
    if (!has_write && may_duplicate_items_) {
      has_write = std::binary_search(writeset_, writeset_ + nwriteset_, -1, [&] (const int& i, const int& j) {
	  auto& e1 = unlikely(i < 0) ? item : transSet_[i];
	  auto& e2 = likely(j < 0) ? item : transSet_[j];
	  auto ret = likely(e1.key_ < e2.key_) || (unlikely(e1.key_ == e2.key_) && unlikely(e1.sharedObj() < e2.sharedObj()));
#if 0
	  if (likely(i >= 0)) {
	    auto cur = &i;
	    int idx;
	    if (ret) {
	      idx = (cur - writeset_) / 2;
	    } else {
	      idx = (writeset_ + nwriteset_ - cur) / 2;
	    }
	    __builtin_prefetch(&transSet_[idx]);
	  }
#endif
	  return ret;
	});
    }
    return has_write;
  }

  public:
  void check_reads() {
      if (!check_reads(transSet_.begin(), transSet_.end())) {
          abort();
      }
  }

  private:
  bool check_reads(TransItem *trans_first, TransItem *trans_last) {
    for (auto it = trans_first; it != trans_last; ++it)
      if (it->has_read()) {
        INC_P(txp_total_check_read);
        if (!it->sharedObj()->check(*it, *this)) {
          // XXX: only do this if we're dup'ing reads
            for (auto jt = trans_first; jt != it; ++jt)
                if (*jt == *it)
                    goto ok;
            return false;
        }
      ok: ;
      }
    return true;
  }

  public:
  bool try_commit() {
#if ASSERT_TX_SIZE
    if (transSet_.size() > TX_SIZE_LIMIT) {
        std::cerr << "transSet_ size at " << transSet_.size()
            << ", abort." << std::endl;
        assert(false);
    }
#endif
    MAX_P(txp_max_set, transSet_.size());
    ADD_P(txp_total_n, transSet_.size());

    if (isAborted_)
      return false;

    bool success = true;

    if (firstWrite_ < 0)
        firstWrite_ = transSet_.size();

    int writeset_alloc[transSet_.size() - firstWrite_];
    writeset_ = writeset_alloc;
    nwriteset_ = 0;
    for (auto it = transSet_.begin(); it != transSet_.end(); ++it) {

      if (it->has_write()) {
        writeset_[nwriteset_++] = it - transSet_.begin();
      }
#ifdef DETAILED_LOGGING
      if (it->has_read()) {
	INC_P(txp_total_r);
      }
#endif
    }

    //phase1
#if !NOSORT
    std::sort(writeset_, writeset_ + nwriteset_, [&] (int i, int j) {
	return transSet_[i] < transSet_[j];
      });
#endif
    TransItem* trans_first = &transSet_[0];
    TransItem* trans_last = trans_first + transSet_.size();

    auto writeset_end = writeset_ + nwriteset_;
    for (auto it = writeset_; it != writeset_end; ) {
      TransItem *me = &transSet_[*it];
      me->sharedObj()->lock(*me);
      ++it;
      if (may_duplicate_items_)
          for (; it != writeset_end && transSet_[*it].same_item(*me); ++it)
              /* do nothing */;
    }

    //    fence();

    tid_type commit_tid = incTid();

    //phase2
    if (!check_reads(trans_first, trans_last)) {
      success = false;
      goto end;
    }

    //    fence();

    //phase3
    for (auto it = trans_first + firstWrite_; it != trans_last; ++it) {
      TransItem& ti = *it;
      if (ti.has_write()) {
        INC_P(txp_total_w);
        ti.sharedObj()->install(ti, commit_tid);
      }
    }

  end:
    //    fence();

    for (auto it = writeset_; it != writeset_end; ) {
      TransItem *me = &transSet_[*it];
      me->sharedObj()->unlock(*me);
      ++it;
      if (may_duplicate_items_)
          for (; it != writeset_end && transSet_[*it].same_item(*me); ++it)
              /* do nothing */;
    }

    //    fence();

    if (success) {
      commitSuccess();
    } else {
      INC_P(txp_commit_time_aborts);
      silent_abort();
    }

    // Nate: we need this line because the Transaction destructor decides
    // whether to do an abort based on whether transSet_ is empty (meh)
    transSet_.clear();
    return success;
  }

  void silent_abort() {
    if (isAborted_)
      return;
    INC_P(txp_total_aborts);
    isAborted_ = true;
    for (auto& ti : transSet_) {
      ti.sharedObj()->cleanup(ti, false);
    }
    end_trans();
  }

  void abort() {
    silent_abort();
    throw Abort();
  }

    void commit() {
        if (!try_commit())
            throw Abort();
    }

  bool aborted() {
    return isAborted_;
  }

  tid_type start_tid() {
    if (start_tid_ == 0) {
      return read_tid();
    } else {
      return start_tid_;
    }
  }

  tid_type read_tid() {
    start_tid_ = _TID;
    return start_tid_;
  }

  class Abort {};

private:

  void commitSuccess() {
    for (TransItem& ti : transSet_) {
      ti.sharedObj()->cleanup(ti, true);
    }
    end_trans();
  }

private:
    int firstWrite_;
    bool may_duplicate_items_;
    bool isAborted_;
    TransactionBuffer buf_;
    local_vector<TransItem, INIT_SET_SIZE> transSet_;
    int* writeset_;
    int nwriteset_;
    mutable tid_type start_tid_;

    friend class TransProxy;
    friend class TransItem;
};



template <typename T>
inline TransProxy& TransProxy::add_read(T rdata) {
    if (!has_read()) {
        i_->__or_flags(TransItem::read_bit);
        i_->rdata_ = t_->buf_.pack(std::move(rdata));
    }
    return *this;
}

template <typename T, typename U>
inline TransProxy& TransProxy::update_read(T old_rdata, U new_rdata) {
    if (has_read() && this->read_value<T>() == old_rdata)
        i_->rdata_ = t_->buf_.pack(std::move(new_rdata));
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::add_write(T wdata) {
    if (has_write())
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        this->template write_value<T>() = std::move(wdata);
    else {
        i_->__or_flags(TransItem::write_bit);
        i_->wdata_ = t_->buf_.pack(std::move(wdata));
        t_->mark_write(*i_);
    }
    return *this;
}

inline bool TransItem::has_lock(const Transaction& t) const {
    return t.check_for_write(*this);
}

inline bool TransProxy::has_lock() const {
    return t_->check_for_write(*i_);
}
