#include <vector>
#include <algorithm>

#include "Interface.hh"

#pragma once

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

class Transaction {
public:
  struct ReaderItem {
    ReaderItem(Reader *r, ReaderData data) : reader(r), data(data) {}
    Reader *reader;
    ReaderData data;
  };
  struct WriterItem {
    WriterItem(Writer *r, WriterData data) : writer(r), data(data) {}
    Writer *writer;
    WriterData data;

    bool operator<(const WriterItem& w2) const {
      return writer->UID(data) < w2.writer->UID(w2.data);
    }
    bool operator==(const WriterItem& w2) const {
      return writer->UID(data) == w2.writer->UID(w2.data);
    }
    bool operator==(const ReaderItem& r2) const {
      return writer->UID(data) == r2.reader->UID(r2.data);
    }
  };

  typedef std::vector<ReaderItem> ReadSet;
  typedef std::vector<WriterItem> WriteSet;

  Transaction() : readSet_(), writeSet_() {}

  void read(Reader *r, ReaderData data) {
    readSet_.emplace_back(r, data);
  }

  void write(Writer *w, WriterData data) {
    writeSet_.emplace_back(w, data);
  }

  bool commit() {
    bool success = true;

    //phase1
    WriteSet sortedWrites(writeSet_);
    std::sort(sortedWrites.begin(), sortedWrites.end());
    sortedWrites.erase(std::unique(sortedWrites.begin(), sortedWrites.end()), sortedWrites.end());

    for (WriterItem& w : sortedWrites) {
      w.writer->lock(w.data);
    }
    //phase2
    for (ReaderItem& r : readSet_) {
      // TODO: binary search?
      if (!r.reader->check(r.data) || (r.reader->is_locked(r.data) && std::find(sortedWrites.begin(), sortedWrites.end(), r) == sortedWrites.end())) {
        success = false;
        goto end;
      }
    }
    //phase3
    // we install in the original order (NOT sorted) so that writes to the same key happen in order
    // TODO: could use stable sort above and then avoid applying duplicate writes to the same location
    for (WriterItem& w : writeSet_) {
      w.writer->install(w.data);
    }

  end:

    // important to iterate through sortedWrites (has no duplicates) so we don't double unlock something
    for (WriterItem& w : sortedWrites) {
      w.writer->unlock(w.data);
    }

    return success;

  }

private:
  ReadSet readSet_;
  WriteSet writeSet_;

};
