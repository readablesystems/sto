#undef NDEBUG
#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "SwissTGeneric.hh"

void testSimpleInt() {
    SwissTNonopaqueGeneric& f = *new SwissTNonopaqueGeneric;
    int f1 = 0;

    {
        TransactionGuard t;
        f.write_throws(&f1, 100);
    }

	{
        TransactionGuard t2;
        int f_read_throws;
        f.read_throws(&f1, f_read_throws);
        assert(f_read_throws == 100);
    }

    assert(f1 == 100);

	printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleInt2() {
    SwissTNonopaqueGeneric& g = *new SwissTNonopaqueGeneric;
    int f[100];

    {
        TransactionGuard t;
	g.write_throws(&f[1], 100);
	g.write_throws(&f[1], 200);
    }

    {
        TransactionGuard t2;
        int f_read_throws;
        g.read_throws(&f[1], f_read_throws);
        assert(f_read_throws == 200);
    }

        printf("PASS: %s\n", __FUNCTION__);
}

/*void testOpacity1() {
    int f[10];
    for (int i = 0; i < 10; i++)
        f[i] = i;
    TGeneric g;

    try {
        TestTransaction t1(1);
        int x;
        g.read_throws(&f[3], x);
        assert(x == 3);

        TestTransaction t(2);
        g.write_throws(&f[3], 2);
        g.write_throws(&f[4], 6);
        assert(t.try_commit());

        t1.use();
        x;
        g.read_throws(&f[4], x);
        assert(false && "shouldn't get here");
        assert(x == 6);
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
    }

    {
        TransactionGuard t2;
        int v;
        g.read_throws(&f[4], v);
        assert(v == 6);
        assert(f[4] == 6);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testNoOpacity1() {
    int f[10];
    for (int i = 0; i < 10; i++)
        f[i] = i;
    TNonopaqueGeneric g;

    {
        TestTransaction t1(1);
        int x;
        g.read_throws(&f[3], x);
        assert(x == 3);

        TestTransaction t(2);
        g.write_throws(&f[3], 2);
        g.write_throws(&f[4], 6);
        assert(t.try_commit());

        t1.use();
        x;
        g.read_throws(&f[4], x);
        assert(x == 6);
        assert(!t1.try_commit());
    }

    {
        TransactionGuard t2;
        int v;
        g.read_throws(&f[4], v);
        assert(v == 6);
        assert(f[4] == 6);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testVariableSizes() {
    struct {
        int a;
        short b;
        char c;
        char d;
    } foo = { 100, 1, 2, 3 };
    TGeneric g;

    {
        TestTransaction t1(1);
        int x;
        g.read_throws(&foo.a, x);
        assert(x == 100);
        x;
        g.read_throws(&foo.b, x);
        assert(x == 1);
        g.write_throws(&foo.b, 10);
        g.write_throws(&foo.a, 10000);
        g.write_throws(&foo.d, 99);
        assert(foo.a == 100);
        assert(foo.b == 1);
        assert(foo.d == 3);
        x;
        g.read_throws(&foo.a, x);
        assert(x == 10000);
        x;
        g.read_throws(&foo.b, x);
        assert(x == 10);
        x;
        g.read_throws(&foo.c, x);
        assert(x == 2);
        x;
        g.read_throws(&foo.d, x);
        assert(x == 99);
        assert(t1.try_commit());

        assert(foo.a == 10000);
        assert(foo.b == 10);
        assert(foo.c == 2);
        assert(foo.d == 99);
    }

    printf("PASS: %s\n", __FUNCTION__);
}*/

void testWriteWriteConflict() {
        SwissTNonopaqueGeneric& g = *new SwissTNonopaqueGeneric;
	int f[100];

        {
                TestTransaction t1(1);

                TestTransaction t2(2);

                try {
                        t1.use();
                        g.write_throws(&f[1], 100);

                        t2.use();
                        g.write_throws(&f[1], 100);
                } catch (Transaction::Abort e) {
                }

                assert(t2.get_tx().is_restarted());

                t1.use();
                assert(t1.try_commit());

                t2.use();
                Sto::start_transaction();
                int a;
        g.read_throws(&f[1], a);
                assert(a == 100);

                g.write_throws(&f[1], 200);
        g.read_throws(&f[1], a);
                assert(a == 200);

        }

        printf("PASS: %s\n", __FUNCTION__);
}


void testAbortReleaseLock() {
	SwissTNonopaqueGeneric& g = *new SwissTNonopaqueGeneric;
	int f[100];

        {
                TestTransaction t1(1);

                TestTransaction t2(2);

                try {
                        t1.use();
			g.write_throws(&f[1], 100);
	
                        t2.use();
                        g.write_throws(&f[2], 200);
                        g.write_throws(&f[1], 300);
                } catch(Transaction::Abort e) {

                }

                assert(t2.get_tx().is_restarted());

                t1.use();
                g.write_throws(&f[2], 400);
                g.write_throws(&f[1], 500);
                assert(t1.try_commit());

        }
        printf("PASS: %s\n", __FUNCTION__);
}



int main() {
    testSimpleInt();
    testSimpleInt2();
    //testOpacity1();
    //testNoOpacity1();
    //testVariableSizes();
    testWriteWriteConflict();
    testAbortReleaseLock();
    return 0;
}
