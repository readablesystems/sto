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

    Transaction::epoch_advance_once();

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

    Transaction::epoch_advance_once();

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

    Transaction::epoch_advance_once();

    {
        TestTransaction t1(1);
        ib = 1;

        TestTransaction t2(2);
        ib = 2;
        assert(t2.try_commit());
        assert(!t1.try_commit());

        assert(ib.nontrans_read() == 2);
    }

    Transaction::epoch_advance_once();
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

    Transaction::epoch_advance_once();

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

    Transaction::epoch_advance_once();
    f.nontrans_write(1);
    g.nontrans_write(0);

    // Later reads also don't validate earlier writes
    {
        TestTransaction t1(1);
        g = g + f;

        TestTransaction t2(2);
        int x = f + g;
        assert(x == 1);

        t1.use();
        assert(t1.try_commit());

        t2.use();
        assert(t2.try_commit());
    }

    Transaction::epoch_advance_once();
    f.nontrans_write(1);
    g.nontrans_write(2);

    // Later reads of earlier transactions do not invalidate earlier writes of
    // later transactions
    {
        TestTransaction t1(1);
        int x = f + g;

        TestTransaction t2(2);
        f = f + 2 * g;

        t1.use();
        x = f + g;
        assert(x == 3);

        t2.use();
        assert(t2.try_commit());

        t1.use();
        assert(t1.try_commit());
    }

    Transaction::epoch_advance_once();
    f.nontrans_write(0);
    g.nontrans_write(1);

    // Read-my-writes support
    {
        TestTransaction t(1);
        f = f + 2 * g;
        g = f;
        f = f + g;
        int x = f;
        assert(x == 4);
        x = g;
        assert(x == 2);
        assert(t.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testMvWrites() {
    TBox<int, TMvWrapped<int>> f, g;
    f.nontrans_write(1);
    g.nontrans_write(-1);

    // Writes to different variables should not affect each other
    {
        TestTransaction t1(1);
        f = 0;

        TestTransaction t2(2);
        g = 0;

        t1.use();
        assert(t1.try_commit());

        t2.use();
        assert(t2.try_commit());
    }

    Transaction::epoch_advance_once();
    f.nontrans_write(1);
    g.nontrans_write(0);

    // Later reads in RW-transactions should invalidate concurrent writes
    {
        TestTransaction t1(1);
        f = 0;

        TestTransaction t2(2);
        g = f;

        t2.use();
        assert(t2.try_commit());

        t1.use();
        assert(!t1.try_commit());
    }

    Transaction::epoch_advance_once();
    f.nontrans_write(1);
    g.nontrans_write(0);

    // Earlier reads in RW-transactions should not invalidate concurrent writes
    {
        TestTransaction t1(1);
        f = g;

        TestTransaction t2(2);
        g = 1;

        t1.use();
        assert(t1.try_commit());

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
    testMvWrites();
    return 0;
}
