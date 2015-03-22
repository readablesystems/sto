#pragma once
#include "config.h"
#include "compiler.hh"
#include "Interface.hh"
#include "Transaction.hh"

class TransAlloc : public Shared {
public:
  static constexpr int alloc_flag = 1;
  
  // used to free things only if successful commit
  void transFree(Transaction& t, void *ptr) {
    t.new_item(this, ptr).add_write(0);
  }

  // malloc() which will be freed on abort
  void* transMalloc(Transaction& t, size_t sz) {
    void *ptr = malloc(sz);
    t.new_item(this, ptr).add_write(alloc_flag);
    return ptr;
  }

  void lock(TransItem&) {}
  void unlock(TransItem&) {}
  bool check(const TransItem&, const Transaction&) { assert(0); return false; }
  void install(TransItem& item, TransactionTid) {
    if (item.write_value<int>() == alloc_flag)
      return;
    void *ptr = item.key<void*>();
    Transaction::rcu_free(ptr);
  }
  void cleanup(TransItem& item, bool committed) {
    if (!committed && item.write_value<int>() == alloc_flag)
      free(item.key<void*>());
  }
};
