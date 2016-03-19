#include "Transaction.hh"
#include <typeinfo>

Transaction::testing_type Transaction::testing;
threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int TThread::the_id;
Transaction::epoch_state __attribute__((aligned(128))) Transaction::global_epochs = {
    1, 0, TransactionTid::increment_value, true
};
__thread Transaction *TThread::txn = nullptr;
std::function<void(threadinfo_t::epoch_type)> Transaction::epoch_advance_callback;
TransactionTid::type __attribute__((aligned(128))) Transaction::_TID = 2 * TransactionTid::increment_value;
   // reserve TransactionTid::increment_value for prepopulated

static void __attribute__((used)) check_static_assertions() {
    static_assert(sizeof(threadinfo_t) % 128 == 0, "threadinfo is 2-cache-line aligned");
}

void* Transaction::epoch_advancer(void*) {
    // don't bother epoch'ing til things have picked up
    usleep(100000);
    while (global_epochs.run) {
        epoch_type g = global_epochs.global_epoch;
        epoch_type e = g;
        for (auto& t : tinfo) {
            if (t.epoch != 0 && signed_epoch_type(t.epoch - e) < 0)
                e = t.epoch;
        }
        global_epochs.global_epoch = std::max(g + 1, epoch_type(1));
        global_epochs.active_epoch = e;
        global_epochs.recent_tid = Transaction::_TID;

        if (epoch_advance_callback)
            epoch_advance_callback(global_epochs.global_epoch);

        usleep(100000);
    }
    return NULL;
}

void Transaction::hard_check_opacity(TransItem* item, TransactionTid::type t) {
    // ignore opacity checks during commit; we're in the middle of checking
    // things anyway
    if (state_ == s_committing || state_ == s_committing_locked)
        return;

    TXP_INCREMENT(txp_hco);
    if (TransactionTid::is_locked_elsewhere(t, threadid_)) {
        TXP_INCREMENT(txp_hco_lock);
        mark_abort_because(item, "locked", t);
    abort:
        TXP_INCREMENT(txp_hco_abort);
        abort();
    }
    if (t & TransactionTid::nonopaque_bit)
        TXP_INCREMENT(txp_hco_invalid);

    start_tid_ = _TID;
    release_fence();
    for (auto it = transSet_.begin(); it != transSet_.end(); ++it)
        if (it->has_read()) {
            TXP_INCREMENT(txp_total_check_read);
            if (!it->owner()->check(*it, *this)
                && !preceding_read_exists(*it)) {
                mark_abort_because(item, "opacity check");
                goto abort;
            }
        } else if (it->has_predicate()) {
            TXP_INCREMENT(txp_total_check_predicate);
            if (!it->owner()->check_predicate(*it, *this, false)) {
                mark_abort_because(item, "opacity check_predicate");
                goto abort;
            }
        }
}

void Transaction::stop(bool committed) {
    if (!committed) {
        TXP_INCREMENT(txp_total_aborts);
#if STO_DEBUG_ABORTS
        if (local_random() <= uint32_t(0xFFFFFFFF * STO_DEBUG_ABORTS_FRACTION)) {
            std::ostringstream buf;
            buf << "$" << (threadid_ < 10 ? "0" : "") << threadid_
                << " abort " << state_name(state_);
            if (abort_reason_)
                buf << " " << abort_reason_;
            if (abort_item_)
                buf << " " << *abort_item_;
            if (abort_version_)
                buf << " V" << TVersion(abort_version_);
            buf << '\n';
            std::cerr << buf.str();
        }
#endif
    }

    TXP_ACCOUNT(txp_max_transbuffer, buf_.size());
    TXP_ACCOUNT(txp_total_transbuffer, buf_.size());
    if (any_writes_ && state_ == s_committing_locked) {
        for (auto it = transSet_.begin() + first_write_; it != transSet_.end(); ++it)
            if (it->needs_unlock())
                it->owner()->unlock(*it);
    }
    if (any_writes_) {
        for (auto it = transSet_.begin() + first_write_; it != transSet_.end(); ++it)
            if (it->has_write())
                it->owner()->cleanup(*it, committed);
    }
    // TODO: this will probably mess up with nested transactions
    threadinfo_t& thr = tinfo[TThread::id()];
    if (thr.trans_end_callback)
        thr.trans_end_callback();
    // XXX should reset trans_end_callback after calling it...
    state_ = s_aborted + committed;
}

bool Transaction::try_commit() {
    assert(TThread::id() == threadid_);
#if ASSERT_TX_SIZE
    if (transSet_.size() > TX_SIZE_LIMIT) {
        std::cerr << "transSet_ size at " << transSet_.size()
            << ", abort." << std::endl;
        assert(false);
    }
#endif
    TXP_ACCOUNT(txp_max_set, transSet_.size());
    TXP_ACCOUNT(txp_total_n, transSet_.size());

    assert(state_ == s_in_progress || state_ >= s_aborted);
    if (state_ >= s_aborted)
        return state_ > s_aborted;

    if (any_nonopaque_)
        TXP_INCREMENT(txp_commit_time_nonopaque);
#if !CONSISTENCY_CHECK
    // commit immediately if read-only transaction with opacity
    if (!any_writes_ && !any_nonopaque_) {
        stop(true);
        return true;
    }
#endif

    state_ = s_committing;

    int writeset[transSet_.size()];
    int nwriteset = 0;
    writeset[0] = transSet_.size();

    for (auto it = transSet_.begin(); it != transSet_.end(); ++it) {
        if (it->has_write()) {
            writeset[nwriteset++] = it - transSet_.begin();
#if !STO_SORT_WRITESET
            if (nwriteset == 1) {
                first_write_ = writeset[0];
                state_ = s_committing_locked;
            }
            if (!it->owner()->lock(*it, *this)) {
                mark_abort_because(it, "commit lock");
                goto abort;
            }
            it->__or_flags(TransItem::lock_bit);
#endif
        }
        if (it->has_predicate()) {
            TXP_INCREMENT(txp_total_check_predicate);
            if (!it->owner()->check_predicate(*it, *this, true)) {
                mark_abort_because(it, "commit check_predicate");
                goto abort;
            }
        }
        if (it->has_read())
            TXP_INCREMENT(txp_total_r);
    }

    first_write_ = writeset[0];

    //phase1
#if STO_SORT_WRITESET
    std::sort(writeset, writeset + nwriteset, [&] (int i, int j) {
        return transSet_[i] < transSet_[j];
    });

    if (nwriteset) {
        state_ = s_committing_locked;
        auto writeset_end = writeset + nwriteset;
        for (auto it = writeset; it != writeset_end; ) {
            TransItem* me = &transSet_[*it];
            if (!me->owner()->lock(*me, *this)) {
                mark_abort_because(me, "commit lock");
                goto abort;
            }
            me->__or_flags(TransItem::lock_bit);
            ++it;
# if 0
            if (may_duplicate_items_)
                for (; it != writeset_end && transSet_[*it].same_item(*me); ++it)
                    /* do nothing */;
# endif
        }
    }
#endif


#if CONSISTENCY_CHECK
    fence();
    commit_tid();
    fence();
#endif

    //phase2
    for (auto it = transSet_.begin(); it != transSet_.end(); ++it)
        if (it->has_read()) {
            TXP_INCREMENT(txp_total_check_read);
            if (!it->owner()->check(*it, *this)
                && !preceding_read_exists(*it)) {
                mark_abort_because(it, "commit check");
                goto abort;
            }
        }

    // fence();

    //phase3
#if STO_SORT_WRITESET
    for (auto it = transSet_.begin() + first_write_; it != transSet_.end(); ++it) {
        TransItem& ti = *it;
        if (ti.has_write()) {
            TXP_INCREMENT(txp_total_w);
            ti.owner()->install(ti, *this);
        }
    }
#else
    if (nwriteset) {
        auto writeset_end = writeset + nwriteset;
        for (auto it = writeset; it != writeset_end; ++it) {
            TransItem* me = &transSet_[*it];
            TXP_INCREMENT(txp_total_w);
            me->owner()->install(*me, *this);
        }
    }
#endif

    // fence();
    stop(true);
    return true;

abort:
    // fence();
    TXP_INCREMENT(txp_commit_time_aborts);
    stop(false);
    return false;
}

void Transaction::print_stats() {
    txp_counters out = txp_counters_combined();
    if (txp_count >= txp_max_set) {
        unsigned long long txc_total_starts = out.p(txp_total_starts);
        unsigned long long txc_total_aborts = out.p(txp_total_aborts);
        unsigned long long txc_commit_aborts = out.p(txp_commit_time_aborts);
        unsigned long long txc_total_commits = txc_total_starts - txc_total_aborts;
        fprintf(stderr, "$ %llu starts, %llu max read set, %llu commits",
                txc_total_starts, out.p(txp_max_set), txc_total_commits);
        if (txc_total_aborts) {
            fprintf(stderr, ", %llu (%.3f%%) aborts",
                    out.p(txp_total_aborts),
                    100.0 * (double) out.p(txp_total_aborts) / out.p(txp_total_starts));
            if (out.p(txp_commit_time_aborts))
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts at commit time",
                        out.p(txp_commit_time_aborts),
                        100.0 * (double) out.p(txp_commit_time_aborts) / out.p(txp_total_aborts));
        }
        unsigned long long txc_commit_attempts = txc_total_starts - (txc_total_aborts - txc_commit_aborts);
        fprintf(stderr, "\n$ %llu commit attempts, %llu (%.3f%%) nonopaque\n",
                txc_commit_attempts, out.p(txp_commit_time_nonopaque),
                100.0 * (double) out.p(txp_commit_time_nonopaque) / txc_commit_attempts);
    }
    if (txp_count >= txp_hco_abort)
        fprintf(stderr, "$ %llu HCO (%llu lock, %llu invalid, %llu aborts)\n",
                out.p(txp_hco), out.p(txp_hco_lock), out.p(txp_hco_invalid), out.p(txp_hco_abort));
    if (txp_count >= txp_hash_collision)
        fprintf(stderr, "$ %llu (%.3f%%) hash collisions, %llu second level\n", out.p(txp_hash_collision),
                100.0 * (double) out.p(txp_hash_collision) / out.p(txp_hash_find),
                out.p(txp_hash_collision2));
    if (txp_count >= txp_total_transbuffer)
        fprintf(stderr, "$ %llu max buffer per txn, %llu total buffer\n",
                out.p(txp_max_transbuffer), out.p(txp_total_transbuffer));
    fprintf(stderr, "$ %llu next commit-tid\n", (unsigned long long) _TID);
}

const char* Transaction::state_name(int state) {
    static const char* names[] = {"in-progress", "committing", "committing-locked", "aborted", "committed"};
    if (unsigned(state) < arraysize(names))
        return names[state];
    else
        return "unknown-state";
}

void Transaction::print(std::ostream& w) const {
    w << "T0x" << (void*) this << " " << state_name(state_) << " [";
    for (auto& ti : transSet_) {
        if (&ti != &transSet_[0])
            w << " ";
        ti.owner()->print(w, ti);
    }
    w << "]\n";
}

void Transaction::print() const {
    print(std::cerr);
}

void TObject::print(std::ostream& w, const TransItem& item) const {
    w << "{" << typeid(*this).name() << " " << (void*) this << "." << item.key<void*>();
    if (item.has_read())
        w << " R" << item.read_value<void*>();
    if (item.has_write())
        w << " =" << item.write_value<void*>();
    if (item.has_predicate())
        w << " P" << item.predicate_value<void*>();
    w << "}";
}
