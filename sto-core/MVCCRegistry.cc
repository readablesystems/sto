#include "MVCCRegistry.hh"
#include "MVCCStructs.hh"
#include "Transaction.hh"

MvRegistry MvRegistry::registrar;

void MvRegistry::collect_garbage_() {
    if (stopped) {
        return;
    }
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

    // Do the actual cleanup
    for (registry_type &registry : registries) {
        MvRegistryEntry *entry = registry;
        while (entry) {
            auto curr = entry;
            entry = entry->next;
            bool valid = curr->valid;

            if (!valid) {
                continue;
            }

            base_type *h = *curr->atomic_base;

            if (!h) {
                continue;
            }

            // Find currently-visible version at gc_tid
            while (gc_tid < h->wtid_) {
                h = h->prev_;
            }
            base_type *garbo = h->prev_;  // First element to collect
            h->prev_ = nullptr;  // Ensures that future collection cycles know
                                 // about the progress of previous cycles

            while (h->status_is(MvStatus::ABORTED)) {
                h = h->prev_;
            }
            if (h->status_is(MvStatus::DELTA)) {
                h->enflatten();
            }

            while (garbo) {
                base_type *delet = garbo;
                garbo = garbo->prev_;
                if (curr->inlined && (curr->inlined == delet)) {
                    delet->status_unused();
                } else {
                    Transaction::rcu_delete(delet);
                }
            }
        }
    }
}
