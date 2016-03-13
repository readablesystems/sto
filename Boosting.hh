#pragma once

#include <assert.h>

#include "config.h"
#include "compiler.hh"

#include "Boosting_fastset.hh"
#include "Boosting_locks.hh"

#include "../../../tl2/stm.h"

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

void transReadLock(RWLock *lock);
void transWriteLock(RWLock *lock);

#define READ_SPIN 100
//(long(1)<<60)
#define WRITE_SPIN 100
//(long(1)<<60)

#define DO_ABORT() TxAbort(STM_CUR_SELF)

#define POST_COMMIT(callback, context1, context2, context3) TxPostCommitHook(STM_CUR_SELF, (callback), (context1), (context2), (context3))
#define ON_ABORT(callback, context1, context2, context3) TxAbortHook(STM_CUR_SELF, (callback), (context1), (context2), (context3))

