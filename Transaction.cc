#include "Transaction.hh"

#ifndef STO
threadinfo_t Transaction::tinfo[MAX_THREADS];
__thread int Transaction::threadid;
unsigned Transaction::global_epoch;
std::function<void(unsigned)> Transaction::epoch_advance_callback;
#endif
