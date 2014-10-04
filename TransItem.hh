#pragma once

#include <type_traits>
#include <string.h>
#include <assert.h>

#include "Tagged64.hh"

enum {
  READER_BIT = 1<<0,
  WRITER_BIT = 1<<1,
  UNDO_BIT = 1<<2,
  AFTERC_BIT = 1<<3,
};

template <bool B> struct packer {};

template <> struct packer<true> {
  template <typename T>
  void* pack(T value) {
    void* x;
    // yuck. we need to force T's copy constructor to be called at some point so we do this...
    new (&x) T(value);

    return x;
  }

  template <typename T>
  T& unpack(void*& packed) {
    // TODO: this won't work on big endian
    // could use memcpy but then can't return a reference
    return reinterpret_cast<T&>(packed);
  }

  template <typename T>
  void free_packed(void*& packed) { if (!__has_trivial_copy(T)) { this->unpack<T>(packed).T::~T(); } }
};

template <> struct packer<false> {
  template <typename T>
  void* pack(T value) {
    return new T(std::move(value));
  }

  template <typename T>
  T& unpack(void*& packed) {
    return *reinterpret_cast<T*>(packed);
  }

  template <typename T>
  void free_packed(void*& packed) {
    delete reinterpret_cast<T*>(packed);
  }
};


template <typename T>
inline void *pack(T v) {
  // TODO: probably better to use std::is_trivially_copyable or copy_constructible but gcc sucks :(
  return packer<sizeof(T) <= sizeof(void*)>().pack(std::move(v));
}

template <typename T>
inline T& unpack(void *&vp) {
  return packer<sizeof(T) <= sizeof(void*)>().template unpack<T>(vp);
}

template <typename T>
inline void free_packed(void *&vp) {
  packer<sizeof(T) <= sizeof(void*)>().template free_packed<T>(vp);
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

class Shared;

struct TransItem {
  template <typename K, typename RD, typename WD>
  TransItem(Shared *s, K k, RD rd, WD wd) : shared(s), data(k, rd, wd) {}

  Shared *sharedObj() const {
    return shared.ptr();
  }

  bool has_write() const {
    return shared.has_flags(WRITER_BIT);
  }
  bool has_read() const {
    return shared.has_flags(READER_BIT);
  }
  bool has_undo() const {
    return shared.has_flags(UNDO_BIT);
  }
  bool has_afterC() const {
    return shared.has_flags(AFTERC_BIT);
  }
  bool same_item(const TransItem& x) const {
    return sharedObj() == x.sharedObj() && data.key == x.data.key;
  }

  template <typename T>
  T& write_value() {
    assert(has_write());
    return unpack<T>(data.wdata);
  }
  template <typename T>
  T& read_value() {
    assert(has_read());
    return unpack<T>(data.rdata);
  }

  void *&key() {
    return data.key;
  }

  inline bool operator==(const TransItem& t2) const {
    return data == t2.data && sharedObj() == t2.sharedObj();
  }

  inline bool operator<(const TransItem& t2) const {
    // we compare keys and THEN shared objects here so that read and write keys with the same value
    // are next to each other
    return data < t2.data
      || (data == t2.data && sharedObj() < t2.sharedObj());
  }

  // TODO: should these be done Transaction methods like their add_ equivalents?
  void remove_write() {
    shared.rm_flags(WRITER_BIT);
  }
  void remove_read() {
    shared.rm_flags(READER_BIT);
  }
  void remove_undo() {
    shared.rm_flags(UNDO_BIT);
  }
  void remove_afterC() {
    shared.rm_flags(AFTERC_BIT);
  }

  // these methods are all for user flags (currently we give them 8 bits, the high 8 of the 16 total flag bits we have)
  uint8_t flags() {
    return shared.flags() >> 8;
  }
  void set_flags(uint8_t flags) {
    shared.set_flags(((uint16_t)flags << 8) | (shared.flags() & 0xff));
  }
  void rm_flags(uint8_t flags) {
    shared.rm_flags((uint16_t)flags << 8);
  }
  void or_flags(uint8_t flags) {
    shared.or_flags((uint16_t)flags << 8);
  }
  bool has_flags(uint8_t flags) {
    return shared.has_flags((uint16_t)flags << 8);
  }

private:
  template <typename T>
  void _add_write(T wdata) {
    shared.or_flags(WRITER_BIT);
    // TODO: this assumes that a given writer data always has the same type.
    // this is certainly true now but we probably shouldn't assume this in general
    // (hopefully we'll have a system that can automatically call destructors and such
    // which will make our lives much easier)
    //free_packed<T>(data.wdata);
    data.wdata = pack(std::move(wdata));
  }
  template <typename T>
  void _add_read(T rdata) {
    shared.or_flags(READER_BIT);
    //free_packed<T>(data.rdata);
    data.rdata = pack(std::move(rdata));
  }
  void _add_undo() {
    shared.or_flags(UNDO_BIT);
  }
  void _add_afterC() {
    shared.or_flags(AFTERC_BIT);
  }

private:
  friend class Transaction;
  Tagged64<Shared> shared;
public:
  // TODO: should this even really be public now that we have write_value() etc. methods?
  TransData data;
};
