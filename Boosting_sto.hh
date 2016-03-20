#pragma once

#if defined(BOOSTING) && defined(STO)

#include "Transaction.hh"
#include "TransUndoable.hh"
#include "TransPessimisticLocking.hh"

extern TransPessimisticLocking __pessimistLocking;

#define ADD_UNDO(callback, self, context1, context2) ({ assert(self == this); this->add_undo(callback, context1, context2); })
#define TRANS_READ_LOCK(lock) __pessimistLocking.transReadLock(lock)
#define TRANS_WRITE_LOCK(lock) __pessimistLocking.transWriteLock(lock)

#endif
