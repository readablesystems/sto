#pragma once

#include "Transaction.hh"

class TransUndoable : public TObject {
public:
  typedef void (*UndoFunction)(void*, void*, void*);
  void add_undo(UndoFunction undo_func, void *context1, void *context2) {
    // abusing semantics a bit here: technically there *could* be duplicates, but we're only
    // using this item for cleanup; duplicates shouldn't affect anything.
    // best solution is probably to have only one item for this and stuff everything in there.
    auto item = Sto::new_item(this, undo_func);
    item.set_stash(context1);
    item.add_write(context2);
  }

  // TODO: would be nice to have an add_undo that takes a std::function or something too.


  bool lock(TransItem&, Transaction&) override { return true; }
  void unlock(TransItem&) override {}
  bool check(const TransItem&, const Transaction&) override { return false; }
  void install(TransItem&, const Transaction&) override {}
  void cleanup(TransItem& item, bool committed) override {
    if (!committed) {
      auto undo_func = item.key<UndoFunction>();
      undo_func(this, item.stash_value<void*>(), item.write_value<void*>());
    }
  }
};
