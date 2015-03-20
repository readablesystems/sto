#pragma once

#include <stdint.h>

class Transaction;
typedef uint32_t TransactionTid;
class TransItem;

class Shared {
public:
    virtual ~Shared() {}

    virtual bool check(const TransItem& item, const Transaction& t) = 0;
    virtual void lock(TransItem& item) = 0;
    virtual void unlock(TransItem& item) = 0;
    virtual void install(TransItem& item, TransactionTid tid) = 0;

    // probably just needs to call destructor
    virtual void cleanup(TransItem& item, bool committed) {
        (void) item, (void) committed;
    }
};
