#include "Transaction.hh"

#define TM_BEGIN() TRANSACTION {
#define TM_END() } RETRY(true)
#define TM_ARG /* Nothing */
#define TM_ARG_ALONE /* Nothing */
