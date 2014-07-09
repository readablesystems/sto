
#pragma once

#include <vector>
#include <algorithm>
#include <unistd.h>

#define LOCAL_VECTOR 1
#define PERF_LOGGING 0

#define MAX_THREADS 8

#if LOCAL_VECTOR
#include "local_vector.hh"
#endif

#include "Interface.hh"
#include "TransItem.hh"

#define INIT_SET_SIZE 16

#if PERF_LOGGING
uint64_t total_n;
uint64_t total_r, total_w;
uint64_t total_searched;
uint64_t total_aborts;
#endif

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

  Transaction() : transSet_(), readMyWritesOnly_(true), isAborted_(false) {
#if !LOCAL_VECTOR
    transSet_.reserve(INIT_SET_SIZE);
#endif
    // TODO: assumes this thread is constantly running transactions
    tinfo[threadid].epoch = global_epoch;
    if (tinfo[threadid].trans_start_callback) tinfo[threadid].trans_start_callback();
  }

  ~Transaction() {
    tinfo[threadid].epoch = 0;
    if (tinfo[threadid].trans_end_callback) tinfo[threadid].trans_end_callback();
  }

  // adds item without checking its presence in the array
  template <bool NOCHECK = true, typename T>
  TransItem& add_item(Shared *s, T key) {
    if (NOCHECK) {
      readMyWritesOnly_ = false;
    }
    void *k = pack(key);
    // TODO: TransData packs its arguments so we're technically double packing here (void* packs to void* though)
    transSet_.emplace_back(s, TransData(k, NULL, NULL));
    return transSet_[transSet_.size()-1];
  }

  // tries to find an existing item with this key, otherwise adds it
  template <typename T>
  TransItem& item(Shared *s, T key) {
    TransItem *ti;
    if ((ti = has_item(s, key)))
      return *ti;

    return add_item<false>(s, key);
  }

  // tries to find an existing item with this key, returns NULL if not found
  template <typename T>
  TransItem* has_item(Shared *s, T key) {
    void *k = pack(key);
    for (TransItem& ti : transSet_) {
#if PERF_LOGGING
      total_searched++;
#endif
      if (ti.sharedObj() == s && ti.data.key == k) {
        return &ti;
      }
    }
    return NULL;
  }

  template <typename T>
  void add_write(TransItem& ti, T wdata) {
    ti.add_write(wdata);
  }
  template <typename T>
  void add_read(TransItem& ti, T rdata) {
    ti.add_read(rdata);
  }
  void add_undo(TransItem& ti) {
    ti.add_undo();
  }

  void add_afterC(TransItem& ti) {
    ti.add_afterC();
  }

  bool commit() {
    if (isAborted_)
      return false;

    bool success = true;

#if PERF_LOGGING
    total_n += transSet_.size();
#endif

    //phase1
    if (readMyWritesOnly_) {
      std::sort(transSet_.begin(), transSet_.end());
    } else {
      std::stable_sort(transSet_.begin(), transSet_.end());
    }
    TransItem* trans_first = &transSet_[0];
    TransItem* trans_last = trans_first + transSet_.size();
    for (auto it = trans_first; it != trans_last; )
      if (it->has_write()) {
        TransItem* me = it;
        me->sharedObj()->lock(*me);
        ++it;
        if (!readMyWritesOnly_)
          for (; it != trans_last && it->same_item(*me); ++it)
            /* do nothing */;
      } else
        ++it;

    /* fence(); */

    //phase2
    for (auto it = trans_first; it != trans_last; ++it)
      if (it->has_read()) {
#if PERF_LOGGING
        total_r++;
#endif
        bool has_write = it->has_write();
        if (!has_write && !readMyWritesOnly_)
          for (auto it2 = it + 1;
               it2 != trans_last && it2->same_item(*it);
               ++it2)
            if (it2->has_write()) {
              has_write = true;
              break;
            }
        if (!it->sharedObj()->check(*it, has_write)) {
          success = false;
          goto end;
        }
      }
    
    //phase3
    for (TransItem& ti : transSet_) {
      if (ti.has_write()) {
#if PERF_LOGGING
        total_w++;
#endif
        ti.sharedObj()->install(ti);
      }
    }
    
  end:

    for (auto it = trans_first; it != trans_last; )
      if (it->has_write()) {
        TransItem* me = it;
        me->sharedObj()->unlock(*me);
        ++it;
        if (!readMyWritesOnly_)
          for (; it != trans_last && it->same_item(*me); ++it)
            /* do nothing */;
      } else
        ++it;
    
    if (success) {
      commitSuccess();
    } else {
      abort();
    }

    return success;
  }


  void abort() {
#if PERF_LOGGING
    __sync_add_and_fetch(&total_aborts, 1);
#endif
    isAborted_ = true;
    for (auto& ti : transSet_) {
      if (ti.has_undo()) {
        ti.sharedObj()->undo(ti);
      }
    }
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
  bool readMyWritesOnly_;
  bool isAborted_;

};

threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int Transaction::threadid;
unsigned Transaction::global_epoch;
std::function<void(unsigned)> Transaction::epoch_advance_callback;
