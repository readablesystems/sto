#pragma once

#define READER_BIT (1<<0)
#define WRITER_BIT (1<<1)
#define UNDO_BIT (1<<2)
// for now at least we mark the highest bit of the pointer for this.
// this is probably incompatible with some 32-bit platforms
#define AFTERC_BIT (((uintptr_t)1) << (sizeof(uintptr_t)*8 - 1))

template <typename T>
T* readObj(T* obj) {
  //assert(!isReadObj(obj));
  return (T*)((uintptr_t)obj | READER_BIT);
}
template <typename T>
T* writeObj(T* obj) {
  //  assert(!isWriteObj(obj));
  return (T*)((uintptr_t)obj | WRITER_BIT);
}
template <typename T>
T* undoObj(T* obj) {
  return (T*)((uintptr_t)obj | UNDO_BIT);
}
template <typename T>
T* afterCObj(T* obj) {
  return (T*)((uintptr_t)obj | AFTERC_BIT);
}
template <typename T>
T* notReadObj(T* obj) {
  return (T*)((uintptr_t)obj & ~READER_BIT);
}
template <typename T>
T* notWriteObj(T* obj) {
  return (T*)((uintptr_t)obj & ~WRITER_BIT);
}
template <typename T>
T* notUndoObj(T* obj) {
  return (T*)((uintptr_t)obj & ~UNDO_BIT);
}
template <typename T>
T* notAfterCObj(T* obj) {
  return (T*)((uintptr_t)obj & ~AFTERC_BIT);
}
template <typename T>
T* untag(T* obj) {
  return (T*)((uintptr_t)obj & ~(WRITER_BIT|READER_BIT|UNDO_BIT|AFTERC_BIT));
}
template <typename T>
bool isReadObj(T* obj) {
  return (uintptr_t)obj & READER_BIT;
}
template <typename T>
bool isWriteObj(T* obj) {
  return (uintptr_t)obj & WRITER_BIT;
}
template <typename T>
bool isUndoObj(T* obj) {
  return (uintptr_t)obj & UNDO_BIT;
}
template <typename T>
bool isAfterCObj(T* obj) {
  return (uintptr_t)obj & AFTERC_BIT;
}

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
  TransItem(Shared *s, TransData data) : shared(s), data(data) {assert(untag(s)==s);}

  Shared *sharedObj() const {
    return untag(shared);
  }

  bool has_write() const {
    return isWriteObj(shared);
  }
  bool has_read() const {
    return isReadObj(shared);
  }
  bool has_undo() const {
    return isUndoObj(shared);
  }
  bool has_afterC() const {
    return isAfterCObj(shared);
  }
  bool same_item(const TransItem& x) const {
    return sharedObj() == x.sharedObj() && data.key == x.data.key;
  }

  template <typename T>
  T write_value() const {
    assert(isWriteObj(shared));
    return unpack<T>(data.wdata);
  }
  template <typename T>
  T read_value() const {
    assert(isReadObj(shared));
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
    shared = notWriteObj(shared);
  }
  void remove_read() {
    shared = notReadObj(shared);
  }
  void remove_undo() {
    shared = notUndoObj(shared);
  }
  void remove_afterC() {
    shared = notAfterCObj(shared);
  }
    
private:
  template <typename T>
  void add_write(T wdata) {
    shared = writeObj(shared);
    data.wdata = pack(wdata);
  }
  template <typename T>
  void add_read(T rdata) {
    shared = readObj(shared);
    data.rdata = pack(rdata);
  }
  void add_undo() {
    shared = undoObj(shared);
  }
  void add_afterC() {
    shared = afterCObj(shared);
  }

private:
  friend class Transaction;
  typedef Shared TaggedShared;
  TaggedShared *shared;
public:
  TransData data;
};
