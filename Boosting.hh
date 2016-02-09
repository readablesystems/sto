#pragma once

#include <assert.h>

#include <unordered_set>

#include "config.h"
#include "compiler.hh"

#define NO_STM
#include "Hashtable.hh"

#include "../../../tl2/stm.h"

class SpinLock {
public:
  typedef uint64_t lock_type;
private:
  lock_type lock;
  
  static constexpr lock_type lock_bit = lock_type(1) << (sizeof(lock_type) * 8 - 1);

public:
  SpinLock() : lock(0) {}

  bool tryLock(int spin = 0) {
    while (spin >= 0) {
      lock_type cur = lock;
      if (!(cur & lock_bit) && bool_cmpxchg(&lock, cur, lock_bit)) {
        return true;
      }
      spin--;
      relax_fence();
    }
    return false;
  }

  void unlock() {
    fence();
    lock = 0;
  }
};

// nate: realized after writing this we have an rwlock.hh. But this one has 
// support for not starving writers (which I guess could be useful), and for 
// upgrading read -> write locks (which we probably need for boosting).
// It's also as of yet untested :)
class RWLock {
public:
  typedef uint64_t lock_type;
private:
  lock_type lock;

  static constexpr lock_type write_lock_bit = lock_type(1) << (sizeof(lock_type) * 8 - 1);
  static constexpr lock_type waiting_write_bit = lock_type(1) << (sizeof(lock_type) * 8 - 2);
  static constexpr lock_type readerMask = ~(write_lock_bit | waiting_write_bit);


public:
  RWLock() : lock(0) {}

  bool tryReadLock(int spin = 0) {
    while (spin >= 0) {
      lock_type cur = lock;
      if (!(cur & write_lock_bit) && !(cur & waiting_write_bit)) {
        // apparently this is faster than cmpxchg
        lock_type prev = __sync_fetch_and_add(&lock, 1);
        // woops, it was already locked or about to be.
        if ((prev & write_lock_bit) || (prev & waiting_write_bit)) {
          __sync_fetch_and_add(&lock, -1);
        } else {
          acquire_fence();
          return true;
        }
      }
      spin--;
      relax_fence();
    }
    return false;
  }

  void readUnlock() {
    assert(lock & readerMask);
    __sync_fetch_and_add(&lock, -1);
  }

  bool tryWriteLock(int spin = 0) {
    bool registered = false;
    while (spin >= 0) {
      lock_type cur = lock;
      if (!(cur & write_lock_bit) && !(cur & readerMask)) {
        if (bool_cmpxchg(&lock, cur, write_lock_bit)) {
          acquire_fence();
          return true;
        }
      }
      // cur could be out of date but then we'll just set this next iteration
      if (!(cur & waiting_write_bit) && spin > 0) {
        lock_type prev = __sync_fetch_and_or(&lock, waiting_write_bit);
        registered = !(prev & waiting_write_bit);
      }
      spin--;
      relax_fence();
    }
    if (registered) {
      __sync_fetch_and_and(&lock, ~waiting_write_bit);
    }
    return false;
  }

  void writeUnlock() {
    __sync_fetch_and_and(&lock, ~write_lock_bit);
  }
  
  // if successful, we now hold a write lock. doesn't release the read lock in
  // either case.
  bool tryUpgrade(int spin = 0) {
    while (spin >= 0) {
      lock_type cur = lock;
      assert(cur & readerMask);
      // someone else upgraded it first
      if ((cur & write_lock_bit) || (__sync_fetch_and_or(&lock, write_lock_bit) & write_lock_bit)) {
        spin--;
        relax_fence();
        continue;
      }
      // we have the upgrade, now just have to wait for readers
      while (spin >= 0) {
        lock_type cur = lock;
        // no new readers should be adding themselves, so once this is won't go
        // back up
        if ((cur & readerMask) == 1) {
          acquire_fence();
          return true;
        }
        spin--;
        relax_fence();
      }
      // we got tired of waiting for other readers, didn't upgrade after all
      __sync_fetch_and_and(&lock, ~write_lock_bit);
    }
    return false;
  }

  // precondition is that you have at least a read lock
  bool isWriteLocked() {
    lock_type cur = lock;
    fence();
    // eek. There are several possible states:
    // * write locked with no readers: normal write lock
    // * write locked with 1 reader: upgraded lock (you own it)
    // * write locked with >1 reader: someone else is trying to upgrade this
    //   lock and is waiting for our read to finish. In that case the answer
    //   to our question is no (it's still technically only a read lock).
    //   Chances are we'll also conflict the other upgrader so we should maybe just abort.

    // might be better to just get a slightly better boosting semantics
    return (cur & write_lock_bit) && (cur & readerMask) <= 1;
  }
  // I'm not sure if it's actually useful to distinguish upgraded (MAYBE for optimizations?)
  // if not we could just readUnlock in tryUpgrade and then at least we have one less weird case.
  bool isUpgraded() {
    lock_type cur = lock;
    fence();
    return (cur & write_lock_bit) && (cur & readerMask) == 1;
  }
};

struct boosting_threadinfo {
  std::unordered_set<SpinLock*> lockset;
  std::unordered_set<RWLock*> rwlockset;
};

#define BOOSTING_MAX_THREADS 16
static boosting_threadinfo boosting_threads[BOOSTING_MAX_THREADS];

// TODO(nate): make a Boosting.cc to create this + something that initializes it for each thread
static __thread int boosting_threadid;

// TODO(nate): ughhh
Thread *self;

static inline boosting_threadinfo& _thread() {
  return boosting_threads[boosting_threadid];
}


void releaseLocksCallback(void*, void*) {
  for (auto *lock : _thread().lockset) {
    lock->unlock();
  }
  for (auto *rwlockp : _thread().rwlockset) {
    auto& rwlock = *rwlockp;
    // this certainly doesn't seem very safe but I think it might be
    if (rwlock.isUpgraded()) {
      rwlock.readUnlock();
      rwlock.writeUnlock();
    } else if (rwlock.isWriteLocked()) {
      rwlock.writeUnlock();
    } else {
      rwlock.readUnlock();
    }
  }

  _thread().lockset.clear();
  _thread().rwlockset.clear();
}

#define READ_SPIN 100
#define WRITE_SPIN 100

#define DO_ABORT() STM_RESTART()

#define ON_ABORT(callback, context1, context2) TxAbortHook(STM_SELF, (callback), (context1), (context2))

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
