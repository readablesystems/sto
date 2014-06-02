
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
  template <typename K, typename RD, typename WD>
  TransData(K k, RD rd, WD wd) : key(pack(k)), rdata(pack(rd)), wdata(pack(wd)) {}
  inline bool operator==(const TransData& d2) const {
    return key == d2.key;
  }
  inline bool operator<(const TransData& d2) const {
    return key < d2.key;
  }
  // this word must be unique (to a particular item) and consistently ordered across transactions
  void *key;
  void *rdata;
  void *wdata;
};

class Shared {
public:
  virtual ~Shared() {}

  virtual bool check(TransData data, bool isReadWrite) = 0;

  virtual void lock(TransData data) = 0;
  virtual void unlock(TransData data) = 0;
  virtual void install(TransData data) = 0;

  // optional
  virtual void undo(TransData data) {}
  virtual void afterT(TransData data) {}
};
