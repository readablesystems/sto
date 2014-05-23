#include <vector>

#include "Interface.hh"

#pragma once

class TransState {
public:
  struct ReaderItem {
    ReaderItem(Reader *r, void *d1, void *d2) : reader(r), data1(d1), data2(d2) {}
    Reader *reader;
    void *data1;
    void *data2;
  };
  struct WriterItem {
    WriterItem(Writer *w, void *d1, void *d2) : writer(w), data1(d1), data2(d2) {}
    Writer *writer;
    void *data1;
    void *data2;

    bool operator<(const WriterItem& w2) const {
      return writer->UID(data1, data2) < w2.writer->UID(w2.data1, w2.data2);
    }
    bool operator==(const WriterItem& w2) const {
      return writer->UID(data1, data2) == w2.writer->UID(w2.data1, w2.data2);
    }
  };

  typedef std::vector<ReaderItem> ReadSet;
  typedef std::vector<WriterItem> WriteSet;

  TransState() : readSet_(), writeSet_() {}

  void read(Reader *r, void *data1, void *data2) {
    readSet_.emplace_back(r, data1, data2);
  }

  void write(Writer *w, void *data1, void *data2) {
    writeSet_.emplace_back(w, data1, data2);
  }

  bool commit() {
    bool success = true;

    //phase1
    WriteSet sortedWrites(writeSet_);
    std::sort(sortedWrites.begin(), sortedWrites.end());
    sortedWrites.erase(std::unique(sortedWrites.begin(), sortedWrites.end()), sortedWrites.end());

    for (WriterItem& w : sortedWrites) {
      w.writer->lock(w.data1,w.data2);
    }
    //phase2
    for (ReaderItem& r : readSet_) {
      if (!r.reader->check(r.data1,r.data2)) {
        success = false;
        goto end;
      }
    }
    //phase3
    // we install in the original order (NOT sorted) so that writes to the same key happen in order
    for (WriterItem& w : writeSet_) {
      w.writer->install(w.data1,w.data2);
    }

  end:

    // important to iterate through sortedWrites (has no duplicates) so we don't double unlock something
    for (WriterItem& w : sortedWrites) {
      w.writer->unlock(w.data1,w.data2);
    }

    return success;

  }

private:
  ReadSet readSet_;
  WriteSet writeSet_;

};
