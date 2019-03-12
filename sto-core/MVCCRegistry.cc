// Implementation of the MVCC Registry, which is responsible for MVCC GC

#include "MVCCRegistry.hh"
#include "MVCCStructs.hh"
#include "Transaction.hh"

MvRegistry MvRegistry::registrar;

MvRegistry::type MvRegistry::compute_gc_tid() {
    type gc_tid = Transaction::_RTID;

    // Find the gc tid
    for (auto &ti : Transaction::tinfo) {
        if (!gc_tid) {
            gc_tid = ti.rtid;
        } else if (ti.rtid) {
            gc_tid = std::min(gc_tid, ti.rtid);
        }
    }
    if (gc_tid) {
        gc_tid--;  // One less than the oldest rtid, unless that's 0...
    }

    return gc_tid;
}

void MvRegistry::collect_(registry_type &registry, const type gc_tid) {
    if (is_stopping) {
        return;
    }

    while (!registry.empty()) {
        if (is_stopping) {
            return;
        }

        auto entry = registry.front();

        if (entry.tid > gc_tid) {
            break;
        }

        registry.pop_front();

        base_type *h = *entry.head;

        if (!h) {
            continue;
        }

        // Find currently-visible version at gc_tid
        while (h->status_is(MvStatus::ABORTED)) {
            h = h->prev_;
        }
        base_type *hhead = h;
        base_type *hnext = hhead;
        while (gc_tid < h->wtid()) {
            hnext = h;
            h = h->prev_;
            while (h->status_is(MvStatus::ABORTED)) {
                h = h->prev_;
            }
        }
        if (h->status_is(MvStatus::DELTA)) {
            h->enflatten();
        }
        base_type *garbo = h->prev_;  // First element to collect
        h->prev_ = nullptr;  // Ensures that future collection cycles know
                             // about the progress of previous cycles

        while (garbo) {
            base_type *delet = garbo;
            garbo = garbo->prev_;
            if (entry.inlined && (entry.inlined == delet)) {
                delet->status_unused();
            } else {
                Transaction::rcu_delete(delet);
            }
        }

        // There is more gc to potentially do!
        if (hnext != hhead) {
            registry.push_back(entry_type(
                entry.head, entry.inlined, hnext->wtid(), entry.flag));
        } else {
            *entry.flag = false;
        }
    }
}
