#undef NDEBUG
#include <string>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TART.hh"
#include "Transaction.hh"

#define GUARDED if (TransactionGuard tguard{})

void testSimple() {
    TART a;

    {
        TransactionGuard t;
        uint8_t key[4] = {0, 0, 0, 0};
        a.transPut(key, 123);
    }

    {
        TransactionGuard t2;
        uint8_t key[4] = {0, 0, 0, 0};
        auto x = a.transGet(key);
        assert(x.second == 123);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    testSimple();
    return 0;
}
