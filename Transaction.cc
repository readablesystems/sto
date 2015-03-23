#include "Transaction.hh"

threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int Transaction::threadid;
unsigned Transaction::global_epoch;
__thread Transaction *Transaction::__transaction;
std::function<void(unsigned)> Transaction::epoch_advance_callback;
TransactionTid::type Transaction::_TID = TransactionTid::valid_bit;

static void __attribute__((used)) check_static_assertions() {
    static_assert(sizeof(threadinfo_t) % 128 == 0, "threadinfo is 2-cache-line aligned");
}

void Transaction::hard_check_opacity(TransactionTid::type t) {
    TransactionTid::type newstart = start_tid_;
    release_fence();
    if (!(t & TransactionTid::lock_bit) && check_reads(transSet_.begin(), transSet_.end()))
        start_tid_ = newstart;
    else
        abort();
}

void Transaction::print_stats() {
    threadinfo_t out = tinfo_combined();
    if (txp_count >= 4) {
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
