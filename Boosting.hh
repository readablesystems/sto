#pragma once

#include <assert.h>

#include <unordered_set>

#include "config.h"
#include "compiler.hh"

#include "local_vector.hh"

#include "Boosting_locks.hh"

#define STO_NO_STM
#include "Hashtable.hh"

#include "../../../tl2/stm.h"

template <typename T, unsigned Size = 512>
class FastSet {
public:
  FastSet() : lockVec_() {}
  bool erase(T& obj) {
    auto it = find(obj);
    if (it != lockVec_.end()) {
      lockVec_.erase(it);
      return true;
    }
    return false;
  }
  bool insert(T& obj) {
    if (exists(obj))
      return false;
    lockVec_.push_back(obj);
    return true;
  }
  typename local_vector<T, Size>::iterator find (T& obj) {
    auto it = lockVec_.begin();
    for (auto end = lockVec_.end(); it != end; ++it) {
      if (*it == obj) {
        return it;
      }
    }
    return it;
  }
  typename local_vector<T, Size>::iterator begin () {
    return lockVec_.begin();
  }
  typename local_vector<T, Size>::iterator end () {
    return lockVec_.end();
  }
  bool exists(T& obj) {
    return find(obj) != lockVec_.end();
  }
  void clear() {
    lockVec_.clear();
  }
private:
  local_vector<T, Size> lockVec_;
};

struct boosting_threadinfo {
  FastSet<SpinLock*> lockset;
  FastSet<RWLock*> rwlockset;
};

#define BOOSTING_MAX_THREADS 16
extern boosting_threadinfo boosting_threads[BOOSTING_MAX_THREADS];
extern __thread int boosting_threadid;

static inline boosting_threadinfo& _thread() {
  return boosting_threads[boosting_threadid];
}

void boosting_txStartHook();

void boosting_setThreadID(int threadid);

void boosting_releaseLocksCallback(void*, void*, void*);

#define READ_SPIN 100
#define WRITE_SPIN 100

#define DO_ABORT() TxAbort(STM_CUR_SELF)

#define POST_COMMIT(callback, context1, context2, context3) TxPostCommitHook(STM_CUR_SELF, (callback), (context1), (context2), (context3))
#define ON_ABORT(callback, context1, context2, context3) TxAbortHook(STM_CUR_SELF, (callback), (context1), (context2), (context3))

