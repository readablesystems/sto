#pragma once

#include "config.h"
#include "compiler.hh"

// XXX: these should really be standalone classes rather than a boosting specific header.

#define READ_SPIN 100000
//(long(1)<<60)
#define WRITE_SPIN 100000
//(long(1)<<60)

class SpinLock {
public:
  typedef uint64_t lock_type;
private:
  lock_type lock;
  
  static constexpr lock_type lock_bit = lock_type(1) << (sizeof(lock_type) * 8 - 1);

public:
  SpinLock() : lock(0) {}

  bool tryLock(long spin = 0) {
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

  bool tryReadLock(long spin = 0) {
    while (spin >= 0) {
      lock_type cur = lock;
      fence();
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

  bool tryWriteLock(long spin = 0) {
    bool registered = false;
    while (spin >= 0) {
      lock_type cur = lock;
      fence();
      if (!(cur & write_lock_bit) && !(cur & readerMask)) {
	// we try to set the lock to just write_lock_bit, which will clear any
	// other flags
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
  bool tryUpgrade(long spin = 0) {
    lock_type cur = lock;
    assert(cur & readerMask);
    // XXX: it could be more clear to have a separate bit for this rather than overloading waiting_write_bit
    if ((cur & waiting_write_bit) || (__sync_fetch_and_or(&lock, waiting_write_bit) & waiting_write_bit)) {
      // either someone else is trying to upgrade this lock, or some other
      // writer is waiting for the lock. In the former case we're in a bit
      // of a deadlock with the other upgrader, so we may as well as let
      // them have it. In the latter case, it still seems okay to just
      // let the other writer win this one.
      return false;
    }
    fence();
    while (spin >= 0) {
      // now just have to wait for readers
      lock_type cur = lock;
      // no new readers should be adding themselves, so once this is 1 it won't go
      // back up (except temporarily)
      if ((cur & readerMask) == 1) {
	// simultaneously write lock and discard our read lock.
	// I think we should always be able to get this since
	// we have a read lock, and only one upgrader should
	// run at a time.
	if (bool_cmpxchg(&lock, cur, write_lock_bit)) {
	  acquire_fence();
	  return true;
	}
      }
      spin--;
      relax_fence();
    }
    // we got tired of waiting for other readers, didn't upgrade after all
    __sync_fetch_and_and(&lock, ~waiting_write_bit);
    return false;
  }

  // precondition is that you have at least a read lock
  bool isWriteLocked() {
    lock_type cur = lock;
    fence();
    return (cur & write_lock_bit);
  }
};
