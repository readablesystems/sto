#undef NDEBUG
#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "TVector_nopred.hh"
#include "TBox.hh"
#define GUARDED if (TransactionGuard tguard{})

void testSimpleInt() {
    TVector_nopred<int> f;

    {
        TransactionGuard t;
        f.push_back(100);
        f.push_back(200);
        *(f.begin()) = *(f.begin() + 1);
    }

    {
        TransactionGuard t2;
        int f_read = f[0];
        assert(f_read == 200);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testWriteNPushBack() {
    TVector_nopred<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    TestTransaction t2(1);
    f[0] = 4;

    TestTransaction t3(2);
    f.push_back(20); // This will resize the array
    //t3.print(std::cerr);
    assert(t3.try_commit());
    //t2.print(std::cerr);
    assert(t2.try_commit()); // This should not conflict with the write

    {
        TransactionGuard t4;
        assert(f[0] == 4); // Make sure that the value is actually updated
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testPushBack() {
    TVector_nopred<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    TestTransaction t2(1);
    f.push_back(4);

    TestTransaction t3(2);
    f.push_back(20);
    assert(t3.try_commit());
    assert(!t2.try_commit()); // This conflicts with the push_back (nopred)

    {
        TransactionGuard t4;
        assert(f[0] == 10);
        assert(f[1] == 20);
    }

    printf("PASS: %s\n", __FUNCTION__);
}


void testPushBackNRead() {
    TVector_nopred<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    {
        TransactionGuard t2;
        f.push_back(4);
        assert(f[1] == 4);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testPushBackNRead1() {
    TVector_nopred<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    TestTransaction t2(1);
    f.push_back(4);
    assert(f[1] == 4);

    TestTransaction t3(2);
    f.push_back(20);
    assert(t3.try_commit());
    assert(!t2.try_commit());

    printf("PASS: %s\n", __FUNCTION__);
}

void testPushBackNRead2() {
    TVector_nopred<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    TestTransaction t2(1);
    f.push_back(4);
    assert(f[f.size() - 1] == 4);

    TestTransaction t3(2);
    f.push_back(20);

    assert(t3.try_commit());
    assert(!t2.try_commit()); // TODO: this can actually commit

    printf("PASS: %s\n", __FUNCTION__);
}


void testPushBackNWrite() {
    TVector_nopred<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    {
        TransactionGuard t2;
        f.push_back(4);
        f[1] = 10;
    }

    {
        TransactionGuard t3;
        assert(f[1] == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}



void testSimpleString() {
    TVector_nopred<std::string> f;

    {
        TransactionGuard t;
        f.push_back("100");
    }

    {
        TransactionGuard t2;
        std::string f_read = f[0];
        assert(f_read.compare("100") == 0);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testIter() {
    std::vector<int> arr;
    TVector_nopred<int> f;
    TRANSACTION_E {
    for (int i = 0; i < 10; i++) {
        int x = rand();
        f.push_back(x);
        arr.push_back(x);
    }
    } RETRY_E(false);

    int max;
    TRANSACTION_E {
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY_E(false);

    assert(max == *(std::max_element(arr.begin(), arr.end())));
    printf("Max is %i\n", max);
    printf("PASS: %s\n", __FUNCTION__);
}


void testConflictingIter() {
    TVector_nopred<int> f;
    TBox<int> box;
    TRANSACTION_E {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY_E(false);

    TestTransaction t(1);
    std::max_element(f.begin(), f.end());
    box = 9; /* avoid read-only txn */

    TestTransaction t1(2);
    f[4] = 10;
    assert(t1.try_commit());
    assert(!t.try_commit());

    printf("PASS: %s\n", __FUNCTION__);
}

void testModifyingIter() {
    TVector_nopred<int> f;
    {
        TransactionGuard tt;
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    }

    {
        TransactionGuard t;
        std::replace(f.begin(), f.end(), 4, 6);
    }

    {
        TransactionGuard t1;
        int v = f[4];
        assert(v == 6);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testConflictingModifyIter1() {
    TVector_nopred<int> f;
    TRANSACTION_E {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY_E(false);

    TestTransaction t(1);
    std::replace(f.begin(), f.end(), 4, 6);

    TestTransaction t1(2);
    f[4] = 10;
    assert(t1.try_commit());
    assert(!t.try_commit());

    {
        TransactionGuard t2;
        int v = f[4];
        assert(v == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testConflictingModifyIter2() {
    TVector_nopred<int> f;
    TRANSACTION_E {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY_E(false);

    {
        TransactionGuard t;
        std::replace(f.begin(), f.end(), 4, 6);
    }

    {
        TransactionGuard t1;
        f[4] = 10;
    }

    {
        TransactionGuard t2;
        int v = f[4];
        assert(v == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testConflictingModifyIter3() {
    TVector_nopred<int> f;
    TBox<int> box;
    TRANSACTION_E {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY_E(false);

    TestTransaction t1(1);
    (int) f[4];
    box = 9; /* avoid read-only txn */

    TestTransaction t(2);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t2;
        int v = f[4];
        assert(v == 6);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testIterNPushBack() {
    TVector_nopred<int> f;
    TRANSACTION_E {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY_E(false);

    int max;
    TRANSACTION_E {
        f.push_back(20);
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY_E(false);

    assert(max == 20);
    printf("PASS: %s\n", __FUNCTION__);
}

void testIterNPushBack1() {
    TVector_nopred<int> f;
    {
        TransactionGuard tt;
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    }

    int max;
    TestTransaction t1(1);
    f.push_back(20);
    max = *(std::max_element(f.begin(), f.end()));
    assert(max == 20);

    TestTransaction t2(2);
    f.push_back(12);
    assert(t2.try_commit());
    assert(!t1.try_commit());

    printf("PASS: %s\n", __FUNCTION__);
}

void testIterNPushBack2() {
    TVector_nopred<int> f;
    TBox<int> box;

    {
        TransactionGuard tt;
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    }

    TestTransaction t1(1);
    std::max_element(f.begin(), f.end());
    box = 9; /* avoid read-only txn */

    TestTransaction t2(2);
    f.push_back(2);
    assert(t2.try_commit());
    assert(!t1.try_commit());

    printf("PASS: %s\n", __FUNCTION__);
}


void testErase() {
    TVector_nopred<int> f;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++)
            f.push_back(i);
    } RETRY_E(false);

    {
        TransactionGuard t1;
        f.erase(f.begin() + 5);
    }

    TRANSACTION_E {
        assert(f.size() == 9);
        int x[9] = {0, 1, 2, 3, 4, 6, 7, 8, 9};
        for (int i = 0; i < 9; ++i)
            assert(f[i] == x[i]);
    } RETRY_E(false);

    printf("PASS: %s\n", __FUNCTION__);
}

void testInsert() {
    TVector_nopred<int> f;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++)
            f.push_back(i);
    } RETRY_E(false);

    {
        TransactionGuard t1;
        f.insert(f.begin() + 5, 25);
    }

    TRANSACTION_E {
        assert(f.size() == 11);
        int x[] = {0, 1, 2, 3, 4, 25, 5, 6, 7, 8, 9};
        for (int i = 0; i < 11; ++i)
            assert(f[i] == x[i]);
    } RETRY_E(false);

    GUARDED {
        f.insert(f.end(), 30);
    }

    TRANSACTION_E {
        assert(f.size() == 12);
        int x[] = {0, 1, 2, 3, 4, 25, 5, 6, 7, 8, 9, 30};
        for (int i = 0; i < 12; ++i)
            assert(f[i] == x[i]);
    } RETRY_E(false);

    printf("PASS: %s\n", __FUNCTION__);
}

void testPushNPop() {
    TVector_nopred<int> f;
    TBox<int> box;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY_E(false);

    {
        TransactionGuard t;
        f.push_back(20);
        f.push_back(21);
        assert(f[0] == 0);
        assert(f.size() == 12);
        //t.print(std::cerr);
        f.pop_back();
        f.pop_back();
    }

    {
        TestTransaction t1(1);
        f.pop_back();
        f.push_back(15);

        TestTransaction t2(2);
        f.push_back(20);
        assert(t2.try_commit());
        assert(!t1.try_commit());

        TRANSACTION_E {
            assert(f.size() == 11);
            assert(f[10] == 20);
        } RETRY_E(false);
    }

    {
        TestTransaction t1(1);
        f.push_back(18);
        assert(f[11] == 18);

        TestTransaction t2(2);
        f.push_back(21);
        assert(t2.try_commit());
        //t1.print(std::cerr);
        assert(!t1.try_commit());

        TRANSACTION_E {
            assert(f.size() == 12);
            assert(f[10] == 20);
            assert(f[11] == 21);
            f.pop_back();
        } RETRY_E(false);
    }

    printf("PASS: testPushNPop\n");

    TestTransaction t3(1);
    f.pop_back();
    f.push_back(15);

    TestTransaction t4(2);
    f.pop_back();
    assert(t4.try_commit());
    assert(!t3.try_commit());

    TRANSACTION_E {
        assert(f.size() == 10);
    } RETRY_E(false);

    printf("PASS: testPushNPop1\n");

    TestTransaction t5(1);
    f.pop_back();
    f.pop_back();
    f.push_back(15);

    TestTransaction t6(2);
    f[8] = 16;
    assert(t6.try_commit());

    TestTransaction t7(3);
    (int) f[8];
    box = 9; /* avoid read-only txn */

    assert(t5.try_commit());
    assert(!t7.try_commit());

    TRANSACTION_E {
        assert(f.size() == 9);
        assert(f[8] == 15);
    } RETRY_E(false);

    printf("PASS: testPushNPop2\n");
}

void testPopAndUdpate() {
    TVector_nopred<int> f;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY_E(false);

    TestTransaction t1(1);
    f[9] = 20;

    TestTransaction t2(2);
    f.pop_back();
    f.pop_back();
    assert(t2.try_commit());
    assert(!t1.try_commit());
}

void testMulPushPops() {
    TVector_nopred<int> f;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY_E(false);

    {
        TransactionGuard t1;
        f.push_back(100);
        f.push_back(200);
        f.pop_back();
        f.pop_back();
        f.pop_back();
    }
}

void testMulPushPops1() {
    TVector_nopred<int> f;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY_E(false);

    {
        TransactionGuard t1;
        f.push_back(100);
        f.push_back(200);
        f.pop_back();
        f.pop_back();
        f.push_back(300);
    }
}

void testUpdatePop() {
    TVector_nopred<int> f;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY_E(false);

    {
        TransactionGuard t1;
        f[9] = 20;
        f.pop_back();
    }
}

void testIteratorBetterSemantics() {
    TVector_nopred<int> f;
    TBox<int> box;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY_E(false);

    TestTransaction t1(1);
    // XXX std::find uses a special case for random-access iterators
    // that calls size()
    TVector_nopred<int>::iterator it;
    for (it = f.begin(); it != f.end(); ++it)
        if (*it == 5)
            break;
    box = 9; /* avoid read-only txn */

    TestTransaction t2(2);
    f.push_back(12);
    assert(t2.try_commit());
    t1.use();
    int x = *it;
    assert(!t1.try_commit());
    (void) x;

    printf("PASS: %s\n", __FUNCTION__);
}

void testSizePredicates() {
    TVector_nopred<int> f;
    TBox<int> box;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++)
            f.push_back(i);
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f.size() > 5);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        for (int i = 0; i < 4; ++i)
            f.pop_back();
        assert(t2.try_commit());
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 6);
    }

    {
        TestTransaction t1(1);
        assert(f.size() > 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 4)
            f.pop_back();
        assert(t2.try_commit());
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 4);
    }

    {
        TestTransaction t0(1);
        while (f.size() < 6)
            f.push_back(f.size());
        assert(t0.try_commit());

        TestTransaction t1(1);
        assert(f.size() > 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 4)
            f.pop_back();
        assert(t1.try_commit());
        assert(t2.try_commit());

        assert(f.nontrans_size() == 4);
    }

    {
        TestTransaction t1(1);
        assert(f.size() < 6);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() < 6)
            f.push_back(f.size());
        assert(t2.try_commit());
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 6);
    }

    {
        TestTransaction t1(1);
        assert(f.size() < 8);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() < 8)
            f.push_back(f.size());
        assert(t1.try_commit());
        assert(t2.try_commit());

        assert(f.nontrans_size() == 8);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testIterPredicates() {
    TVector_nopred<int> f;
    TBox<int> box;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++)
            f.push_back(i);
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f.begin() + 5 < f.end());
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        for (int i = 0; i < 4; ++i)
            f.pop_back();
        assert(t2.try_commit());
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 6);
    }

    {
        TestTransaction t1(1);
        assert(f.begin() + 4 < f.end());
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 4)
            f.pop_back();
        assert(t2.try_commit());
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 4);
    }

    {
        TestTransaction t0(1);
        while (f.size() < 6)
            f.push_back(f.size());
        assert(t0.try_commit());

        TestTransaction t1(1);
        assert(f.end() > f.begin() + 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 4)
            f.pop_back();
        assert(t1.try_commit());
        assert(t2.try_commit());

        assert(f.nontrans_size() == 4);
    }

    {
        TestTransaction t1(1);
        assert(f.end() <= f.begin() + 5);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() < 6)
            f.push_back(f.size());
        assert(t2.try_commit());
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 6);
    }

    {
        TestTransaction t1(1);
        assert(f.end() - f.begin() < 8);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() < 8)
            f.push_back(f.size());
        assert(t1.try_commit());
        assert(t2.try_commit());

        assert(f.nontrans_size() == 8);
    }

    {
        TestTransaction t1(1);
        assert(f.end() - f.begin() < 9);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() < 9)
            f.push_back(f.size());
        assert(t2.try_commit());
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 9);
    }

    {
        TestTransaction t1(1);
        assert(f.begin() - f.end() == -9);
        assert(t1.try_commit());

        assert(f.nontrans_size() == 9);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testResize() {
    TVector_nopred<int> f;
    TBox<int> box;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++)
            f.push_back(i);
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        f.resize(4);
        assert(t2.try_commit());

        t1.use();
        assert(f.size() >= 4);
        assert(t1.try_commit());

        assert(f.nontrans_size() == 4);
    }

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f.size() >= 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        f.resize(5, -100);
        assert(t2.try_commit());
        assert(!t1.try_commit());

        TestTransaction t3(3);
        assert(f.size() == 5);
        assert(f[0] == 0);
        assert(f[1] == 1);
        assert(f[2] == 2);
        assert(f[3] == 3);
        assert(f[4] == -100);
        assert(t3.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testFrontBack() {
    TVector_nopred<int> v;

    GUARDED {
        for (int i = 0; i < 10; ++i)
            v.push_back(i);
    }

    GUARDED {
        assert(v.size() == 10);
    }

    GUARDED {
        assert(v.front() == 0);
        assert(v.back() == 9);

        v.pop_back();

        assert(v.back() == 8);

        v.push_back(30);

        assert(v.back() == 30);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testOpacity() {
    TVector_nopred<int> f;
    TBox<int> box;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++)
            f.push_back(i);
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        f.pop_back();
        assert(t2.try_commit());

        t1.use();
        assert(f[2] == 2);
        assert(t1.try_commit());

        assert(f.nontrans_size() == 9);
    }

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f[4] == 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 3)
            f.pop_back();
        assert(t2.try_commit());

        t1.use();
        assert(f[2] == 2);
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 3);
    }

    TRANSACTION_E {
        while (f.size() < 10)
            f.push_back(f.size());
    } RETRY_E(false);

    try {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f[4] == 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 3)
            f.pop_back();
        f[2] = -1;
        assert(t2.try_commit());

        t1.use();
        assert(f[2] == -1);
        assert(false && "should not get here b/c opacity");
    } catch (Transaction::Abort e) {
    }

    TRANSACTION_E {
        while (f.size() < 10)
            f.push_back(f.size());
    } RETRY_E(false);

    try {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f[4] == 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 3)
            f.pop_back();
        f[2] = -1;
        assert(t2.try_commit());

        t1.use();
        assert(f.size() > 4);
        assert(false && "should not get here b/c opacity");
    } catch (Transaction::Abort e) {
    }

    TRANSACTION_E {
        while (f.size() < 10)
            f.push_back(f.size());
    } RETRY_E(false);

    try {
        TestTransaction t1(1);
        f[1] = 1;
        f[4] = 4;

        TestTransaction t2(2);
        while (f.size() > 3)
            f.pop_back();
        f[2] = -1;
        assert(t2.try_commit());

        t1.use();
        assert(f.size() > 4);
        assert(false && "should not get here b/c opacity");
    } catch (Transaction::Abort e) {
    }

    TRANSACTION_E {
        while (f.size() < 10)
            f.push_back(f.size());
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f[4] == 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 5)
            f.pop_back();
        f[2] = -1;
        assert(t2.try_commit());

        t1.use();
        assert(f[2] == -1);
        assert(t1.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testNoOpacity() {
    TVector_nopred<int, TNonopaqueWrapped> f;
    TBox<int, TNonopaqueWrapped<int> > box;

    TRANSACTION_E {
        for (int i = 0; i < 10; i++)
            f.push_back(i);
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        f.pop_back();
        assert(t2.try_commit());

        t1.use();
        assert(f[2] == 2);
        assert(t1.try_commit());

        assert(f.nontrans_size() == 9);
    }

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f[4] == 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 3)
            f.pop_back();
        assert(t2.try_commit());

        t1.use();
        assert(f[2] == 2);
        assert(!t1.try_commit());

        assert(f.nontrans_size() == 3);
    }

    TRANSACTION_E {
        while (f.size() < 10)
            f.push_back(f.size());
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f[4] == 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 3)
            f.pop_back();
        f[2] = -1;
        assert(t2.try_commit());

        t1.use();
        assert(f[2] == -1);
        assert(!t1.try_commit());
    }

    TRANSACTION_E {
        while (f.size() < 10)
            f.push_back(f.size());
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f[4] == 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 3)
            f.pop_back();
        f[2] = -1;
        assert(t2.try_commit());

        t1.use();
        assert(f.size() <= 4);
        assert(!t1.try_commit());
    }

    TRANSACTION_E {
        while (f.size() < 10)
            f.push_back(f.size());
    } RETRY_E(false);

    {
        TestTransaction t1(1);
        assert(f[1] == 1);
        assert(f[4] == 4);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        while (f.size() > 5)
            f.pop_back();
        f[2] = -1;
        assert(t2.try_commit());

        t1.use();
        assert(f[2] == -1);
        assert(t1.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}



int main() {
    testSimpleInt();
    testWriteNPushBack();
    testPushBack();
    testPushBackNRead();
    testPushBackNRead1();
    testPushBackNRead2();
    testPushBackNWrite();
    testSimpleString();
    testIter();
    testConflictingIter();
    testModifyingIter();
    testConflictingModifyIter1();
    testConflictingModifyIter2();
    testConflictingModifyIter3();
    testIterNPushBack();
    testIterNPushBack1();
    testIterNPushBack2();
    testErase();
    testInsert();
    testPushNPop();
    testPopAndUdpate();
    testMulPushPops();
    testMulPushPops1();
    testUpdatePop();
    testIteratorBetterSemantics();
    testSizePredicates();
    testIterPredicates();
    testResize();
    testFrontBack();
    testOpacity();
    testNoOpacity();

    std::thread advancer;  // empty thread because we have no advancer thread
    Transaction::rcu_release_all(advancer, 2);
    return 0;
}
