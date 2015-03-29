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
  typedef unsigned Version;
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

  inline void atomicRead(Transaction& t, Key i, Version& v, Value& val) {
    Version v2;
    // if version stays the same across these reads then .val should match up with .version
    do {
      v2 = data_[i].version;
      if (v2 & lock_bit)
        t.abort();
      fence();
      val = data_[i].val;
      fence();
      // make sure version didn't change after we read the value
      v = data_[i].version;
    } while (v != v2);
  }

  Value transRead_nocheck(Transaction& t, Key i) {
    auto item = t.fresh_item(this, i);
    Version v;
    Value val;
    atomicRead(t, i, v, val);
    item.add_read(v);
    return val;
  }

  void transWrite_nocheck(Transaction& t, Key i, Value val) {
    auto item = t.fresh_item(this, i);
    item.add_write(val);
  }

  Value transRead(Transaction& t, Key i) {
    auto item = t.item(this, i);
    if (item.has_write()) {
      return item.template write_value<Value>();
    } else {
      Version v;
      Value val;
      atomicRead(t, i, v, val);
      item.add_read(v);
      return val;
    }
  }

  void transWrite(Transaction& t, Key i, Value val) {
    auto item = t.item(this, i);
    // can just do this blindly
    item.add_write(val);
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

  bool check(const TransItem& item, const Transaction& t) {
    bool versionOK = ((elem(item.key<Key>()).version ^ item.read_value<Version>()) 
                      & ~lock_bit) == 0;
    return versionOK && (!is_locked(item.key<Key>()) || item.has_lock(t));
  }

  void lock(TransItem& item) {
    lock(item.key<Key>());
  }

  void unlock(TransItem& item) {
    unlock(item.key<Key>());
  }

  void install(TransItem& item, const Transaction&) {
    Key i = item.key<Key>();
    Value val = item.write_value<Value>();
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
