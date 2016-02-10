#pragma once

#include "Boosting.hh"

template <typename K, unsigned Init_size = 129, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
class LockKey {
public:
  LockKey(unsigned size = Init_size, Hash h = Hash(), Pred p = Pred()) : lockMap(size, h, p) {}

  void readLock(const K& key) {
    RWLock *lock = getLock(key);
    bool inserted;
    std::tie(std::ignore, inserted) = _thread().rwlockset.insert(lock);
    // check if we already have the lock or need to get it now
    if (inserted) {
      if (!lock->tryReadLock(READ_SPIN)) {
        _thread().rwlockset.erase(lock);
        DO_ABORT();
      }
    }
  }

  void writeLock(const K& key) {
    RWLock *lock = getLock(key);
    bool inserted;
    std::tie(std::ignore, inserted) = _thread().rwlockset.insert(lock);
    if (inserted) {
      // don't have the lock in any form yet
      if (!lock->tryWriteLock(WRITE_SPIN)) {
        _thread().rwlockset.erase(lock);
        DO_ABORT();
      }
      return;
    }
    // only have a read lock so far
    if (!lock->isWriteLocked()) {
      if (!lock->tryUpgrade(WRITE_SPIN)) {
        DO_ABORT();
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

template <typename K, typename V, unsigned Init_size = 129, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
class TransHashtable {
  Hashtable<K, V, true, Init_size, V, Hash, Pred> hashtable;
  LockKey<K, Init_size, Hash, Pred> lockKey;

public:
  typedef K Key;
  typedef V Value;

  TransHashtable(unsigned size = Init_size, Hash h = Hash(), Pred p = Pred()) : hashtable(size, h, p), lockKey(size, h, p) {}

  bool transGet(const Key& k, Value& retval) {
    lockKey.readLock(k);
    return hashtable.read(k, retval);
  }

  virtual void _undoDelete(std::pair<Key, Value> *pair) {
    hashtable.put(pair->key, pair->value);
    delete pair;
  }

  virtual void _undoInsert(Key *key) {
    hashtable.remove(key);
    delete key;
  }

  bool transDelete(const Key& k) {
    lockKey.writeLock(k);
    Value oldval;
    if (!hashtable.read(k, oldval)) {
      return false;
    }
    bool success = hashtable.remove(k);
    assert(success);

    auto *pair = new std::pair<Key, Value>(k, std::move(oldval));

    //    ON_ABORT(&TransHashtable::_undoDelete, this, pair);

    return success;
  }

  bool transInsert(const Key& k, const Value& val) {
    lockKey.writeLock(k);
    bool success = hashtable.insert(k, val);
    
    if (success) {
      //    ON_ABORT(Hashtable::remove, this, k);
    }

    return success;
  }
};
