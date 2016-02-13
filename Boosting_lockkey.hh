#pragma once

#include "Boosting.hh"

#define STO_NO_STM
#include "Hashtable.hh"

template <typename K, unsigned Init_size = 129, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
class LockKey {
public:
  LockKey(unsigned size = Init_size, Hash h = Hash(), Pred p = Pred()) : lockMap(size, h, p) {}

  void readLock(const K& key) {
    RWLock *lock = getLock(key);
    if (!_thread().rwlockset.exists(lock)) {
      if (!lock->tryReadLock(READ_SPIN)) {
        DO_ABORT();
        return;
      }
      _thread().rwlockset.push(lock);
    }
  }

  void writeLock(const K& key) {
    RWLock *lock = getLock(key);
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

private:
  Hashtable<K, RWLock*, true, Init_size, RWLock*, Hash, Pred> lockMap;

  RWLock *getLock(const K& key) {
    RWLock *lock;
    if (!lockMap.read(key, lock)) {
      // TODO(nate): might want to acquire the lock before we insert it
      lock = new RWLock();
      // lock will either stay the same or be set to the current lock if one exists.
      lockMap.putIfAbsent(key, lock);
    }
    return lock;
  }
};
