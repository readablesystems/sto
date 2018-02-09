#pragma once

// this is common glue used by both TL2 bindings and "standalone" bindings 
// (but not by the STO bindings)

#include <assert.h>

#include "config.h"
#include "compiler.hh"

#include "Boosting_fastset.hh"
#include "Boosting_locks.hh"

#if !defined(BOOSTING) || defined(STO)
#error "Should not be used with BoostingSTO"
#endif

#define STO_NO_STM

// these 3 functions are what data structures use
#define ADD_UNDO ON_ABORT
#define TRANS_WRITE_LOCK transWriteLock
#define TRANS_READ_LOCK transReadLock

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
