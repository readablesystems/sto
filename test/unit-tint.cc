#undef NDEBUG
#include <string>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TInt.hh"
#include "Transaction.hh"
//XXX disabled string wrapper due to unknown compiler issue
//#include "StringWrapper.hh"

#define GUARDED if (TransactionGuard tguard{})

void testSimpleInt() {
    TInt f;

    {
        TransactionGuard t;
        f = 100;
    }

    {
        TransactionGuard t2;
        int f_read = f;
        assert(f_read == 100);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testIncrement() {
    TInt f;

    {
        TransactionGuard t;
        f = 100;
        f.inc(1);
        int f_read = f;
        assert(f_read == 101);
        f = 10;
        assert(f == 10);

        f.inc(1);
        f = f + 1;
        assert(f == 12);
        f.inc(1);
        f.inc(1);
    }

    {
        TransactionGuard t2;
        int f_read = f;
        assert(f_read == 14);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void test1() {
    TInt x;

    {
        TransactionGuard t;
        x = 0;
    }

    {
        TransactionGuard t;
        int tmp = x;
        x.inc(1);
        assert(tmp == 0);
    }

    {
        TransactionGuard t;
        assert(x == 1);
        x = 0;
    }

    {
        TransactionGuard t;
        x.inc(1);
        int tmp = x;
        assert(tmp == 1);
    }

    {
        TransactionGuard t;
        assert(x == 1);
        x = 0;
    }

    {
        TransactionGuard t;
        x.inc(1);
    }

    {
        TransactionGuard t;
        assert(x == 1);
        x = 0;
    }

    {
        TransactionGuard t;
        x = 5;
        x.inc(1);
    }

    {
        TransactionGuard t;
        assert(x == 6);
        x = 0;
    }

    {
        TransactionGuard t;
        x.inc(1);
        x.inc(1);
        x.inc(1);
    }

    {
        TransactionGuard t;
        assert(x == 3);
        x = 0;
    }

    {
        TransactionGuard t;
        x = 5;
        x.inc(1);
        int tmp = x;
        assert(tmp == 6);
    }

    {
        TransactionGuard t;
        assert(x == 6);
        x = 0;
    }

    {
        TransactionGuard t;
        x.inc(1);
        x = 5;
    }

    {
        TransactionGuard t;
        assert(x == 5);
        x = 0;
    }

    {
        TransactionGuard t;
        x.inc(1);
        x = 5;
        x.inc(1);
    }

    {
        TransactionGuard t;
        assert(x == 6);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    testSimpleInt();
    testIncrement();
    test1();
    return 0;
}
