#pragma once

#include <assert.h>

#include "config.h"
#include "compiler.hh"

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
          // don't do a read unlock because we'll already have an undo
          // registered for that.
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
    return (cur & write_lock_bit) && (cur & readerMask) <= 1;
  }
};


