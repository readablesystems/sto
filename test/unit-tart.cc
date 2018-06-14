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

    std::string key1 = "hello world";
    std::string key2 = "1234";
    {
        TransactionGuard t;
        a.transPut(key1, 123);
        a.transPut(key2, 321);
    }

    {
        TransactionGuard t;
        auto x = a.transGet(key1);
        auto y = a.transGet(key2);
        assert(x.second == 123);
        assert(y.second == 321);
    }

    {
        TransactionGuard t;
        a.erase(key1);
        auto x = a.transGet(key1);
        assert(x.second == 0);
    }

    {
        TransactionGuard t;
        auto x = a.transGet(key1);
        assert(x.second == 0);
        a.transPut(key1, 567);
    }

    {
        TransactionGuard t;
        auto x = a.transGet(key1);
        assert(x.second == 567);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    testSimple();
    return 0;
}
