#include "MVCCRegistry.hh"
#include "MVCCStructs.hh"
#include "Transaction.hh"

MvRegistry MvRegistry::registrar;

void MvRegistry::collect_garbage_() {
    type gc_tid = 0;

    // Find the gc tid
    for (auto &ti : Transaction::tinfo) {
        if (!gc_tid) {
            gc_tid = ti.rtid;
        } else {
            gc_tid = std::min(gc_tid, ti.rtid);
        }
    }

    // Do the actual cleanup
    for (registry_type &registry : registries) {
        MvRegistryEntry *entry = registry.load();
        while (entry) {
            base_type *h = entry->atomic_base->load();
            entry = entry->next;

            // Find currently-visible version at gc_tid
            bool found_committed = h->status_is(MvStatus::COMMITTED) && !h->status_is(MvStatus::DELTA);
            while (gc_tid < h->wtid_ || !found_committed) {
                h = h->prev_;
                found_committed = h->status_is(MvStatus::COMMITTED) && !h->status_is(MvStatus::DELTA);
            }

            base_type *garbo = h->prev_;  // First element to collect
            h->prev_ = nullptr;     // Ensures that future collection cycles know
                                    // about the progress of previous cycles
            while (garbo) {
                base_type *delet = garbo;
                garbo = garbo->prev_;
                Transaction::rcu_delete(delet);
            }
        }
    }
}
