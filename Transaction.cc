#include "Transaction.hh"

threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int Transaction::threadid;
unsigned __attribute__((aligned(64))) Transaction::global_epoch;
bool Transaction::run_epochs = true;
__thread Transaction *Transaction::__transaction;
std::function<void(unsigned)> Transaction::epoch_advance_callback;
TransactionTid::type __attribute__((aligned(128))) Transaction::_TID = TransactionTid::valid_bit;

static void __attribute__((used)) check_static_assertions() {
    static_assert(sizeof(threadinfo_t) % 128 == 0, "threadinfo is 2-cache-line aligned");
}

void Transaction::update_hash() {
    if (nhashed_ == 0)
        memset(hashtable_, 0, sizeof(hashtable_));
    for (auto it = transSet_.begin() + nhashed_; it != transSet_.end(); ++it, ++nhashed_) {
        int h = hash(it->sharedObj(), it->key_);
        if (!hashtable_[h])
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
