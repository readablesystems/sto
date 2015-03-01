#pragma once

#include <vector>
#include <algorithm>
#include <functional>
#include <unistd.h>

#define LOCAL_VECTOR 1
#define PERF_LOGGING 1

#define NOSORT 0

#define MAX_THREADS 8

#if LOCAL_VECTOR
#include "local_vector.hh"
#endif

#include "Interface.hh"
#include "TransItem.hh"

#define INIT_SET_SIZE 512

void reportPerf();
#define STO_SHUTDOWN() reportPerf()

struct threadinfo_t {
  unsigned epoch;
  unsigned spin_lock;
  std::vector<std::pair<unsigned, std::function<void(void)>>> callbacks;
  std::function<void(void)> trans_start_callback;
  std::function<void(void)> trans_end_callback;
};

class Transaction {
public:
  static threadinfo_t tinfo[MAX_THREADS];
  static __thread int threadid;
  static unsigned global_epoch;

#if PERF_LOGGING
  static uint64_t total_n, total_r, total_w;
  static uint64_t total_searched;
  static uint64_t total_aborts;
  static uint64_t total_starts;
  static uint64_t commit_time_aborts;
#endif

  static std::function<void(unsigned)> epoch_advance_callback;

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
          if (it->first <= g-2) {
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
        release_spinlock(t.spin_lock);
      }
    }
    return NULL;
  }

  static void cleanup(std::function<void(void)> callback) {
    acquire_spinlock(tinfo[threadid].spin_lock);
    tinfo[threadid].callbacks.emplace_back(global_epoch, callback);
    release_spinlock(tinfo[threadid].spin_lock);
  }

#if LOCAL_VECTOR
  typedef local_vector<TransItem, INIT_SET_SIZE> TransSet;
#else
  typedef std::vector<TransItem> TransSet;
#endif

  Transaction() : transSet_(), permute(NULL), perm_size(0), readMyWritesOnly_(true), isAborted_(false), firstWrite_(-1) {
#if !LOCAL_VECTOR
    transSet_.reserve(INIT_SET_SIZE);
#endif
    // TODO: assumes this thread is constantly running transactions
    tinfo[threadid].epoch = global_epoch;
    if (tinfo[threadid].trans_start_callback) tinfo[threadid].trans_start_callback();
#if PERF_LOGGING
    __sync_add_and_fetch(&total_starts, 1);
#endif
  }

  ~Transaction() {
    tinfo[threadid].epoch = 0;
    if (tinfo[threadid].trans_end_callback) tinfo[threadid].trans_end_callback();
    if (!isAborted_ && transSet_.size() != 0) {
      silent_abort();
    }
  }

  void consolidateReads() {
    // TODO: should be stable sort technically, but really we want to use insertion sort
    auto first = transSet_.begin();
    auto last = transSet_.end();
    std::sort(first, last);
    // takes the first element of any duplicates which is what we want. that is, we want to verify
    // only the oldest read
    transSet_.resize(std::unique(first, last) - first);
  }

  // adds item without checking its presence in the array
  template <bool NOCHECK = true, typename T>
  TransItem& add_item(Shared *s, const T& key) {
    if (NOCHECK) {
      readMyWritesOnly_ = false;
    }
    void *k = pack(key);
    // TODO: TransData packs its arguments so we're technically double packing here (void* packs to void* though)
    transSet_.emplace_back(s, k, NULL, NULL);
    return transSet_[transSet_.size()-1];
  }

  // tries to find an existing item with this key, otherwise adds it
  template <typename T>
  TransItem& item(Shared *s, const T& key) {
    TransItem *ti = has_item(s, key);
    // we use the firstwrite optimization when checking for item(), but do a full check if they call has_item.
    // kinda jank, ideal I think would be we'd figure out when it's the first write, and at that point consolidate
    // the set to be doing rmw (and potentially even combine any duplicate reads from earlier)

    if (!ti)
      ti = &add_item<false>(s, key);
    return *ti;
  }

  // gets an item that is intended to be read only. this method essentially allows for duplicate items
  // in the set in some cases
  template <typename T>
  TransItem& read_only_item(Shared *s, const T& key) {
    TransItem *ti;
    if ((ti = has_item<true>(s, key)))
      return *ti;

    return add_item<false>(s, key);
  }

  // tries to find an existing item with this key, returns NULL if not found
  template <bool read_only = false, typename T>
  TransItem* has_item(Shared *s, const T& key) {
    if (read_only && firstWrite_ == -1) return NULL;

    if (!read_only && firstWrite_ == -1) {
      consolidateReads();
    }

    // TODO: the semantics here are wrong. this all works fine if key if just some opaque pointer (which it sorta has to be anyway)
    // but if it wasn't, we'd be doing silly copies here, AND have totally incorrect behavior anyway because k would be a unique
    // pointer and thus not comparable to anything in the transSet. We should either actually support custom key comparisons
    // or enforce that key is in fact trivially copyable/one word
    void *k = pack(key);
    for (auto it = transSet_.begin(); it != transSet_.end(); ++it) {
      TransItem& ti = *it;
#if PERF_LOGGING
      total_searched++;
#endif
      if (ti.sharedObj() == s && ti.data.key == k) {
        if (!read_only && firstWrite_ == -1)
          firstWrite_ = item_index(ti);
        return &ti;
      }
    }
    if (!read_only && firstWrite_ == -1)
      firstWrite_ = transSet_.size();
    return NULL;
  }

  int item_index(TransItem& ti) {
    return &ti - &transSet_[0];
  }

  template <typename T>
  void add_write(TransItem& ti, T wdata) {
    auto idx = item_index(ti);
    if (firstWrite_ < 0 || idx < firstWrite_)
      firstWrite_ = idx;
    ti._add_write(std::move(wdata));
  }
  template <typename T>
  void add_read(TransItem& ti, T rdata) {
    ti._add_read(std::move(rdata));
  }
  void add_undo(TransItem& ti) {
    ti._add_undo();
  }

  void add_afterC(TransItem& ti) {
    ti._add_afterC();
  }

  bool check_for_write(TransItem& item) {
    // if permute is NULL, we're not in commit (just an opacity check), so no need to check our writes (we
    // haven't locked anything yet)
    if (!permute)
      return false;
    auto it = &item;
    bool has_write = it->has_write();
    if (!has_write && !readMyWritesOnly_) {
      has_write = std::binary_search(permute, permute + perm_size, -1, [&] (const int& i, const int& j) {
	  auto& e1 = unlikely(i < 0) ? item : transSet_[i];
	  auto& e2 = likely(j < 0) ? item : transSet_[j];
	  auto ret = likely(e1.data < e2.data) || (unlikely(e1.data == e2.data) && unlikely(e1.sharedObj() < e2.sharedObj()));
#if 0
	  if (likely(i >= 0)) {
	    auto cur = &i;
	    int idx;
	    if (ret) {
	      idx = (cur - permute) / 2;
	    } else {
	      idx = (permute + perm_size - cur) / 2;
	    }
	    __builtin_prefetch(&transSet_[idx]);
	  }
#endif
	  return ret;
	});
    }
    return has_write;
  }

  void check_reads() {
    if (!check_reads(&transSet_[0], &transSet_[transSet_.size()])) {
      abort();
    }
  }

  bool check_reads(TransItem *trans_first, TransItem *trans_last) {
    for (auto it = trans_first; it != trans_last; ++it)
      if (it->has_read()) {
#if PERF_LOGGING
        total_r++;
#endif
        if (!it->sharedObj()->check(*it, *this)) {
          return false;
        }
      }
    return true;
  }

  bool commit() {
    if (isAborted_)
      return false;

    bool success = true;

#if PERF_LOGGING
    total_n += transSet_.size();
#endif

    if (firstWrite_ == -1) firstWrite_ = transSet_.size();

    int permute_alloc[transSet_.size() - firstWrite_];
    permute = permute_alloc;

    //    int permute[transSet_.size() - firstWrite_];
    /*int*/ perm_size = 0;
    auto begin = &transSet_[0];
    auto end = begin + transSet_.size();
    for (auto it = begin + firstWrite_; it != end; ++it) {
      if (it->has_write()) {
	permute[perm_size++] = it - begin;
      }
    }

    //phase1
#if !NOSORT
    std::sort(permute, permute + perm_size, [&] (int i, int j) {
	return transSet_[i] < transSet_[j];
      });
#endif
    TransItem* trans_first = &transSet_[0];
    TransItem* trans_last = trans_first + transSet_.size();

    auto perm_end = permute + perm_size;
    for (auto it = permute; it != perm_end; ) {
      TransItem *me = &transSet_[*it];
      if (me->has_write()) {
        me->sharedObj()->lock(*me);
        ++it;
        if (!readMyWritesOnly_)
          for (; it != perm_end && transSet_[*it].same_item(*me); ++it)
            /* do nothing */;
      } else
        ++it;
    }

    /* fence(); */

    //phase2
    if (!check_reads(trans_first, trans_last)) {
      success = false;
      goto end;
    }

    //phase3
    for (auto it = trans_first + firstWrite_; it != trans_last; ++it) {
      TransItem& ti = *it;
      if (ti.has_write()) {
#if PERF_LOGGING
        total_w++;
#endif
        ti.sharedObj()->install(ti);
      }
    }

  end:

    for (auto it = permute; it != perm_end; ) {
      TransItem *me = &transSet_[*it];
      if (me->has_write()) {
        me->sharedObj()->unlock(*me);
        ++it;
        if (!readMyWritesOnly_)
          for (; it != perm_end && transSet_[*it].same_item(*me); ++it)
            /* do nothing */;
      } else
        ++it;
    }
    
    if (success) {
      commitSuccess();
    } else {
#if PERF_LOGGING
      __sync_add_and_fetch(&commit_time_aborts, 1);
#endif
      abort();
    }

    transSet_.resize(0);

    return success;
  }

  void silent_abort() {
    if (isAborted_)
      return;
#if PERF_LOGGING
    __sync_add_and_fetch(&total_aborts, 1);
#endif
    isAborted_ = true;
    for (auto& ti : transSet_) {
      if (ti.has_undo()) {
        ti.sharedObj()->undo(ti);
      }
    }
  }

  void abort() {
    silent_abort();
    throw Abort();
  }

  bool aborted() {
    return isAborted_;
  }

  class Abort {};

private:

  void commitSuccess() {
    for (TransItem& ti : transSet_) {
      if (ti.has_afterC())
        ti.sharedObj()->afterC(ti);
      ti.sharedObj()->cleanup(ti);
    }
  }

private:
  TransSet transSet_;
  int *permute;
  int perm_size;
  bool readMyWritesOnly_;
  bool isAborted_;
  int16_t firstWrite_;
};
