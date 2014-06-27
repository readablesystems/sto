#pragma once

#include "Tagged64.hh"

#define READER_BIT (1<<0)
#define WRITER_BIT (1<<1)
#define UNDO_BIT (1<<2)
#define AFTERC_BIT (1<<3)

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

class Shared;

struct TransItem {
  TransItem(Shared *s, TransData data) : shared(s), data(data) {}

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
  T write_value() const {
    assert(has_write());
    return unpack<T>(data.wdata);
  }
  template <typename T>
  T read_value() const {
    assert(has_read());
    return unpack<T>(data.rdata);
  }

  void *key() {
    return data.key;
  }

  inline bool operator<(const TransItem& t2) const {
      return data < t2.data
        || (data == t2.data && shared < t2.shared);
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
    
private:
  template <typename T>
  void add_write(T wdata) {
    shared.or_flags(WRITER_BIT);
    data.wdata = pack(wdata);
  }
  template <typename T>
  void add_read(T rdata) {
    shared.or_flags(READER_BIT);
    data.rdata = pack(rdata);
  }
  void add_undo() {
    shared.or_flags(UNDO_BIT);
  }
  void add_afterC() {
    shared.or_flags(AFTERC_BIT);
  }

private:
  friend class Transaction;
  Tagged64<Shared> shared;
public:
  TransData data;
};
