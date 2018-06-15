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
        a.insert(key1, 123);
        a.insert(key2, 321);
    }

    {
        TransactionGuard t;
        auto x = a.lookup(key1);
        auto y = a.lookup(key2);
        assert(x == 123);
        assert(y == 321);
    }

    {
        TransactionGuard t;
        a.erase(key1);
        auto x = a.lookup(key1);
        assert(x == 0);
    }

    {
        TransactionGuard t;
        auto x = a.lookup(key1);
        assert(x == 0);
        a.insert(key1, 567);
    }

    {
        TransactionGuard t;
        auto x = a.lookup(key1);
        assert(x == 567);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    testSimple();
    return 0;
}
