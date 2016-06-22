#undef NDEBUG
#include <iostream>
#include <assert.h>
#include "Transaction.hh"
#include "FCQueue.hh"
#define GUARDED if (TransactionGuard tguard{})

void testSimpleInt() {
    FCQueue<int> f;
    int ret1, ret2;

    {
        TransactionGuard t;
        f.push(100);
        f.push(200);
    }

    {
        TransactionGuard t2;
        bool f_read = f.pop(ret1);
        assert(f_read);
        f_read = f.pop(ret2);
        assert(f_read);
        f_read = f.pop(ret2);
        assert (ret2 == 200 && ret1 == 100);
        assert(!f_read);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testPushNPop() {
    FCQueue<int> f;
    int ret;

    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push(i);
        }
    } RETRY(false);

    {
        TransactionGuard t;
        f.push(20);
        f.push(21);
        assert(f.pop(ret));
        //t.print(std::cerr);
        f.pop(ret);
        f.pop(ret);
    }

    {
        TestTransaction t1(1);
        f.pop(ret);
        f.push(15);

        TestTransaction t2(2);
        f.push(20);
        assert(t2.try_commit());
        assert(t1.try_commit());

        TRANSACTION {
        } RETRY(false);
    }

    {
        TestTransaction t1(1);
        f.pop(ret);
        f.push(18);

        TestTransaction t2(2);
        f.push(21);
        assert(t2.try_commit());
        assert(t1.try_commit());

        TRANSACTION {
            f.pop(ret);
        } RETRY(false);
    }

    printf("PASS: testPushNPop\n");

    TestTransaction t3(1);
    f.pop(ret);
    f.push(15);

    TestTransaction t4(2);
    f.pop(ret);
    assert(t4.try_commit());
    assert(t3.try_commit());

    TRANSACTION {
    } RETRY(false);

    printf("PASS: testPushNPop1\n");

    TestTransaction t5(1);
    f.pop(ret);
    f.pop(ret);
    f.push(15);

    TestTransaction t6(2);
    assert(t6.try_commit());

    TestTransaction t7(3);

    assert(t5.try_commit());
    assert(t7.try_commit());

    printf("PASS: testPushNPop2\n");
}

void testMulPushPops() {
    FCQueue<int> f;
    int ret;

    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push(i);
        }
    } RETRY(false);

    {
        TransactionGuard t1;
        f.push(100);
        f.push(200);
        f.pop(ret);
        f.pop(ret);
        f.pop(ret);
    }
}

int main() {
    testSimpleInt();
    testPushNPop();
    testMulPushPops();
    return 0;
}
