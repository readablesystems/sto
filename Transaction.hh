#pragma once

#include <vector>
#include <algorithm>
#include <unistd.h>

#define LOCAL_VECTOR 1
#define PERF_LOGGING 0

#define MAX_THREADS 4

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

struct threadinfo {
  int epoch;
  std::vector<std::pair<int, std::function<void(void)>>> callbacks;
};

class Transaction {
public:
  static threadinfo tinfo[MAX_THREADS];
  static __thread int threadid;
  static int global_epoch;

  static void* epoch_advancer(void*) {
    while (1) {
      usleep(100000);
      auto g = global_epoch;
      for (auto&& t : tinfo) {
        if (t.epoch < g)
          g = t.epoch;
      }

      global_epoch = ++g;

      for (auto&& t : tinfo) {
        if (t.epoch == 0) continue;
        for (auto it = t.callbacks.begin(); it != t.callbacks.end(); ) {
          if (it->first < g-2) {
            it->second();
            it = t.callbacks.erase(it);
          } else {
            ++it;
          }
        }
      }
    }
  }

  static void cleanup(std::function<void(void)> callback) {
    tinfo[threadid].callbacks.emplace_back(global_epoch, callback);
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
  }

  ~Transaction() { tinfo[threadid].epoch = 0; }

  // adds item without checking its presence in the array
  template <bool NOCHECK = true, typename T>
  TransItem& add_item(Shared *s, T key) {
    if (NOCHECK) {
      readMyWritesOnly_ = false;
    }
    void *k = pack(key);
    // TODO: if user of this forgets to do add_read or add_write, we end up with a non-read, non-write, weirdo item
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
    }
  }

private:
  TransSet transSet_;
  bool readMyWritesOnly_;
  bool isAborted_;

};

threadinfo Transaction::tinfo[MAX_THREADS];
__thread int Transaction::threadid;
int Transaction::global_epoch;

