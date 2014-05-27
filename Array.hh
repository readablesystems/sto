#pragma once

#include <stdint.h>
#include <mutex>
#include <iostream>

#include "compiler.hh"

#include "TransState.hh"
#include "Interface.hh"

#define SPIN_LOCK

template <typename T>
inline void *pack(T v) {
  static_assert(sizeof(T) <= sizeof(void*), "Can't fit type into void*");
  return reinterpret_cast<void*>(v);
}

template <typename T>
inline T unpack(void *vp) {
  static_assert(sizeof(T) <= sizeof(void*), "Can't fit type into void*");
  return (T)(intptr_t)vp;
}

template <typename T, unsigned N>
class Array : public Reader, public Writer {
public:
  typedef unsigned Version;
  typedef unsigned Key;
  typedef T Value;

  const Version lock_bit = 1<<(sizeof(Version)*8 - 1);

  Array() : data_() {}

  T read(Key i) {
    return data_[i].val;
  }

  void write(Key i, Value v) {
    assert(0);
    lock(i);
    data_[i].val = v;
    unlock(i);
  }

  T transRead(TransState& t, Key i) {
    internal_elem cur;
    Version v;
    // if version stays the same across these reads then .val should match up with .version
    do {
      v = data_[i].version;
      cur = data_[i];
    } while (cur.version != v);
    t.read(this, pack(i), pack(cur.version));
    return cur.val;
  }

  void transWrite(TransState& t, Key i, Value v) {
    t.write(this, pack(i), pack(v));
  }

  bool is_locked(Key i) {
    return (elem(i).version & lock_bit) != 0;
  }

  void lock(Key i) {
#ifdef SPIN_LOCK
    internal_elem *pos = &elem(i);
    while (1) {
      Version cur = pos->version;
      if (!(cur&lock_bit) && bool_cmpxchg(&pos->version, cur, cur|lock_bit)) {
        break;
      }
    }
#else
    mutex(i).lock();
    // so we can quickly check is_locked
    elem(i).version |= lock_bit;
#endif
  }

  void unlock(Key i) {
#ifdef SPIN_LOCK
    Version cur = elem(i).version;
    assert(cur & lock_bit);
    cur &= ~lock_bit;
    elem(i).version = cur;
#else
    elem(i).version &= ~lock_bit;
    mutex(i).unlock();
#endif
  }

  bool check(void *data1, void *data2) {
    return (elem(unpack<Key>(data1)).version ^ unpack<Version>(data2)) <= lock_bit;
  }

  bool is_locked(void *data1, void *data2) {
    return is_locked(unpack<Key>(data1));
  }

  void lock(void *data1, void *data2) {
    lock(unpack<Key>(data1));
  }

  void unlock(void *data1, void *data2) {
    unlock(unpack<Key>(data1));
  }

  uint64_t UID(void *data1, void *data2) const {
    return unpack<Key>(data1);
  }

  void install(void *data1, void *data2) {
    Key i = unpack<Key>(data1);
    Value v = unpack<Value>(data2);
    assert(is_locked(i));
    if (elem(i).val == v) {
      return;
    }
    elem(i).version++;
    assert((elem(i).version & ~lock_bit) == v);
    elem(i).val = v;
  }

private:

  struct internal_elem {
    Version version;
    T val;
  };

  internal_elem data_[N];
  
  internal_elem& elem(Key i) {
    return data_[i];
  }

#ifndef SPIN_LOCK
  std::mutex locks_[N];

  std::mutex& mutex(Key i) {
    return locks_[i];
  }
#endif

};
