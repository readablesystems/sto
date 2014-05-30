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

struct ReaderData {
  template <typename T, typename U>
  ReaderData(T t,  U u) : data1(pack(t)), data2(pack(u)) {}
  void *data1;
  void *data2;
};
struct WriterData {
  template <typename T, typename U>
  WriterData(T t, U u) : data1(pack(t)), data2(pack(u)) {}
  void *data1;
  void *data2;
};

class Reader {
public:
  virtual ~Reader() {}

  virtual bool check(ReaderData data) = 0;
  virtual bool is_locked(ReaderData data) = 0;
  virtual uint64_t UID(ReaderData data) const = 0;
};

class Writer {
public:
  virtual ~Writer() {}

  virtual void lock(WriterData data) = 0;
  virtual void unlock(WriterData data) = 0;
  virtual uint64_t UID(WriterData data) const = 0;
  virtual void install(WriterData data) = 0;

  // optional
  virtual void undo(WriterData data) {}
  virtual void afterT(WriterData data) {}
};
