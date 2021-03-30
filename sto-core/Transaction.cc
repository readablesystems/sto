#include "Sto.hh"
#include <typeinfo>
#include <bitset>
#include <fstream>

#include <sys/resource.h>
#include <sys/time.h>

#include "MVCC.hh"

Transaction::testing_type Transaction::testing;
threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int TThread::the_id;
PercentGen TThread::gen[MAX_THREADS];

Transaction::epoch_state __attribute__((aligned(128))) Transaction::global_epochs = {
    {2}, {1}, {0}, TransactionTid::increment_value, true
};
__thread Transaction *TThread::txn = nullptr;
std::function<void(threadinfo_t::epoch_type)> Transaction::epoch_advance_callback;
std::atomic<TransactionTid::type> __attribute__((aligned(128)))
    Transaction::_TID(3 * TransactionTid::increment_value);
std::atomic<TransactionTid::type> __attribute__((aligned(128)))
    Transaction::_RTID(2 * TransactionTid::increment_value);
   // reserve TransactionTid::increment_value for prepopulated
unsigned Transaction::us_per_epoch = 1000;  // Defaults to 1ms

static void __attribute__((used)) check_static_assertions() {
    static_assert(sizeof(threadinfo_t) % 128 == 0, "threadinfo is 2-cache-line aligned");
}

void Transaction::initialize() {
    static_assert(tset_initial_capacity % tset_chunk == 0, "tset_initial_capacity not an even multiple of tset_chunk");
    hash_base_ = 32768;
    tset_size_ = 0;
    lrng_state_ = 12897;
#if CICADA_HASHTABLE == 0 && defined(TRANSACTION_HASHTABLE)
    bzero(hashtable_, sizeof(hashtable_));
#endif
    commit_tid_ = 0;
    prev_commit_tid_ = 0;
    for (unsigned i = 0; i != tset_initial_capacity / tset_chunk; ++i)
        tset_[i] = &tset0_[i * tset_chunk];
    for (unsigned i = tset_initial_capacity / tset_chunk; i != arraysize(tset_); ++i)
        tset_[i] = nullptr;
}

Transaction::~Transaction() {
    if (in_progress())
        silent_abort();
    TransItem* live = tset0_;
    for (unsigned i = 0; i != arraysize(tset_); ++i, live += tset_chunk)
        if (live != tset_[i])
            delete[] tset_[i];
}

void Transaction::refresh_tset_chunk() {
    assert(tset_size_ % tset_chunk == 0);
    assert(tset_size_ < tset_max_capacity);
    if (!tset_[tset_size_ / tset_chunk])
        tset_[tset_size_ / tset_chunk] = new TransItem[tset_chunk];
    tset_next_ = tset_[tset_size_ / tset_chunk];
}

void* Transaction::epoch_advancer(void*) {
    static int num_epoch_advancers = 0;
    if (fetch_and_add(&num_epoch_advancers, 1) != 0)
        std::cerr << "WARNING: more than one epoch_advancer thread\n";

    // don't bother epoch'ing til things have picked up
    usleep(us_per_epoch);
    while (global_epochs.run) {
        global_epoch_advance_once();
        usleep(us_per_epoch);
    }

    fetch_and_add(&num_epoch_advancers, -1);
    return NULL;
}

void Transaction::global_epoch_advance_once() {
    epoch_type ge = global_epochs.global_epoch.load();
    epoch_type re = global_epochs.global_epoch.load();
    epoch_type ae = global_epochs.read_epoch.load();
    int i = 0;
    for (auto& t : tinfo) {
        auto twepoch = t.write_snapshot_epoch.load();
        auto trepoch = t.epoch.load();
        if (twepoch != 0 && signed_epoch_type(twepoch - re) < 0) {
            re = twepoch;
        }
        if (trepoch != 0 && signed_epoch_type(trepoch - ae) < 0) {
            ae = trepoch;
        }
        i++;
    }
    global_epochs.global_epoch = std::max(ge + 1, epoch_type(1));
    global_epochs.read_epoch = re;
    global_epochs.active_epoch = ae;
    global_epochs.recent_tid = _TID.load(std::memory_order_relaxed);

    if (epoch_advance_callback)
        epoch_advance_callback(global_epochs.global_epoch);
}

void Transaction::epoch_advance_once() {
    tid_type min_wtid = _TID.load(std::memory_order_relaxed);
    for (auto& t : tinfo) {
        fence();
        tid_type wtid = t.wtid;
        if (wtid != 0 && wtid < min_wtid)
            min_wtid = wtid;
    }
    fence();
    if (min_wtid > 0) {
        tid_type next = min_wtid - TransactionTid::increment_value;
        tid_type rtid;
        do {
            rtid = _RTID;
        } while (rtid < next && !_RTID.compare_exchange_weak(rtid, next));
    }
}

bool Transaction::preceding_duplicate_read(TransItem* needle) const {
    const TransItem* it = nullptr;
    for (unsigned tidx = 0; ; ++tidx) {
        it = (tidx % tset_chunk ? it + 1 : tset_[tidx / tset_chunk]);
        if (it == needle)
            return false;
        if (it->owner() == needle->owner() && it->key_ == needle->key_
            && it->has_read())
            return true;
    }
}

bool Transaction::hard_check_opacity(TransItem* item, TransactionTid::type t) {
    // ignore opacity checks during commit; we're in the middle of checking
    // things anyway
    if (state_ == s_committing || state_ == s_committing_locked)
        return true;

    // ignore if version hasn't changed
    if (item && item->has_read() && item->read_value<TransactionTid::type>() == t)
        return true;

    // die on recursive opacity check; this is only possible for predicates
    if (unlikely(state_ == s_opacity_check)) {
        mark_abort_because(item, "recursive opacity check", t);
    abort:
        TXP_INCREMENT(txp_hco_abort);

        state_ = s_in_progress;
        return false;
    }
    assert(state_ == s_in_progress);

    TXP_INCREMENT(txp_hco);
    if (TransactionTid::is_locked_elsewhere(t, threadid_)) {
        TXP_INCREMENT(txp_hco_lock);
        mark_abort_because(item, "locked", t);
        goto abort;
    }
    if (t & TransactionTid::nonopaque_bit)
        TXP_INCREMENT(txp_hco_invalid);

    state_ = s_opacity_check;
    start_tid_ = _TID.load(std::memory_order_relaxed);
    release_fence();
    TransItem* it = nullptr;
    for (unsigned tidx = 0; tidx != tset_size_; ++tidx) {
        it = (tidx % tset_chunk ? it + 1 : tset_[tidx / tset_chunk]);
        if (it->has_read()) {
            TXP_INCREMENT(txp_total_check_read);
            if (!it->owner()->check(*it, *this)
                && (!may_duplicate_items_ || !preceding_duplicate_read(it))) {
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
    state_ = s_in_progress;
    return true;
}

void Transaction::callCMstart() {
#if CONTENTION_REGULATION
    ContentionManager::start(this);
#endif
}

void Transaction::stop(bool committed, unsigned* writeset, unsigned nwriteset) {
#if STO_TSC_PROFILE
    TimeKeeper<tc_cleanup> tk;
#endif
    if (!committed) {
        TXP_INCREMENT(txp_total_aborts);
#if STO_DEBUG_ABORTS
        if (local_random() <= uint32_t(0xFFFFFFFF * STO_DEBUG_ABORTS_FRACTION)) {
            std::ostringstream buf;
            buf << "$" << (threadid_ < 10 ? "0" : "") << threadid_;
            //    << " abort " << state_name(state_);
            if (abort_reason_)
                buf << " " << abort_reason_;
            buf << " tset_size_ = [" << tset_size_ << "]";
            if (abort_item_)
                buf << " " << *abort_item_;
            if (abort_version_)
                buf << " V" << TVersion(abort_version_);
            buf << '\n';
            std::cerr << buf.str();
        }
#endif
    }

    TXP_ACCOUNT(txp_max_transbuffer, buf_.buffer_size());
    TXP_ACCOUNT(txp_total_transbuffer, buf_.buffer_size());

    TransItem* it;
    if (!any_writes_)
        goto unlock_all;

    if (committed && !STO_SORT_WRITESET) {
/*
        for (unsigned* idxit = writeset + nwriteset; idxit != writeset; ) {
            --idxit;
            if (*idxit < tset_initial_capacity)
                it = &tset0_[*idxit];
            else
                it = &tset_[*idxit / tset_chunk][*idxit % tset_chunk];
            if (it->needs_unlock())
                it->owner()->unlock(*it);
        }
*/
        for (unsigned* idxit = writeset + nwriteset; idxit != writeset; ) {
            --idxit;
            if (*idxit < tset_initial_capacity)
                it = &tset0_[*idxit];
            else
                it = &tset_[*idxit / tset_chunk][*idxit % tset_chunk];
            if (it->has_write()) // always true unless a user turns it off in install()/check()
                it->owner()->cleanup(*it, committed);
        }
    } else {
/*
        if (state_ == s_committing_locked) {
            it = &tset_[tset_size_ / tset_chunk][tset_size_ % tset_chunk];
            for (unsigned tidx = tset_size_; tidx != first_write_; --tidx) {
                //outfile << "P6" << std::endl;
                it = (tidx % tset_chunk ? it - 1 : &tset_[(tidx - 1) / tset_chunk][tset_chunk - 1]);
                if (it->needs_unlock()) {
                    //outfile << "P7" << std::endl;
                    //outfile << "Unlocking item[" << it->key<unsigned>() << "]" << std::endl;
                    it->owner()->unlock(*it);
                }
            }
        }
*/
        it = &tset_[tset_size_ / tset_chunk][tset_size_ % tset_chunk];
        for (unsigned tidx = tset_size_; tidx != first_write_; --tidx) {
            it = (tidx % tset_chunk ? it - 1 : &tset_[(tidx - 1) / tset_chunk][tset_chunk - 1]);
            if (it->has_write())
                it->owner()->cleanup(*it, committed);
        }
    }

unlock_all:
    it = &tset_[tset_size_ / tset_chunk][tset_size_ % tset_chunk];
    for (unsigned tidx = tset_size_; tidx != 0; --tidx) {
        it = (tidx % tset_chunk ? it - 1 : &tset_[(tidx - 1) / tset_chunk][tset_chunk - 1]);
        if (it->needs_unlock())
            it->owner()->unlock(*it);
    }

    // TODO: this will probably mess up with nested transactions
    threadinfo_t& thr = tinfo[TThread::id()];
    if (thr.trans_end_callback)
        thr.trans_end_callback();
    thr.wtid.store(0, std::memory_order_release);
    // XXX should reset trans_end_callback after calling it...
    state_ = s_aborted + committed;
    restarted = true;

#if CONTENTION_REGULATION
    if (!committed) {
       ContentionManager::on_rollback(TThread::id());
    }
#endif

    // clear/consolidate transactional scratch space
    scratch_.clear();

#if STO_TSC_PROFILE
    auto endtime = read_tsc();
    if (!committed)
        TSC_ACCOUNT(tc_abort, endtime - start_tsc_);
#endif

    //COZ_PROGRESS;
}

bool Transaction::try_commit() {
#if STO_TSC_PROFILE
    TimeKeeper<tc_commit> tk;
#endif
    assert(TThread::id() == threadid_);
#if ASSERT_TX_SIZE
    if (tset_size_ > TX_SIZE_LIMIT) {
        std::cerr << "transSet_ size at " << tset_size_
            << ", abort." << std::endl;
        assert(false);
    }
#endif
    TXP_ACCOUNT(txp_max_set, tset_size_);
    TXP_ACCOUNT(txp_total_n, tset_size_);

    assert(state_ == s_in_progress || state_ >= s_aborted);
    if (state_ >= s_aborted) {
        return state_ > s_aborted;
    }

    if (any_nonopaque_)
        TXP_INCREMENT(txp_commit_time_nonopaque);
#if !CONSISTENCY_CHECK
    // commit immediately if read-only transaction with opacity
    if (!any_writes_ && !any_nonopaque_) {
        stop(true, nullptr, 0);
        return true;
    }
#endif

    state_ = s_committing;

    unsigned writeset[tset_size_];
    unsigned nwriteset = 0;
    writeset[0] = tset_size_;

    TransItem* it = nullptr;
    for (unsigned tidx = 0; tidx != tset_size_; ++tidx) {
        it = (tidx % tset_chunk ? it + 1 : tset_[tidx / tset_chunk]);
        if (it->has_write()) {
            writeset[nwriteset++] = tidx;
#if !STO_SORT_WRITESET
            if (nwriteset == 1) {
                first_write_ = writeset[0];
                state_ = s_committing_locked;
            }
            if (!it->needs_unlock() && !it->owner()->lock(*it, *this)) {
                mark_abort_because(it, "commit lock");
                goto abort;
            }
            it->__or_flags(TransItem::lock_bit);
            it->__or_flags(TransItem::cl_bit);
#endif
        }
        if (it->has_read()) {
            TXP_INCREMENT(txp_total_r);
            if (it->cc_mode() == CCMode::opt) {
                TXP_INCREMENT(txp_total_adaptive_opt);
            }
            // tracking TicToc commit ts (for reads) here
            if (it->cc_mode() == CCMode::tictoc) {
                if (it->is_tictoc_compressed())
                    it->tictoc_extract_read_ts<TicTocCompressedVersion<>>().compute_commit_ts_step(this->tictoc_tid_, false/* !write */);
                else
                    it->tictoc_extract_read_ts<TicTocVersion<>>().compute_commit_ts_step(this->tictoc_tid_, false /* ! write */);
            }
        } else if (it->has_predicate()) {
            TXP_INCREMENT(txp_total_check_predicate);
            if (!it->owner()->check_predicate(*it, *this, true)) {
                mark_abort_because(it, "commit check_predicate");
                goto abort;
            }
        }
    }

    first_write_ = writeset[0];

    //phase1
#if STO_SORT_WRITESET
    std::sort(writeset, writeset + nwriteset, [&] (unsigned i, unsigned j) {
        TransItem* ti = &tset_[i / tset_chunk][i % tset_chunk];
        TransItem* tj = &tset_[j / tset_chunk][j % tset_chunk];
        return *ti < *tj;
    });

    if (nwriteset) {
        state_ = s_committing_locked;
        auto writeset_end = writeset + nwriteset;
        for (auto it = writeset; it != writeset_end; ) {
            TransItem* me = &tset_[*it / tset_chunk][*it % tset_chunk];
            if (!me->owner()->lock(*me, *this)) {
                mark_abort_because(me, "commit lock");
                goto abort;
            }
            me->__or_flags(TransItem::lock_bit);
            ++it;
        }
    }
#endif

#if CONSISTENCY_CHECK
    fence();
    commit_tid();
    fence();
#endif

    //phase2
    for (unsigned tidx = 0; tidx != tset_size_; ++tidx) {
        it = (tidx % tset_chunk ? it + 1 : tset_[tidx / tset_chunk]);
        if (it->has_read() && (it->locked_at_commit() || !it->needs_unlock())) {
            TXP_INCREMENT(txp_total_check_read);
            if (!it->owner()->check(*it, *this)
                && (!may_duplicate_items_ || !preceding_duplicate_read(it))) {
                mark_abort_because(it, "commit check");
                goto abort;
            }
        }
    }

    // fence();

    //phase3
#if STO_SORT_WRITESET
    for (unsigned tidx = first_write_; tidx != tset_size_; ++tidx) {
        it = &tset_[tidx / tset_chunk][tidx % tset_chunk];
        if (it->has_write()) {
            TXP_INCREMENT(txp_total_w);
            it->owner()->install(*it, *this);
        }
    }
#else
    if (nwriteset) {
        auto writeset_end = writeset + nwriteset;
        for (auto idxit = writeset; idxit != writeset_end; ++idxit) {
            if (likely(*idxit < tset_initial_capacity))
                it = &tset0_[*idxit];
            else
                it = &tset_[*idxit / tset_chunk][*idxit % tset_chunk];
            TXP_INCREMENT(txp_total_w);
            it->owner()->install(*it, *this);
        }
    }
#endif

    // fence();
    stop(true, writeset, nwriteset);

    //COZ_PROGRESS;
    return true;

abort:
    //outfile.close();
    // fence();
    TXP_INCREMENT(txp_commit_time_aborts);
    // scan the whole read set for locks if aborting
    // XXX this can be optimized later
    stop(false, nullptr, 0);
#if STO_TSC_PROFILE
    auto endtime = read_tsc();
    TSC_ACCOUNT(tc_commit_wasted, endtime - tk.init_tsc_val());
#endif
    return false;
}

void Transaction::print_stats() {
    txp_counters out = txp_counters_combined();
    if (txp_count >= txp_max_set) {
        unsigned long long txc_total_starts = out.p(txp_total_starts);
        unsigned long long txc_total_aborts = out.p(txp_total_aborts);
        unsigned long long txc_commit_aborts = out.p(txp_commit_time_aborts);
        unsigned long long txc_total_commits = txc_total_starts - txc_total_aborts;
        fprintf(stderr, "$ %llu allocs, %llu bytes\n", out.p(txp_alloc_t), out.p(txp_alloc_b));
        fprintf(stderr, "$ %llu starts, %llu max read set, %llu commits",
                txc_total_starts, out.p(txp_max_set), txc_total_commits);
        if (txc_total_aborts) {
            fprintf(stderr, ", %llu (%.3f%%) aborts",
                    out.p(txp_total_aborts),
                    100.0 * (double) out.p(txp_total_aborts) / out.p(txp_total_starts));
            if (out.p(txp_commit_time_aborts)) {
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts at commit time",
                        out.p(txp_commit_time_aborts),
                        100.0 * (double) out.p(txp_commit_time_aborts) / out.p(txp_total_aborts));
            }

            if (out.p(txp_lock_aborts)) {
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts due to lock time-outs",
                        out.p(txp_lock_aborts),
                        100.0 * (double) out.p(txp_lock_aborts) / out.p(txp_total_aborts));
            }

            if (out.p(txp_observe_lock_aborts)) {
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts due to observing write-locked versions",
                        out.p(txp_observe_lock_aborts),
                        100.0 * (double) out.p(txp_observe_lock_aborts) / out.p(txp_total_aborts));
            }

            if (out.p(txp_mvcc_lock_status_aborts)) {
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts due to MVCC lock failure at status check",
                        out.p(txp_mvcc_lock_status_aborts),
                        100.0 * (double) out.p(txp_mvcc_lock_status_aborts) / out.p(txp_total_aborts));
            }

            if (out.p(txp_mvcc_lock_vis_aborts)) {
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts due to MVCC lock failure at visibility check",
                        out.p(txp_mvcc_lock_vis_aborts),
                        100.0 * (double) out.p(txp_mvcc_lock_vis_aborts) / out.p(txp_total_aborts));
            }

            if (out.p(txp_mvcc_lock_vc_aborts)) {
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts due to MVCC lock failure at version consistency check",
                        out.p(txp_mvcc_lock_vc_aborts),
                        100.0 * (double) out.p(txp_mvcc_lock_vc_aborts) / out.p(txp_total_aborts));
            }

            if (out.p(txp_mvcc_check_aborts)) {
                fprintf(stderr, "\n$ %llu (%.3f%%) of aborts due to MVCC check failure",
                        out.p(txp_mvcc_check_aborts),
                        100.0 * (double) out.p(txp_mvcc_check_aborts) / out.p(txp_total_aborts));
            }
        }
        unsigned long long txc_commit_attempts = txc_total_starts - (txc_total_aborts - txc_commit_aborts);
        fprintf(stderr, "\n$ %llu commit attempts, %llu (%.3f%%) nonopaque\n",
                txc_commit_attempts, out.p(txp_commit_time_nonopaque),
                100.0 * (double) out.p(txp_commit_time_nonopaque) / txc_commit_attempts);
    }
    if (txp_count >= txp_hco_abort)
        fprintf(stderr, "$ %llu HCO (%llu lock, %llu invalid, %llu aborts) out of %llu check attempts (%.3f%%)\n",
                out.p(txp_hco), out.p(txp_hco_lock), out.p(txp_hco_invalid), out.p(txp_hco_abort), out.p(txp_tco),
                100.0 * (double) out.p(txp_hco) / out.p(txp_tco));
    if (txp_count >= txp_hash_collision)
        fprintf(stderr, "$ %llu (%.3f%%) hash collisions, %llu second level\n", out.p(txp_hash_collision),
                100.0 * (double) out.p(txp_hash_collision) / out.p(txp_hash_find),
                out.p(txp_hash_collision2));
    if (txp_count >= txp_total_transbuffer)
        fprintf(stderr, "$ %llu max buffer per txn, %llu total buffer\n",
                out.p(txp_max_transbuffer), out.p(txp_total_transbuffer));
    if (txp_count >= txp_mvcc_flat_spins) {
        fprintf(stderr, "$ MVCC flattening profiles:\n");
        fprintf(stderr, "$       Enflatten runs: %llu\n", out.p(txp_mvcc_flat_runs));
        fprintf(stderr, "$   Flattened versions: %llu\n", out.p(txp_mvcc_flat_versions));
        fprintf(stderr, "$     Avg versions/run: %.3f\n", 1.0 * out.p(txp_mvcc_flat_versions) / out.p(txp_mvcc_flat_runs));
        fprintf(stderr, "$      Committing runs: %llu\n", out.p(txp_mvcc_flat_commits));
        fprintf(stderr, "$        Spinning runs: %llu\n", out.p(txp_mvcc_flat_spins));
        fprintf(stderr, "$     Avg spins/commit: %.3f\n", 1.0 * out.p(txp_mvcc_flat_spins) / out.p(txp_mvcc_flat_commits));
    }
    if (txp_count >= txp_tpcc_st_aborts) {
        fprintf(stderr, "$ TPCC txn profiles: commits(aborts), abort rate\n");
        fprintf(stderr, "$     New-Order: %llu(%llu), %.3f%%\n", out.p(txp_tpcc_no_commits), out.p(txp_tpcc_no_aborts),
                100.0 * (double) out.p(txp_tpcc_no_aborts) / (out.p(txp_tpcc_no_commits) + out.p(txp_tpcc_no_aborts)));
        fprintf(stderr, "$       Payment: %llu(%llu), %.3f%%\n", out.p(txp_tpcc_pm_commits), out.p(txp_tpcc_pm_aborts),
                100.0 * (double) out.p(txp_tpcc_pm_aborts) / (out.p(txp_tpcc_pm_commits) + out.p(txp_tpcc_pm_aborts)));
        fprintf(stderr, "$  Order-Status: %llu(%llu), %.3f%%\n", out.p(txp_tpcc_os_commits), out.p(txp_tpcc_os_aborts),
                100.0 * (double) out.p(txp_tpcc_os_aborts) / (out.p(txp_tpcc_os_commits) + out.p(txp_tpcc_os_aborts)));
        fprintf(stderr, "$      Delivery: %llu(%llu), %.3f%%\n", out.p(txp_tpcc_dl_commits), out.p(txp_tpcc_dl_aborts),
                100.0 * (double) out.p(txp_tpcc_dl_aborts) / (out.p(txp_tpcc_dl_commits) + out.p(txp_tpcc_dl_aborts)));
        fprintf(stderr, "$   Stock-Level: %llu(%llu), %.3f%%\n", out.p(txp_tpcc_st_commits), out.p(txp_tpcc_st_aborts),
                100.0 * (double) out.p(txp_tpcc_st_aborts) / (out.p(txp_tpcc_st_commits) + out.p(txp_tpcc_st_aborts)));
//        fprintf(stderr, "$ NewOrder (stage 1): %llu\n", out.p(txp_tpcc_no_stage1));
//        fprintf(stderr, "$ NewOrder (stage 2): %llu\n", out.p(txp_tpcc_no_stage2));
//        fprintf(stderr, "$ NewOrder (stage 3): %llu\n", out.p(txp_tpcc_no_stage3));
//        fprintf(stderr, "$ NewOrder (stage 4): %llu\n", out.p(txp_tpcc_no_stage4));
//        fprintf(stderr, "$ NewOrder (stage 5): %llu\n", out.p(txp_tpcc_no_stage5));
//        fprintf(stderr, "$ Payment  (stage 1): %llu\n", out.p(txp_tpcc_pm_stage1));
//        fprintf(stderr, "$ Payment  (stage 2): %llu\n", out.p(txp_tpcc_pm_stage2));
//        fprintf(stderr, "$ Payment  (stage 3): %llu\n", out.p(txp_tpcc_pm_stage3));
//        fprintf(stderr, "$ Payment  (stage 4): %llu\n", out.p(txp_tpcc_pm_stage4));
//        fprintf(stderr, "$ Payment  (stage 5): %llu\n", out.p(txp_tpcc_pm_stage5));
//        fprintf(stderr, "$ Payment  (stage 6): %llu\n", out.p(txp_tpcc_pm_stage6));
//        fprintf(stderr, "$ Delivery (stage 1): %llu\n", out.p(txp_tpcc_dl_stage1));
//        fprintf(stderr, "$ Delivery (stage 2): %llu\n", out.p(txp_tpcc_dl_stage2));
//        fprintf(stderr, "$ Delivery (stage 3): %llu\n", out.p(txp_tpcc_dl_stage3));
//        fprintf(stderr, "$ Delivery (stage 4): %llu\n", out.p(txp_tpcc_dl_stage4));
//        fprintf(stderr, "$ Delivery (stage 5): %llu\n", out.p(txp_tpcc_dl_stage5));
        fprintf(stderr, "$       Lock Abort 1: %llu\n", out.p(txp_tpcc_lock_abort1));
        fprintf(stderr, "$       Lock Abort 2: %llu\n", out.p(txp_tpcc_lock_abort2));
        fprintf(stderr, "$       Lock Abort 3: %llu\n", out.p(txp_tpcc_lock_abort3));
        fprintf(stderr, "$      Check Abort 1: %llu\n", out.p(txp_tpcc_check_abort1));
        fprintf(stderr, "$      Check Abort 2: %llu\n", out.p(txp_tpcc_check_abort2));
    }

#if STO_TSC_PROFILE
    tc_counters out_tcs = tc_counters_combined();
    std::stringstream ss;
    ss << std::endl << "$ Timing breakdown: " << std::endl;
    ss << "   time_commit: " << out_tcs.to_realtime(tc_commit) << std::endl;
    ss << "   time_commit_wasted: " << out_tcs.to_realtime(tc_commit_wasted) << std::endl;
    ss << "   time_find_item: " << out_tcs.to_realtime(tc_find_item) << std::endl;
    ss << "   time_abort: " << out_tcs.to_realtime(tc_abort) << std::endl;
    ss << "   time_cleanup: " << out_tcs.to_realtime(tc_cleanup) << std::endl;
    ss << "   time_opacity: " << out_tcs.to_realtime(tc_opacity) << std::endl;
    ss << "   time_elapsed: " << out_tcs.to_realtime(tc_elapsed) << std::endl;

    fprintf(stderr, "%s\n", ss.str().c_str());
#endif

    fprintf(stderr, "$ %llu next commit-tid\n", (unsigned long long) _TID.load(std::memory_order_relaxed));
}

const char* Transaction::state_name(int state) {
    static const char* names[] = {"in-progress", "opacity-check", "committing", "committing-locked", "aborted", "committed"};
    if (unsigned(state) < arraysize(names))
        return names[state];
    else
        return "unknown-state";
}

void Transaction::print(std::ostream& w) const {
    w << "T0x" << (void*) this << " " << state_name(state_) << " [";
    const TransItem* it = nullptr;
    for (unsigned tidx = 0; tidx != tset_size_; ++tidx) {
        it = (tidx % tset_chunk ? it + 1 : tset_[tidx / tset_chunk]);
        if (tidx)
            w << " ";
        it->owner()->print(w, *it);
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

std::ostream& operator<<(std::ostream& w, const Transaction& txn) {
    txn.print(w);
    return w;
}

std::ostream& operator<<(std::ostream& w, const TestTransaction& txn) {
    txn.print(w);
    return w;
}

std::ostream& operator<<(std::ostream& w, const TransactionGuard& txn) {
    txn.print(w);
    return w;
}

void Transaction::rcu_release_all(std::thread& epoch_advancer, int num_work_threads) {
    Transaction::global_epochs.run = false;
    if (epoch_advancer.joinable()) {
        epoch_advancer.join();
    }

    auto wse = global_epochs.global_epoch.load(std::memory_order_relaxed);
    for (int i = 0; i < num_work_threads; ++i) {
        wse = std::max(wse, Transaction::tinfo[i].write_snapshot_epoch.load(std::memory_order_relaxed));
    }
    assert(wse > global_epochs.active_epoch.load());

    bool more = true;
    while (more) {
        more = false;
        auto ae = global_epochs.active_epoch.load();
        // XXX this would be safe to do in parallel too
        for (int i = 0; i < num_work_threads; ++i) {
            Transaction::tinfo[i].write_snapshot_epoch = wse;
            Transaction::tinfo[i].rcu_set.clean_until(ae);
            more = more || !Transaction::tinfo[i].rcu_set.empty();
        }
        global_epochs.active_epoch.store(ae + 1);
        ++wse;
    }
}
