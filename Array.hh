#pragma once

#include <stdint.h>
#include <mutex>
#include <iostream>

#include "compiler.hh"

#include "Transaction.hh"
#include "Interface.hh"

#define SPIN_LOCK 1

template <typename T, unsigned N>
class Array : public Shared {
public:
  typedef uint64_t Version;
  typedef unsigned Key;
  typedef T Value;

  const Version lock_bit = 1U<<(sizeof(Version)*8 - 1);

  Array() : data_() {}

  T read(Key i) {
    return data_[i].val;
  }

  void write(Key i, Value v) {
    lock(i);
    data_[i].val = v;
    unlock(i);
  }

  inline void atomicRead(Key i, Version& v, Value& val) {
    Version v2;
    // if version stays the same across these reads then .val should match up with .version
    do {
      v = data_[i].version;
      fence();
      val = data_[i].val;
      fence();
      // make sure version didn't change after we read the value
      v2 = data_[i].version;
    } while (v != v2);
  }

  Value transRead_nocheck(Transaction& t, Key i) {
    auto& item = t.add_item(this, i);
    Version v;
    Value val;
    atomicRead(i, v, val);
    t.add_read(item, v);
    return val;
  }

  void transWrite_nocheck(Transaction& t, Key i, Value val) {
    auto& item = t.add_item(this, i);
    t.add_write(item, val);
  }

  Value transRead(Transaction& t, Key i) {
    auto& item = t.item(this, i);
    if (item.has_write()) {
      return item.template write_value<Value>();
    } else {
      Version v;
      Value val;
      atomicRead(i, v, val);
      if (!item.has_read()) {
        t.add_read(item, v);
      }
      return val;
    }
  }

  void transWrite(Transaction& t, Key i, Value val) {
    auto& item = t.item(this, i);
    // can just do this blindly
    t.add_write(item, val);
  }

  bool is_locked(Key i) {
    return (elem(i).version & lock_bit) != 0;
  }

  void lock(Key i) {
#if SPIN_LOCK
    internal_elem *pos = &elem(i);
    while (1) {
      Version cur = pos->version;
      if (!(cur&lock_bit) && bool_cmpxchg(&pos->version, cur, cur|lock_bit)) {
        break;
      }
      relax_fence();
    }
#else
    mutex(i).lock();
    // so we can quickly check is_locked
    elem(i).version |= lock_bit;
#endif
  }

  void unlock(Key i) {
#if SPIN_LOCK
    Version cur = elem(i).version;
    assert(cur & lock_bit);
    cur &= ~lock_bit;
    elem(i).version = cur;
#else
    elem(i).version &= ~lock_bit;
    mutex(i).unlock();
#endif
  }

  bool check(TransItem& item, Transaction& t) {
    bool versionOK = ((elem(unpack<Key>(item.key())).version ^ unpack<Version>(item.data.rdata)) 
                      & ~lock_bit) == 0;
    return versionOK && (t.check_for_write(item) || !is_locked(unpack<Key>(item.data.key)));
  }

  void lock(TransItem& item) {
    lock(unpack<Key>(item.data.key));
  }

  void unlock(TransItem& item) {
    unlock(unpack<Key>(item.data.key));
  }

  void install(TransItem& item, uint64_t tid) {
    Key i = unpack<Key>(item.data.key);
    Value val = unpack<Value>(item.data.wdata);
    assert(is_locked(i));
    // TODO: updating version then value leads to incorrect behavior
    // updating value then version means atomic read isn't a real thing
    // maybe transRead should just spin on is_locked? (because it essentially guarantees 
    // transaction failure anyway)
    elem(i).val = val;
    fence();
    Version cur = elem(i).version & ~lock_bit;
    // addition mod 2^(word_size-1)
    cur = (cur+1) & ~lock_bit;
    elem(i).version = cur | lock_bit;
  }

private:

  struct internal_elem {
    Version version;
    T val;
    internal_elem()
        : version(0), val() {
    }
  };

  internal_elem data_[N];

  internal_elem& elem(Key i) {
    return data_[i];
  }

#if !SPIN_LOCK
  std::mutex locks_[N];

  std::mutex& mutex(Key i) {
    return locks_[i];
  }
#endif

};
