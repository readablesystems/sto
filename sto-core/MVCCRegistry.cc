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

void MvRegistry::collect_(const size_t index, const type rtid_inf) {
    auto& registry = this->registry(index);
    if (is_stopping) {
        return;
    }
#if STO_PROFILE_COUNTERS > 1
    collect_call_cnts[index]++;
#endif
    while (!registry.empty()) {
        if (is_stopping) {
            return;
        }

        auto entry = registry.front();

        if (rtid_inf <= entry.tid) {
            break;
        }

        registry.pop_front();

        base_type *h = nullptr;
        base_type *hnext = nullptr;

        if (entry.base_version) {
#if STO_PROFILE_COUNTERS > 1
        collect_up_call_cnts[index]++;
#endif

            h = entry.base_version;

#if STO_PROFILE_COUNTERS > 1
        collect_up_visit_cnts[index]++;
#endif

            while (h->next_ && rtid_inf > h->next_.load()->btid()) {
#if STO_PROFILE_COUNTERS > 1
            collect_up_visit_cnts[index]++;
#endif
                h = h->next_;
            }
            hnext = h->next_;

            while (rtid_inf <= h->btid()) {
#if STO_PROFILE_COUNTERS > 1
                collect_up_visit_cnts[index]++;
#endif
                hnext = h;
                h = h->prev_;
#if STO_PROFILE_COUNTERS > 1
                h = h->with(COMMITTED_DELTA, COMMITTED, collect_up_visit_cnts + index);
#else
                h = h->with(COMMITTED_DELTA, COMMITTED);
#endif
            }
        } else {
#if STO_PROFILE_COUNTERS > 1
            collect_down_call_cnts[index]++;
#endif

            h = *entry.head;

            if (!h) {
                continue;
            }

#if STO_PROFILE_COUNTERS > 1
            collect_down_visit_cnts[index]++;
#endif

            // Find currently-visible version at rtid_inf
#if STO_PROFILE_COUNTERS > 1
            h = h->with(COMMITTED_DELTA, COMMITTED, collect_down_visit_cnts + index);
#else
            h = h->with(COMMITTED_DELTA, COMMITTED);
#endif
            hnext = h->next_;

            while (rtid_inf <= h->btid()) {
#if STO_PROFILE_COUNTERS > 1
                collect_down_visit_cnts[index]++;
#endif
                hnext = h;
                h = h->prev_;
#if STO_PROFILE_COUNTERS > 1
                h = h->with(COMMITTED_DELTA, COMMITTED, collect_down_visit_cnts + index);
#else
                h = h->with(COMMITTED_DELTA, COMMITTED);
#endif
            }
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
#if STO_PROFILE_COUNTERS > 1
            collect_free_cnts[index]++;
#endif

            if (entry.inlined && (entry.inlined == delet)) {
                delet->status_unused();
            } else {
                Transaction::rcu_delete(delet);
            }
        }

        // There is more gc to potentially do!
        if (hnext) {
#if STO_PROFILE_COUNTERS > 1
            if (entry.base_version && !h) {
                convert_up_down_cnts[index]++;
            } else if (!entry.base_version && h) {
                convert_down_up_cnts[index]++;
            }
#endif
            entry.base_version = h;
            registry.push_back(entry_type(
                entry.head, entry.inlined, entry.base_version, hnext->btid(),
                entry.flag));
        } else {
#if STO_PROFILE_COUNTERS > 1
            if (entry.base_version) {
                convert_up_down_cnts[index]++;
            }
#endif
            entry.base_version = nullptr;
            *entry.flag = false;
        }
    }
}

void MvRegistry::flatten_(size_t index, const type rtid_inf) {
    auto& registry = this->registry(index);
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

        // Find the newest COMMITTED or COMMITTED_DELTA version
        h = h->with(COMMITTED, COMMITTED);

        // Traverse to the gc boundary & update next pointers
        base_type *next = nullptr;
        if (h->wtid() < Transaction::_RTID &&
                ((h->status() & (COMMITTED | DELTA)) == COMMITTED)) {
            next = h;
        }
        while (rtid_inf <= h->btid()) {
            h = h->prev_;
            h = h->with(COMMITTED, COMMITTED);
            h->next(next);
            if (h->wtid() < Transaction::_RTID &&
                    ((h->status() & (COMMITTED | DELTA)) == COMMITTED)) {
                next = h;
            }
        }

        if (h->status_is(DELTA)) {
            h->enflatten();
        }
    }

    while (!stk.empty()) {
        registry.push_front(stk.top());
        stk.pop();
    }
}

void MvRegistry::print_counters() {
#if STO_PROFILE_COUNTERS > 1
    for (size_t i = 0; i < MAX_THREADS; ++i) {
        auto c = registrar().collect_call_cnts[i];
        auto dc = registrar().collect_down_call_cnts[i];
        auto uc = registrar().collect_up_call_cnts[i];
        auto d = registrar().collect_down_visit_cnts[i];
        auto u = registrar().collect_up_visit_cnts[i];
        auto v = d + u;
        auto f = registrar().collect_free_cnts[i];
        auto d2u = registrar().convert_down_up_cnts[i];
        auto u2d = registrar().convert_up_down_cnts[i];
        if (v == 0)
            continue;

        printf("t-%02lu: c:%lu (%lu down, %lu up), v:%lu (%lu down, %lu up), f:%lu\n",
                i, c, dc, uc, v, d, u, f);
        printf("  η:%04.2f%%, down->up:%lu, up->down:%lu\n",
               (double)f / (double)v * 100.0, d2u, u2d);
    }
#endif
}
