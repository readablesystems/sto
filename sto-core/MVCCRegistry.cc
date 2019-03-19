// Implementation of the MVCC Registry, which is responsible for MVCC GC

#include "MVCCRegistry.hh"
#include "MVCCStructs.hh"
#include "Transaction.hh"

__thread size_t MvRegistry::cycles = 0;
MvRegistry MvRegistry::registrar_;

MvRegistry::type MvRegistry::compute_rtid_inf() {
    type rtid_inf = Transaction::_RTID;

    // Find an infimum for the rtid
    for (auto &ti : Transaction::tinfo) {
        if (!rtid_inf) {
            rtid_inf = ti.rtid.load();
        } else if (ti.rtid) {
            rtid_inf = std::min(rtid_inf, ti.rtid.load());
        }
    }

    return rtid_inf;
}

void MvRegistry::collect_(registry_type& registry, const type rtid_inf) {
    if (is_stopping) {
        return;
    }

    while (!registry.empty()) {
        if (is_stopping) {
            return;
        }

        auto entry = registry.front();

        if (rtid_inf <= entry.tid) {
            break;
        }

        registry.pop_front();

        base_type *h = *entry.head;

        if (!h) {
            continue;
        }

        // Find currently-visible version at rtid_inf
        h = h->with(COMMITTED_DELTA, COMMITTED);
        base_type *hnext = nullptr;
        while (rtid_inf <= h->btid()) {
            hnext = h;
            h = h->prev_;
            h = h->with(COMMITTED_DELTA, COMMITTED);
        }
        assert(!h->status_is(ABORTED));
        assert(h->status() == COMMITTED || h->status() == COMMITTED_DELETED);
        assert(rtid_inf > h->btid());
        base_type *garbo = h->prev_;  // First element to collect
        h->prev_ = nullptr;  // Ensures that future collection cycles know
                             // about the progress of previous cycles

        type next_tid = h->btid();
        while (garbo) {
            base_type *delet = garbo;
            garbo = garbo->prev_;
            assert(rtid_inf > delet->wtid());
            assert(rtid_inf > next_tid);
            next_tid = std::min(next_tid, delet->btid());
            if (entry.inlined && (entry.inlined == delet)) {
                delet->status_unused();
            } else {
                Transaction::rcu_delete(delet);
            }
        }

        // There is more gc to potentially do!
        if (hnext) {
            registry.push_back(entry_type(
                entry.head, entry.inlined, hnext->btid(), entry.flag));
        } else {
            *entry.flag = false;
        }
    }
}

void MvRegistry::flatten_(registry_type& registry, const type rtid_inf) {
    if (is_stopping) {
        return;
    }

    std::stack<entry_type> stk;
    while (!registry.empty()) {
        if (is_stopping) {
            return;
        }

        auto entry = registry.front();

        if (rtid_inf <= entry.tid) {
            break;
        }

        stk.push(entry);
        registry.pop_front();

        base_type *h = *entry.head;

        if (!h) {
            continue;
        }

        // Traverse to the gc boundary
        while (rtid_inf <= h->btid()) {
            h = h->prev_;
        }

        // Find the first COMMITTED or COMMITTED_DELTA version
        h = h->with(COMMITTED, COMMITTED);

        if (h->status_is(DELTA)) {
            h->enflatten();
        }
    }

    while (!stk.empty()) {
        registry.push_front(stk.top());
        stk.pop();
    }
}
