#include "MVCCStructs.hh"
#include "Transaction.hh"

void MvHistoryBase::gc_push(const bool inlined) {
    bool expected = false;
    if (gc_enqueued_.compare_exchange_strong(expected, true)) {
        hard_gc_push(inlined);
    }
}

void MvHistoryBase::hard_gc_push(const bool inlined) {
    assert(gc_enqueued_.load());
    if (inlined) {
        Transaction::rcu_call(gc_inlined_cb, this);
    } else {
        Transaction::rcu_delete(this);
    }
}
