#include "Transaction.hh"

#define TM_BEGIN() while (1) { Transaction __transaction; try {
#define TM_END() __transaction.commit(); } catch (Transaction::Abort E) { continue; } break; }
#define TM_ARG __transaction,
#define TM_ARG_ALONE __transaction
