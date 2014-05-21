
class TransState {

  class AlreadyIn{};
  class NotIn{};

  size_t hashFn(Shared p) {
    return (size_t)p.UID();
  }

  typedef std::vector<Reader> ReadSet;
  typedef std::vector<Writer> WriteSet;

  ReadSet readSet_;
  WriteSet writeSet_;

  TransState() : readSet_(), writeSet_() {}

  void read(Reader r) {
    if (td.readSet.count(p) == 0) {
      td.readSet[p] = context;
    }
    assert(td.readSet[p] == context);
  }

  void write(Writer r) {

  }

  void logReadWrite(Shared p, void *context) {
    logRead(p, context);
    logWrite(p);
  }

  void logWrite(Shared p) {
    td.writeSet.insert(p);
  }

  void commit() {
    if (!td.active) {
      throw NotIn();
    }
    //phase1
    for (Shared& p : td.writeSet) {
      
    }
    //phase2
    for (auto&& pair : td.readSet) {
      if (!pair.first.check(pair.second)) {
        // abort
      }
    }
    //phase3
    for (Shared& p : td.writeSet) {
      p.install(td.readSet[p])
    }

    td.readSet.clear();
    td.writeSet.clear();
  }

};
