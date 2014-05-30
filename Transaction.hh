#include <vector>
#include <algorithm>

#include "Interface.hh"

#pragma once

class Transaction {
public:
  struct ReaderItem {
    ReaderItem(Reader *r, TransData data) : reader(r), data(data) {}
    Reader *reader;
    TransData data;
  };
  struct WriterItem {
    WriterItem(Writer *r, TransData data) : writer(r), data(data) {}
    Writer *writer;
    TransData data;

    bool operator<(const WriterItem& w2) const {
      return data < w2.data;
    }
    bool operator==(const WriterItem& w2) const {
      return data == w2.data;
    }
    bool operator==(const ReaderItem& r2) const {
      return data == r2.data;
    }
  };

  typedef std::vector<ReaderItem> ReadSet;
  typedef std::vector<WriterItem> WriteSet;

  Transaction() : readSet_(), writeSet_(), abortSet_(), commitSet_() {}

  void read(Reader *r, TransData data) {
    readSet_.emplace_back(r, data);
  }

  void write(Writer *w, TransData data) {
    writeSet_.emplace_back(w, data);
  }

  // TODO: should this be a different virtual object or?
  void onAbort(Writer *w, TransData data) {
    abortSet_.emplace_back(w, data);
  }

  void onCommit(Writer *w, TransData data) {
    commitSet_.emplace_back(w, data);
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
      bool isRW = std::find(sortedWrites.begin(), sortedWrites.end(), r) != sortedWrites.end();
      if (!r.reader->check(r.data, isRW)) {
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

    if (success) {
      commitSuccess();
    } else {
      abort();
    }

    return success;

  }

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
  
  ReadSet readSet_;
  WriteSet writeSet_;
  WriteSet abortSet_;
  WriteSet commitSet_;

};
