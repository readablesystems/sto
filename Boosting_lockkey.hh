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
    transReadLock(lock);
  }

  void writeLock(const K& key) {
    RWLock *lock = getLock(key);
    transWriteLock(lock);
  }

private:
  Hashtable<K, RWLock, true, Init_size, RWLock, Hash, Pred> lockMap;

  RWLock *getLock(const K& key) {
    RWLock *lock = lockMap.readPtr(key);
    if (!lock) {
      // TODO(nate): might want to acquire the lock before we insert it
      // lock will either stay the same or be set to the current lock if one exists.
      lock = lockMap.putIfAbsentPtr(key, RWLock());
    }
    return lock;
  }
};
