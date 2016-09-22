#pragma once

#include "Boosting_locks.hh"

// TODO: kind of an awkward name :)
class TransPessimisticLocking : public TObject {
  typedef uint64_t bit_type;
  // constexprs are too strange to use :|
  static inline bit_type spin_lock() { return 1<<0; }
  static inline bit_type read_lock() { return 1<<1; }
  static inline bit_type write_lock() { return 1<<2; }

public:
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
      item.add_write(read_lock());
    }
  }
  void transWriteLock(RWLock *lock) {
    auto item = Sto::item(this, lock);
    if (!item.has_write()) {
      if (!lock->tryWriteLock(WRITE_SPIN)) {
        Sto::abort();
        return;
      }
      item.add_write(write_lock());
    } else if (item.template write_value<bit_type>() == read_lock()) {
      if (!lock->tryUpgrade(WRITE_SPIN)) {
        Sto::abort();
        return;
      }
      item.add_write(write_lock());
    }
  }
  void transSpinLock(SpinLock *lock) {
    auto item = Sto::item(this, lock);
    if (!item.has_write()) {
      if (!lock->tryLock(WRITE_SPIN)) {
        Sto::abort();
        return;
      }
      item.add_write(spin_lock());
    }
  }

  bool lock(TransItem&, Transaction&) override { return true; }
  bool check(const TransItem&, const Transaction&) override { return false; }
  void install(TransItem&, const Transaction&) override {}
  void unlock(TransItem&) override {}
  void cleanup(TransItem& item, bool committed) override {
    auto type = item.template write_value<bit_type>();
    if (type == spin_lock()) {
      item.template key<SpinLock*>()->unlock();
      return;
    }
    auto *lock = item.template key<RWLock*>();
    if (type == read_lock()) {
      lock->readUnlock();
    } else if (type == write_lock()) {
      lock->writeUnlock();
    }
  }
};
