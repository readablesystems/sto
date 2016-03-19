#pragma once

#include "Transaction.hh"

class TransUndoable : public Shared {
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


  void lock(TransItem&) {}
  void unlock(TransItem&) {}
  bool check(TransItem&, Transaction&) {}
  void install(TransItem&) {}

  void cleanup(TransItem& item, bool committed) {
    if (!committed) {
      auto undo_func = item.key<CallbackFunction>();
      undo_func(item.stash_value<void*>(), item.write_value<void*>());
    }
  }
}
