#pragma once

#include <stdint.h>
#include <mutex>
#include <iostream>

#include "compiler.hh"

#include "Transaction.hh"
#include "Interface.hh"

#define SPIN_LOCK 1

template <typename T, unsigned N>
class Array : public Reader, public Writer {
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

  T transRead(Transaction& t, Key i) {
    Version v;
    Version v2;
    Value val;
    // if version stays the same across these reads then .val should match up with .version
    do {
      v = data_[i].version;
      fence();
      val = data_[i].val;
      fence();
      // make sure version didn't change after we read the value
      v2 = data_[i].version;
    } while (v != v2);
    t.read(this, TransData(i, v));
    return val;
  }

  void transWrite(Transaction& t, Key i, Value v) {
    t.write(this, TransData(i, v));
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

  bool check(TransData data, bool isReadWrite) {
    bool versionOK = ((elem(unpack<Key>(data.key)).version ^ unpack<Version>(data.data)) 
                      & ~lock_bit) == 0;
    return versionOK && (isReadWrite || !is_locked(unpack<Key>(data.key)));
  }

  void lock(TransData data) {
    lock(unpack<Key>(data.key));
  }

  void unlock(TransData data) {
    unlock(unpack<Key>(data.key));
  }

  uint64_t UID(TransData data) const {
    return unpack<Key>(data.key);
  }

  void install(TransData data) {
    Key i = unpack<Key>(data.key);
    Value val = unpack<Value>(data.data);
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
