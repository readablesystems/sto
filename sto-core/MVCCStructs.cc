#include "MVCCStructs.hh"
#include "Transaction.hh"

void MvHistoryBase::gc_push(const bool inlined) {
    bool expected = false;
    auto& thr = Transaction::tinfo[TThread::id()];
    if (gc_enqueued_.compare_exchange_strong(expected, true)) {
        if (gc_epoch()) {
            if (inlined) {
                thr.rcu_set.add(gc_epoch(), gc_inlined_cb, this);
            } else {
                thr.rcu_set.add(
                    gc_epoch(), Transaction::rcu_delete_cb<base_type>, this);
            }
        } else {
            gc_epoch(gc_epoch(), thr.epoch);
            if (inlined) {
                Transaction::rcu_call(gc_inlined_cb, this);
            } else {
                Transaction::rcu_delete(this);
            }
        }
    }
}
