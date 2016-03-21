#pragma once

#include "Transaction.hh"

class TransUndoable : public Shared {
public:
  typedef void (*UndoFunction)(void*, void*, void*);
  void add_undo(UndoFunction undo_func, void *context1, void *context2, bool commitandabort=false) {
    // abusing semantics a bit here: technically there *could* be duplicates, but we're only
    // using this item for cleanup; duplicates shouldn't affect anything.
    // best solution is probably to have only one item for this and stuff everything in there.
    auto item = Sto::new_item(this, undo_func);
    item.set_stash(context1);
    item.add_write(context2);
    if (commitandabort)
      item.add_flags(TransItem::user0_bit);
  }

  // TODO: would be nice to have an add_undo that takes a std::function or something too.


  bool lock(TransItem&, Transaction&) { return true; }
  void unlock(TransItem&) {}
  bool check(const TransItem&, const Transaction&) { return false; }
  void install(TransItem&, const Transaction&) {}

  void cleanup(TransItem& item, bool committed) {
    if (!committed || item.has_flag(TransItem::user0_bit)) {
      auto undo_func = item.key<UndoFunction>();
      undo_func(this, item.stash_value<void*>(), item.write_value<void*>());
    }
  }
};
