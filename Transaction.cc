#include "Transaction.hh"

threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int Transaction::threadid;
unsigned Transaction::global_epoch;
__thread Transaction *Transaction::__transaction;
std::function<void(unsigned)> Transaction::epoch_advance_callback;

static void __attribute__((used)) check_static_assertions() {
    static_assert(sizeof(threadinfo_t) % 128 == 0, "threadinfo is 2-cache-line aligned");
}
