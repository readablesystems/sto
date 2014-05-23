#include <stdint.h>
#include <mutex>
#include <iostream>

#include "TransState.hh"
#include "Interface.hh"

#pragma once

template <typename T>
void *pack(T v) {
  static_assert(sizeof(T) <= sizeof(void*), "Can't fit type into void*");
  return reinterpret_cast<void*>(v);
}

template <typename T>
T unpack(void *vp) {
  static_assert(sizeof(T) <= sizeof(void*), "Can't fit type into void*");
  return (T)(intptr_t)vp;
}

template <typename T, unsigned N>
class Array : public Reader, public Writer {
public:
  typedef uint32_t Version;
  typedef unsigned Key;
  typedef T Value;

  Array() : data_() {}

  T read(Key i) {
    return data_[i].val;
  }

  void write(Key i, Value v) {
    mutex(i).lock();
    data_[i].val = v;
    mutex(i).unlock();
  }

  T transRead(TransState& t, Key i) {
    // "atomic read"
    internal_elem cur = data_[i];
    t.read(this, pack(i), pack(cur.version));
    return cur.val;
  }

  void transWrite(TransState& t, Key i, Value v) {
    t.write(this, pack(i), pack(v));
  }

  bool check(void *data1, void *data2) {
    return elem(unpack<Key>(data1)).version == unpack<Version>(data2);
  }

  void lock(void *data1, void *data2) {
    // TODO: double writes to same place will double (dead) lock
    mutex(unpack<Key>(data1)).lock();
  }

  virtual void unlock(void *data1, void *data2) {
    mutex(unpack<Key>(data1)).unlock();
  }

  virtual uint64_t UID(void *data1, void *data2) const {
    return unpack<Key>(data1);
  }

  virtual void install(void *data1, void *data2) {
    elem(unpack<Key>(data1)).val = unpack<Value>(data2);
    elem(unpack<Key>(data1)).version++;
  }

private:

  struct internal_elem {
    Version version;
    T val;
  };

  std::mutex locks_[N];
  internal_elem data_[N];
  
  internal_elem& elem(Key i) {
    return data_[i];
  }

  std::mutex& mutex(Key i) {
    return locks_[i];
  }

};
