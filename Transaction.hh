#include <vector>
#include <algorithm>

#define LOCAL_VECTOR 1
#define PERF_LOGGING 0

#if LOCAL_VECTOR
#include "local_vector.hh"
#endif

#include "Interface.hh"

#pragma once

#define INIT_SET_SIZE 16

#define READER_BIT (1<<0)
#define WRITER_BIT (1<<1)
#define UNDO_BIT (1<<2)

#if PERF_LOGGING
uint64_t total_n;
uint64_t total_r, total_w;
uint64_t total_searched;
#endif

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
T* undoObj(T* obj) {
  return (T*)((intptr_t)obj | UNDO_BIT);
}
template <typename T>
T* untag(T* obj) {
  return (T*)((intptr_t)obj & ~(WRITER_BIT|READER_BIT|UNDO_BIT));
}
template <typename T>
bool isReadObj(T* obj) {
  return (intptr_t)obj & READER_BIT;
}
template <typename T>
bool isWriteObj(T* obj) {
  return (intptr_t)obj & WRITER_BIT;
}
template <typename T>
bool isUndoObj(T* obj) {
  return (intptr_t)obj & UNDO_BIT;
}

class Transaction {
public:
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

    inline bool operator<(const TransItem& t2) const {
      return data < t2.data
        || (data == t2.data && shared < t2.shared);
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

  private:
    friend class Transaction;
    typedef Shared TaggedShared;
    TaggedShared *shared;
  public:
    TransData data;
  };

#if LOCAL_VECTOR
  typedef local_vector<TransItem, INIT_SET_SIZE> TransSet;
#else
  typedef std::vector<TransItem> TransSet;
#endif

  Transaction() : transSet_(), readMyWritesOnly_(true), isAborted_(false) {
#if !LOCAL_VECTOR
    transSet_.reserve(INIT_SET_SIZE);
#endif
  }

  // adds item without checking its presence in the array
  template <bool NOCHECK = true, typename T>
  TransItem& add_item(Shared *s, T key) {
    if (NOCHECK) {
      readMyWritesOnly_ = false;
    }
    void *k = pack(key);
    // TODO: if user of this forgets to do add_read or add_write, we end up with a non-read, non-write, weirdo item
    transSet_.emplace_back(s, TransData(k, NULL, NULL));
    return transSet_[transSet_.size()-1];
  }

  // tries to find an existing item with this key, otherwise adds it
  template <typename T>
  TransItem& item(Shared *s, T key) {
    TransItem *ti;
    if ((ti = has_item(s, key)))
      return *ti;

    return add_item<false>(s, key);
  }

  // tries to find an existing item with this key, returns NULL if not found
  template <typename T>
  TransItem* has_item(Shared *s, T key) {
    void *k = pack(key);
    for (TransItem& ti : transSet_) {
#if PERF_LOGGING
      total_searched++;
#endif
      if (ti.sharedObj() == s && ti.data.key == k) {
        return &ti;
      }
    }
    return NULL;
  }

  template <typename T>
  void add_write(TransItem& ti, T wdata) {
    ti.add_write(wdata);
  }
  template <typename T>
  void add_read(TransItem& ti, T rdata) {
    ti.add_read(rdata);
  }
  void add_undo(TransItem& ti) {
    ti.add_undo();
  }

  bool commit() {
    if (isAborted_)
      return false;

    bool success = true;

#if PERF_LOGGING
    total_n += transSet_.size();
#endif

    //phase1
    if (readMyWritesOnly_) {
      std::sort(transSet_.begin(), transSet_.end());
    } else {
      std::stable_sort(transSet_.begin(), transSet_.end());
    }
    TransItem* trans_first = &transSet_[0];
    TransItem* trans_last = trans_first + transSet_.size();
    for (auto it = trans_first; it != trans_last; )
      if (it->has_write()) {
        TransItem* me = it;
        me->sharedObj()->lock(me->data);
        ++it;
        if (!readMyWritesOnly_)
          for (; it != trans_last && it->same_item(*me); ++it)
            /* do nothing */;
      } else
        ++it;

    /* fence(); */

    //phase2
    for (auto it = trans_first; it != trans_last; ++it)
      if (it->has_read()) {
#if PERF_LOGGING
        total_r++;
#endif
        bool has_write = it->has_write();
        if (!has_write && !readMyWritesOnly_)
          for (auto it2 = it + 1;
               it2 != trans_last && it2->same_item(*it);
               ++it2)
            if (it2->has_write()) {
              has_write = true;
              break;
            }
        if (!it->sharedObj()->check(it->data, has_write)) {
          success = false;
          goto end;
        }
      }
    
    //phase3
    for (TransItem& ti : transSet_) {
      if (ti.has_write()) {
#if PERF_LOGGING
        total_w++;
#endif
        ti.sharedObj()->install(ti.data);
      }
    }
    
  end:

    for (auto it = trans_first; it != trans_last; )
      if (it->has_write()) {
        TransItem* me = it;
        me->sharedObj()->unlock(me->data);
        ++it;
        if (!readMyWritesOnly_)
          for (; it != trans_last && it->same_item(*me); ++it)
            /* do nothing */;
      } else
        ++it;
    
    if (success) {
      commitSuccess();
    } else {
      abort();
    }

    return success;
  }


  void abort() {
    isAborted_ = true;
    for (auto& ti : transSet_) {
      if (ti.has_undo()) {
        ti.sharedObj()->undo(ti.data);
      }
    }
    throw Abort();
  }

  bool aborted() {
    return isAborted_;
  }

  class Abort {};

private:

  void commitSuccess() {
#if 0
    for (WriterItem& w : commitSet_) {
      w.writer->afterT(w.data);
    }
#endif
  }

private:
  TransSet transSet_;
  bool readMyWritesOnly_;
  bool isAborted_;

};
