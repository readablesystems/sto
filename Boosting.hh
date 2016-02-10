#pragma once

#include <assert.h>

#include <unordered_set>

#include "config.h"
#include "compiler.hh"

#include "Boosting_locks.hh"

#define STO_NO_STM
#include "Hashtable.hh"

#include "../../../tl2/stm.h"

struct boosting_threadinfo {
  std::unordered_set<SpinLock*> lockset;
  std::unordered_set<RWLock*> rwlockset;
};

#define BOOSTING_MAX_THREADS 16
extern boosting_threadinfo boosting_threads[BOOSTING_MAX_THREADS];
extern __thread int boosting_threadid;

static inline boosting_threadinfo& _thread() {
  return boosting_threads[boosting_threadid];
}

void boosting_txStartHook();

void boosting_setThreadID(int threadid);

void boosting_releaseLocksCallback(void*, void*);

#define READ_SPIN 100
#define WRITE_SPIN 100

#define DO_ABORT() TxAbort(STM_CUR_SELF)

#define POST_COMMIT(callback, context1, context2) TxPostCommitHook(STM_CUR_SELF, (callback), (context1), (context2))
#define ON_ABORT(callback, context1, context2) TxAbortHook(STM_CUR_SELF, (callback), (context1), (context2))

