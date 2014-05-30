#pragma once

#include <stdint.h>

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

struct TransData {
  template <typename K, typename D>
  TransData(K k, D d) : key(pack(k)), data(pack(d)) {}
  void *key;
  void *data;
};

class Reader {
public:
  virtual ~Reader() {}

  virtual bool check(TransData data) = 0;
  virtual bool is_locked(TransData data) = 0;
  virtual uint64_t UID(TransData data) const = 0;
};

class Writer {
public:
  virtual ~Writer() {}

  virtual void lock(TransData data) = 0;
  virtual void unlock(TransData data) = 0;
  virtual uint64_t UID(TransData data) const = 0;
  virtual void install(TransData data) = 0;

  // optional
  virtual void undo(TransData data) {}
  virtual void afterT(TransData data) {}
};
