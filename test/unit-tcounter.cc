#undef NDEBUG
#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include "Transaction.hh"
#include "TCounter.hh"
#include "TBox.hh"

void testTrivial() {
	TCounter<int> c;

    {
        TransactionGuard t;
        c = 1;
    }

	{
        TransactionGuard t;
        int i_read = c;
        assert(i_read == 1);
    }

	printf("PASS: %s\n", __FUNCTION__);
}

void testConcurrentUpdate() {
    TCounter<int> c;
    bool b;

    std::vector<int> permutation{1, 2, 3, 4};
    do {
        c.nontrans_write(0);

        TestTransaction t1(1);
        ++c;

        TestTransaction t2(2);
        ++c;

        TestTransaction t3(3);
        c -= 1;

        TestTransaction t4(4);
        c += 5;

        for (auto which : permutation)
            switch (which) {
            case 1:
                assert(t1.try_commit());
                break;
            case 2:
                assert(t2.try_commit());
                break;
            case 3:
                assert(t3.try_commit());
                break;
            case 4:
                assert(t4.try_commit());
                break;
            }

        assert(c.nontrans_read() == 6);
    } while (std::next_permutation(permutation.begin(), permutation.end()));

    {
        TestTransaction t1(1);
        b = c >= 4;
        assert(b);
        c += 4;

        TestTransaction t2(2);
        c -= 2;
        assert(t2.try_commit());

        t1.use();
        b = c >= 8;
        assert(b);
        assert(t1.try_commit());
        assert(c.nontrans_read() == 8);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleRangesOk() {
    TCounter<int> c;
    TBox<int> box;
    bool match;

    {
        TestTransaction t1(1);
        ++c;

        TestTransaction t2(2);
        c = 1;
        assert(t2.try_commit());
        t1.use();
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(1);
        match = c > -4;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = -1;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    c.nontrans_write(1);

    {
        TestTransaction t1(1);
        match = c == 1;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 2;
        assert(t2.try_commit());

        TestTransaction t3(2);
        c = 1;
        assert(t3.try_commit());

        assert(t1.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleRangesFail() {
    TCounter<int> c;
    TBox<int> box;
    bool match;

    {
        TestTransaction t1(1);
        match = c < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 5;
        assert(t2.try_commit());
        assert(!t1.try_commit());
    }

    c.nontrans_write(0);

    try {
        TestTransaction t1(1);
        match = c < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 5;
        assert(t2.try_commit());

        t1.use();
        match = c > 1;
        assert(false && "should not get here b/c opacity");
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
    }

    c.nontrans_write(0);

    try {
        TestTransaction t1(1);
        match = c < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 5;
        assert(t2.try_commit());

        t1.use();
        match = c > 1;
        assert(false && "should not get here b/c opacity");
        assert(match);

        TestTransaction t3(2);
        c = 2;
        assert(t3.try_commit());
        assert(t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
    }

    c.nontrans_write(4);

    try {
        TestTransaction t1(1);
        match = c > 1;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 0;
        assert(t2.try_commit());

        t1.use();
        match = c < 1;
        assert(false && "should not get here");
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleRangesFailNoOpacity() {
    TCounter<int, TNonopaqueWrapped<int> > c;
    TBox<int> box;
    bool match;

    {
        TestTransaction t1(1);
        match = c < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 5;
        assert(t2.try_commit());
        assert(!t1.try_commit());
    }

    c.nontrans_write(0);

    try {
        TestTransaction t1(1);
        match = c < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 5;
        assert(t2.try_commit());

        t1.use();
        match = c > 1;
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
    }

    c.nontrans_write(0);

    try {
        TestTransaction t1(1);
        match = c < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 5;
        assert(t2.try_commit());

        t1.use();
        match = c > 1;
        assert(match);

        TestTransaction t3(2);
        c = 2;
        assert(t3.try_commit());
        assert(t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
    }

    c.nontrans_write(4);

    try {
        TestTransaction t1(1);
        match = c > 1;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        c = 0;
        assert(t2.try_commit());

        t1.use();
        match = c < 1;
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testUpdateRead() {
    TCounter<int> c;
    TBox<int> box;
    bool match;

    {
        TestTransaction t1(1);
        ++c;
        match = c > 0;
        assert(match);

        TestTransaction t2(2);
        c = 1;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    assert(c.nontrans_read() == 2);

    {
        TestTransaction t1(1);
        ++c;
        match = c > 0;
        assert(match);

        TestTransaction t2(2);
        c = -1;
        assert(t2.try_commit());
        assert(!t1.try_commit());
    }

    assert(c.nontrans_read() == -1);

    printf("PASS: %s\n", __FUNCTION__);
}

void testOpacity() {
    TCounter<int> c1, c2;
    bool match;

    try {
        TestTransaction t1(1);
        ++c1;
        match = c1 > 0;
        assert(match);

        TestTransaction t2(2);
        c2 = 5;
        --c1;
        assert(t2.try_commit());

        t1.use();
        match = c2 > 4;
        assert(false && "shouldn't get here");
        assert(match);
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
    }

    {
        TransactionGuard g;
        match = c2 > 4;
        assert(match);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testNoOpacity() {
    TCounter<int, TNonopaqueWrapped<int> > c1, c2;
    bool match;

    try {
        TestTransaction t1(1);
        ++c1;
        match = c1 > 0;
        assert(match);

        TestTransaction t2(2);
        c2 = 5;
        --c1;
        assert(t2.try_commit());

        t1.use();
        match = c2 > 4;
        assert(match);
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
    }

    {
        TransactionGuard g;
        match = c2 > 4;
        assert(match);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    testTrivial();
    testConcurrentUpdate();
    testSimpleRangesOk();
    testSimpleRangesFail();
    testSimpleRangesFailNoOpacity();
    testUpdateRead();
    testOpacity();
    testNoOpacity();
    return 0;
}
