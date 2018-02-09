#include "Boosting_tl2.hh"
#include "Boosting_standalone.hh"

boosting_threadinfo boosting_threads[BOOSTING_MAX_THREADS];
__thread int boosting_threadid;

void boosting_txStartHook() {
  POST_COMMIT(boosting_releaseLocksCallback, NULL, NULL, NULL);
  // XXX: this needs to exec last. We do this by having boosting execute callbacks in revers order
  ON_ABORT(boosting_releaseLocksCallback, NULL, NULL, NULL);
}

void boosting_setThreadID(int threadid) {
  boosting_threadid = threadid;
}

void boosting_releaseLocksCallback(void*, void*, void*) {
  for (auto *lock : _thread().lockset) {
    lock->unlock();
  }
  for (auto *rwlockp : _thread().rwlockset) {
    auto& rwlock = *rwlockp;
    if (rwlock.isWriteLocked()) {
      rwlock.writeUnlock();
    } else {
      rwlock.readUnlock();
    }
  }

  _thread().lockset.unsafe_clear();
  _thread().rwlockset.unsafe_clear();
}

void transReadLock(RWLock *lock) {
  if (!_thread().rwlockset.exists(lock)) {
    if (!lock->tryReadLock(READ_SPIN)) {
      DO_ABORT();
      return;
    }
    _thread().rwlockset.push(lock);
  }
}

void transWriteLock(RWLock *lock) {
  if (!_thread().rwlockset.exists(lock)) {
    // don't have the lock in any form yet
    if (!lock->tryWriteLock(WRITE_SPIN)) {
      DO_ABORT();
      return;
    }
    _thread().rwlockset.push(lock);
  } else {
    // only have a read lock so far
    if (!lock->isWriteLocked()) {
      if (!lock->tryUpgrade(WRITE_SPIN)) {
        DO_ABORT();
      }
    }
  }
}
