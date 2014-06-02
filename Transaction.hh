#include <vector>
#include <algorithm>

#include "Interface.hh"

#pragma once

#define READER_BIT 1<<0
#define WRITER_BIT 1<<1

template <typename T>
T* readObj(T* obj) {
  //assert(!isReadObj(obj));
  return (T*)((intptr_t)obj | READER_BIT);
}
template <typename T>
T* writeObj(T* obj) {
  //  assert(!isWriteObj(obj));
  return (T*)((intptr_t)obj | WRITER_BIT);
}
template <typename T>
T* untag(T* obj) {
  return (T*)((intptr_t)obj & ~(WRITER_BIT|READER_BIT));
}
template <typename T>
bool isReadObj(T* obj) {
  return (intptr_t)obj & READER_BIT;
}
template <typename T>
bool isWriteObj(T* obj) {
  return (intptr_t)obj & WRITER_BIT;
}

class Transaction {
public:
  struct TransItem {
    TransData data;
    TransItem(Shared *s, TransData data) : shared(s), data(data) {}

    Shared *sharedObj() {
      return untag(shared);
    }

    bool has_write() {
      return isWriteObj(shared);
    }
    bool has_read() {
      return isReadObj(shared);
    }

    template <typename T>
    T write_value() {
      assert(isWriteObj(shared));
      return unpack<T>(data.wdata);
    }
    template <typename T>
    T read_value() {
      assert(isReadObj(shared));
      return unpack<T>(data.rdata);
    }

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

    inline bool operator<(const TransItem& t2) const {
      return data < t2.data;
    }
    inline bool operator==(const TransItem& t2) const {
      return data == t2.data;
    }

  private:
    typedef Shared TaggedShared;
    TaggedShared *shared;
  };

  typedef std::vector<TransItem> TransSet;

  Transaction() : transSet_() {}

  template <typename T>
  TransItem& item(Shared *s, T key) {
    void *k = pack(key);
    for (TransItem& ti : transSet_) {
      if (ti.sharedObj() == s && ti.data.key == k) {
        return ti;
      }
    }
    // TODO: if user of this forgets to do add_read or add_write, we end up with a non-read, non-write, weirdo item
    transSet_.emplace_back(s, TransData(k, NULL, NULL));
    return transSet_[transSet_.size()-1];
  }

#if 0
  void read(Shared *s, TransData data) {
    transSet_.emplace_back(readObj(r), data);
  }

  void write(Shared *s, TransData data) {
    transSet_.emplace_back(writeObj(w), data);
  }
#endif

#if 0
  // TODO: should this be a different virtual object or?
  void onAbort(Writer *w, TransData data) {
    abortSet_.emplace_back(w, data);
  }

  void onCommit(Writer *w, TransData data) {
    commitSet_.emplace_back(w, data);
  }
#endif

  bool commit() {
    bool success = true;

    //phase1
    std::sort(transSet_.begin(), transSet_.end());

    for (TransItem& ti : transSet_) {
      if (ti.has_write()) {
        ti.sharedObj()->lock(ti.data);
      }
    }
    //phase2
    for (TransItem& ti : transSet_) {
      if (ti.has_read()) {
        bool isRW = ti.has_write();
        if (!ti.sharedObj()->check(ti.data, isRW)) {
          success = false;
          goto end;
        }
      }
    }
    //phase3
    for (TransItem& ti : transSet_) {
      if (ti.has_write()) {
        ti.sharedObj()->install(ti.data);
      }
    }

  end:

    // important to iterate through sortedWrites (has no duplicates) so we don't double unlock something
    for (TransItem& ti : transSet_) {
      if (ti.has_write()) {
        ti.sharedObj()->unlock(ti.data);
      }
    }

#if 0
    if (success) {
      commitSuccess();
    } else {
      abort();
    }
#endif

    return success;

  }

#if 0
  void abort() {
    for (WriterItem& w : abortSet_) {
      w.writer->undo(w.data);
    }
  }

private:
  void commitSuccess() {
    for (WriterItem& w : commitSet_) {
      w.writer->afterT(w.data);
    }
  }
#endif

private:
  TransSet transSet_;

};
