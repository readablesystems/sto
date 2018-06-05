#undef NDEBUG
#include <string>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TInt.hh"
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
        f.inc();
        int f_read = f;
        assert(f_read == 101);
        f = 10;
        assert(f == 10);

        f.inc();
        f = f + 1;
        assert(f == 12);
        f.inc();
        f.inc();
    }

    {
        TransactionGuard t2;
        int f_read = f;
        assert(f_read == 14);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    testSimpleInt();
    testIncrement();
    return 0;
}
