#include "Transaction.hh"

Transaction::testing_type Transaction::testing;
threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int Transaction::threadid;
threadinfo_t::epoch_type __attribute__((aligned(64))) Transaction::global_epoch;
bool Transaction::run_epochs = true;
__thread Transaction *Sto::__transaction;
std::function<void(threadinfo_t::epoch_type)> Transaction::epoch_advance_callback;
TransactionTid::type __attribute__((aligned(128))) Transaction::_TID = TransactionTid::valid_bit;

static void __attribute__((used)) check_static_assertions() {
    static_assert(sizeof(threadinfo_t) % 128 == 0, "threadinfo is 2-cache-line aligned");
}

void* Transaction::epoch_advancer(void*) {
    // don't bother epoch'ing til things have picked up
    usleep(100000);
    while (run_epochs) {
        auto g = global_epoch;
        for (auto&& t : tinfo) {
            if (t.epoch != 0 && t.epoch < g)
                g = t.epoch;
        }

        global_epoch = ++g;

        if (epoch_advance_callback)
            epoch_advance_callback(global_epoch);

        for (auto&& t : tinfo) {
            acquire_spinlock(t.spin_lock);
            auto deletetil = t.callbacks.begin();
            for (auto it = t.callbacks.begin(); it != t.callbacks.end(); ++it) {
                if (threadinfo_t::signed_epoch_type(g - it->first) >= 2) {
                    it->second();
                    ++deletetil;
                } else {
                    // callbacks are in ascending order so if this one is too soon of an epoch the rest will be too
                    break;
                }
            }
            if (t.callbacks.begin() != deletetil) {
                t.callbacks.erase(t.callbacks.begin(), deletetil);
            }
            auto deletetil2 = t.needs_free.begin();
            for (auto it = t.needs_free.begin(); it != t.needs_free.end(); ++it) {
                if (threadinfo_t::signed_epoch_type(g - it->first) >= 2) {
                    free(it->second);
                    ++deletetil2;
                } else {
                    break;
                }
            }
            if (t.needs_free.begin() != deletetil2) {
                t.needs_free.erase(t.needs_free.begin(), deletetil2);
            }
            release_spinlock(t.spin_lock);
        }
        usleep(100000);
    }
    return NULL;
}

void Transaction::update_hash() {
    if (nhashed_ == 0)
        memset(hashtable_, 0, sizeof(hashtable_));
    for (auto it = transSet_.begin() + nhashed_; it != transSet_.end(); ++it, ++nhashed_) {
        int h = hash(it->owner(), it->key_);
        if (!hashtable_[h] || !may_duplicate_items_)
            hashtable_[h] = nhashed_ + 1;
    }
}

void Transaction::hard_check_opacity(TransactionTid::type t) {
    INC_P(txp_hco);
    if (t & TransactionTid::lock_bit)
        INC_P(txp_hco_lock);
    if (!(t & TransactionTid::valid_bit))
        INC_P(txp_hco_invalid);

    TransactionTid::type newstart = _TID;
    release_fence();
    if (!(t & TransactionTid::lock_bit) && check_reads(transSet_.begin(), transSet_.end()))
        start_tid_ = newstart;
    else {
        INC_P(txp_hco_abort);
        abort();
    }
}

bool Transaction::try_commit() {
#if ASSERT_TX_SIZE
    if (transSet_.size() > TX_SIZE_LIMIT) {
        std::cerr << "transSet_ size at " << transSet_.size()
            << ", abort." << std::endl;
        assert(false);
    }
#endif
    MAX_P(txp_max_set, transSet_.size());
    ADD_P(txp_total_n, transSet_.size());

    if (state_ >= s_aborted)
        return state_ > s_aborted;

    state_ = s_committing;

    if (firstWrite_ < 0)
        firstWrite_ = transSet_.size();

    int writeset_alloc[transSet_.size() - firstWrite_];
    writeset_ = writeset_alloc;
    nwriteset_ = 0;

    for (auto it = transSet_.begin(); it != transSet_.end(); ++it) {
        if (it->has_write())
            writeset_[nwriteset_++] = it - transSet_.begin();
        else if (it->has_predicate()) {
            if (!it->owner()->check_predicate(*it, *this))
                goto abort;
        }
#ifdef DETAILED_LOGGING
        if (it->has_read())
            INC_P(txp_total_r);
#endif
    }

    //phase1
#if !NOSORT
    std::sort(writeset_, writeset_ + nwriteset_, [&] (int i, int j) {
        return transSet_[i] < transSet_[j];
    });
#endif
    {
        auto writeset_end = writeset_ + nwriteset_;
        for (auto it = writeset_; it != writeset_end; ) {
            TransItem* me = &transSet_[*it];
            me->owner()->lock(*me);
            me->__or_flags(TransItem::lock_bit);
            ++it;
            if (may_duplicate_items_)
                for (; it != writeset_end && transSet_[*it].same_item(*me); ++it)
                    /* do nothing */;
        }
    }


#if CONSISTENCY_CHECK
    fence();
    commit_tid();
    fence();
#endif

    //phase2
    if (!check_reads(transSet_.begin(), transSet_.end()))
        goto abort;

    // fence();

    //phase3
    for (auto it = transSet_.begin() + firstWrite_; it != transSet_.end(); ++it) {
        TransItem& ti = *it;
        if (ti.has_write()) {
            INC_P(txp_total_w);
            ti.owner()->install(ti, *this);
        }
    }

    // fence();
    stop(true);
    return true;

abort:
    // fence();
    INC_P(txp_commit_time_aborts);
    if (state_ == s_committing)
        stop(false);
    return false;
}

void Transaction::print_stats() {
    threadinfo_t out = tinfo_combined();
    if (txp_count >= txp_max_set) {
        fprintf(stderr, "$ %llu starts, %llu max read set, %llu commits",
                out.p(txp_total_starts),
                out.p(txp_max_set),
                out.p(txp_total_starts) - out.p(txp_total_aborts));
        if (out.p(txp_total_aborts)) {
            fprintf(stderr, ", %llu (%.3f%%) aborts",
                    out.p(txp_total_aborts),
                    100.0 * (double) out.p(txp_total_aborts) / out.p(txp_total_starts));
            if (out.p(txp_commit_time_aborts))
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts at commit time",
                        out.p(txp_commit_time_aborts),
                        100.0 * (double) out.p(txp_commit_time_aborts) / out.p(txp_total_aborts));
        }
        fprintf(stderr, "\n");
    }
    if (txp_count >= txp_hco_abort)
        fprintf(stderr, "$ %llu HCO (%llu lock, %llu invalid, %llu aborts)\n",
                out.p(txp_hco), out.p(txp_hco_lock), out.p(txp_hco_invalid), out.p(txp_hco_abort));
}

void TransactionBuffer::hard_clear(bool delete_all) {
    while (e_ && e_->next) {
        elt* e = e_->next;
        e_->next = e->next;
        e->clear();
        delete[] (char*) e;
    }
    if (e_)
        e_->clear();
    if (e_ && delete_all) {
        delete[] (char*) e_;
        e_ = 0;
    }
}

void Transaction::print(FILE* f) const {
    static const char* names[] = {"in-progress", "committing", "aborted", "committed"};
    fprintf(f, "T%p %s [", this, names[state_]);
    for (auto& ti : transSet_)
        ti.owner()->print(f, ti);
    fprintf(f, "]\n");
}

void Shared::print(FILE* f, const TransItem& item) const {
    fprintf(f, "<%p.%p", this, item.key<void*>());
    if (item.has_read())
        fprintf(f, " r%p", item.read_value<void*>());
    if (item.has_write())
        fprintf(f, " w%p", item.write_value<void*>());
    if (item.has_predicate())
        fprintf(f, " p%p", item.predicate_value<void*>());
    fprintf(f, ">");
}
