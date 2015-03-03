#include "Transaction.hh"

#if PERF_LOGGING
uint64_t Transaction::total_n, Transaction::total_r, Transaction::total_w;
uint64_t Transaction::total_searched;
uint64_t Transaction::total_aborts;
uint64_t Transaction::total_starts;
uint64_t Transaction::commit_time_aborts;
#endif

threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int Transaction::threadid;
unsigned Transaction::global_epoch;
std::function<void(unsigned)> Transaction::epoch_advance_callback;

static void __attribute__((used)) check_static_assertions() {
    static_assert(sizeof(threadinfo_t) % 128 == 0, "threadinfo is 2-cache-line aligned");
}
