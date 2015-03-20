#pragma once
#include "config.h"
#include "compiler.hh"
#include "Interface.hh"
#include "Transaction.hh"

class TransFree : public Shared {
public:
  // used to free things only if successful commit
  void transFree(Transaction& t, void *ptr) {
    t.new_item(this, ptr).add_write(0);
  }

  void lock(TransItem&) {}
  void unlock(TransItem&) {}
  bool check(const TransItem&, const Transaction&) { assert(0); return false; }
  void install(TransItem& item, TransactionTid) {
    void *ptr = item.template write_value<void*>();
    Transaction::rcu_cleanup([ptr] () { free(ptr); });
  }
};
