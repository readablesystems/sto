#undef NDEBUG
#include <string>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TBox.hh"
//XXX disabled string wrapper due to unknown compiler issue
//#include "StringWrapper.hh"

#define GUARDED if (TransactionGuard tguard{})

void testSimpleInt() {
	TBox<int, TMvWrapped<int>> f;

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

void testSimpleString() {
	TBox<std::string, TMvWrapped<std::string>> f;

	{
        TransactionGuard t;
        f = "100";
    }

	{
        TransactionGuard t2;
        std::string f_read = f;
        assert(f_read.compare("100") == 0);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testConcurrentInt() {
    TBox<int, TMvWrapped<int>> ib;
    TBox<int, TMvWrapped<int>> box;
    bool match;

    {
        TestTransaction t1(1);
        match = ib < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ib = 1;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(1);
        ib = 1;

        TestTransaction t2(2);
        ib = 2;
        assert(t2.try_commit());
        assert(t1.try_commit());

        assert(ib.nontrans_read() == 1);
    }

    ib.nontrans_write(2);

    {
        TestTransaction t1(1);
        match = ib == 2;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ib = 0;
        assert(t2.try_commit());

        TestTransaction t3(3);
        ib = 2;
        assert(t3.try_commit());
        assert(t1.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testOpacity1() {
    TBox<int, TMvWrapped<int>> f, g;
    TBox<int, TMvWrapped<int>> box;
    f.nontrans_write(3);

    {
        TestTransaction t1(1);
        int x = f;
        assert(x == 3);
        box = 9; /* avoid read-only txn */

        TestTransaction t(2);
        f = 2;
        g = 4;
        assert(t.try_commit());

        t1.use();
        x = g;
        assert(x == 0);
        assert(t1.try_commit());
    }

    {
        TransactionGuard t2;
        int v = f;
        assert(v == 2);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testMvReads() {
    TBox<int, TMvWrapped<int>> f, g;
    f.nontrans_write(1);
    g.nontrans_write(-1);

    // Read-only transactions should always be able to commit
    {
        TestTransaction t1(1);
        int x = f + g;
        assert(x == 0);

        TestTransaction t2(2);
        f = 2;
        g = 4;
        assert(t2.try_commit());

        t1.use();
        x = f + g;
        assert(x == 0);
        assert(t1.try_commit());
    }

    f.nontrans_write(1);
    g.nontrans_write(0);

    // Later reads invalidate earlier writes
    {
        TestTransaction t1(1);
        g = g + f;

        TestTransaction t2(2);
        int x = f + g;
        assert(x == 1);

        t1.use();
        assert(!t1.try_commit());

        t2.use();
        assert(t2.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}


int main() {
    testSimpleInt();
    testSimpleString();
    testConcurrentInt();
    testOpacity1();
    testMvReads();
    return 0;
}
