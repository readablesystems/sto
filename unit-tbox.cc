#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "TBox.hh"

void testSimpleInt() {
	TBox<int> f;

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
	TBox<std::string> f;

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
    TBox<int> ib;
    bool match;

    {
        TestTransaction t1(1);
        match = ib < 3;
        assert(match);

        TestTransaction t2(2);
        ib = 1;
        assert(t2.try_commit());
        assert(!t1.try_commit());
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

        TestTransaction t2(2);
        ib = 0;
        assert(t2.try_commit());

        TestTransaction t3(3);
        ib = 2;
        assert(t3.try_commit());
        assert(!t1.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}



void testOpacity1() {
    TBox<int> f, g;
    f.nontrans_write(3);

    try {
        TestTransaction t1(1);
        int x = f;
        assert(x == 3);

        TestTransaction t(2);
        f = 2;
        g = 4;
        assert(t.try_commit());

        t1.use();
        x = g;
        assert(false && "shouldn't get here");
        assert(x == 4);
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
    }

    {
        TransactionGuard t2;
        int v = f;
        assert(v == 2);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testNoOpacity1() {
    TBox<int, TNonopaqueWrapped<int> > f, g;
    f.nontrans_write(3);

    {
        TestTransaction t1(1);
        int x = f;
        assert(x == 3);

        TestTransaction t(2);
        f = 2;
        g = 4;
        assert(t.try_commit());

        t1.use();
        x = g;
        assert(x == 4);
        assert(!t1.try_commit());
    }

    {
        TransactionGuard t2;
        int v = f;
        assert(v == 2);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    testSimpleInt();
    testSimpleString();
    testConcurrentInt();
    testOpacity1();
    testNoOpacity1();
    return 0;
}
