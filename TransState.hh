#include <vector>

#include "Interface.hh"

#pragma once

class TransState {
public:
  typedef std::vector<Reader> ReadSet;
  typedef std::vector<Writer> WriteSet;

  TransState() : readSet_(), writeSet_() {}

  void read(Reader r) {
    readSet_.push_back(r);
  }

  void write(Writer w) {
    writeSet_.push_back(w);
  }

  void commit() {
    //phase1
    std::vector<Writer> sortedWrites(writeSet_);
    std::sort(sortedWrites.begin(), sortedWrites.end(), [] (Writer& w1, Writer& w2) {
        return w1.UID() < w2.UID();
    });
    for (Writer& w : sortedWrites) {
      w.lock();
    }
    //phase2
    for (Reader& r : readSet_) {
      if (!r.check()) {
        goto end;
      }
    }
    //phase3
    // we install in the original order (NOT sorted) so that writes to the same key happen in order
    for (Writer& w : writeSet_) {
      w.install();
    }

  end:

    for (Writer& w : writeSet_) {
      w.unlock();
    }

    readSet_.clear();
    writeSet_.clear();

  }

private:
  ReadSet readSet_;
  WriteSet writeSet_;

};
