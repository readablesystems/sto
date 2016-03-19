#pragma once

#include "Boosting_locks.hh"

class TransPessimisticLocking : public Shared {
  typedef uint64_t bit_type;
  static constexpr bit_type spin_lock = 1<<0;
  static constexpr bit_type read_lock = 1<<1;
  static constexpr bit_type write_lock = 1<<2;

  // XXX: it might be cleaner if we had 1 method that took a lock and its
  // unlock method. But this specificity allows us to inline the unlock methods.
  // We could potentially also make RWLock and SpinLock shared objects
  void transReadLock(RWLock *lock) {
    auto item = Sto::item(this, lock);
    // if we have the lock already (whether as a read lock or write lock), we're done.
    if (!item.has_write()) {
      if (!lock->tryReadLock(READ_SPIN)) {
        Sto::abort();
        return;
      }
      item.add_write(read_lock);
    }
  }
  void transWriteLock(RWLock *lock) {
    auto item = Sto::item(this, lock);
    if (!item.has_write()) {
      if (!lock->tryWriteLock(WRITE_SPIN)) {
        Sto::abort();
        return;
      }
      item.add_write(write_lock);
    } else if (item.template write_value<bit_type>() == read_lock) {
      if (!lock->tryUpgrade(WRITE_SPIN)) {
        Sto::abort();
        return;
      }
      item.add_write(write_lock);
    }
  }
  void transSpinLock(SpinLock *lock) {
    auto item = Sto::item(this, lock);
    if (!item.has_write()) {
      if (!lock->tryLock(WRITE_SPIN)) {
        Sto::abort();
        return;
      }
      item.add_write(spin_lock);
    }
  }
  void unlock(TransItem& item) {
    auto type = item.template write_value<bit_type>;
    if (type == spin_lock) {
      item.template key<SpinLock*>()->unlock();
      return;
    }
    auto *lock = item.template key<RWLock*>();
    if (type == read_lock) {
      lock->readUnlock();
    } else if (type == write_lock) {
      lock->writeUnlock();
    }
  }
};
