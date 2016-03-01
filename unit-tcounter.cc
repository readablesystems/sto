#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include "Transaction.hh"
#include "TCounter.hh"

void testTrivial() {
	TCounter<int> ip;

    {
        TransactionGuard t;
        ip = 1;
    }

	{
        TransactionGuard t;
        int i_read = ip;
        assert(i_read == 1);
    }

	printf("PASS: %s\n", __FUNCTION__);
}

void testConcurrentUpdate() {
    TCounter<int> ip;
    bool b;

    std::vector<int> permutation{1, 2, 3, 4};
    do {
        ip.nontrans_write(0);

        TestTransaction t1(1);
        ++ip;

        TestTransaction t2(2);
        ++ip;

        TestTransaction t3(3);
        ip -= 1;

        TestTransaction t4(4);
        ip += 5;

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

        assert(ip.nontrans_read() == 6);
    } while (std::next_permutation(permutation.begin(), permutation.end()));

    {
        TestTransaction t1(1);
        b = ip >= 4;
        assert(b);
        ip += 4;

        TestTransaction t2(2);
        ip -= 2;
        assert(t2.try_commit());

        t1.use();
        b = ip >= 8;
        assert(b);
        assert(t1.try_commit());
        assert(ip.nontrans_read() == 8);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleRangesOk() {
    TCounter<int> ip;
    bool match;

    {
        TestTransaction t1(1);
        ++ip;

        TestTransaction t2(2);
        ip = 1;
        assert(t2.try_commit());
        t1.use();
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(1);
        match = ip > -4;
        assert(match);

        TestTransaction t2(2);
        ip = -1;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    ip.nontrans_write(1);

    {
        TestTransaction t1(1);
        match = ip == 1;
        assert(match);

        TestTransaction t2(2);
        ip = 2;
        assert(t2.try_commit());

        TestTransaction t3(2);
        ip = 1;
        assert(t3.try_commit());

        assert(t1.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleRangesFail() {
    TCounter<int> ip;
    bool match;

    {
        TestTransaction t1(1);
        match = ip < 3;
        assert(match);

        TestTransaction t2(2);
        ip = 5;
        assert(t2.try_commit());

        assert(!t1.try_commit());
    }

    ip.nontrans_write(0);

    {
        TestTransaction t1(1);
        match = ip < 3;
        assert(match);

        TestTransaction t2(2);
        ip = 5;
        assert(t2.try_commit());

        t1.use();
        match = ip > 1;
        // XXX we are actually able to abort here, but don't, because there
        // *does* exist an eventual value that satisfies all constraints (2)
        assert(!t1.try_commit());
    }

    ip.nontrans_write(0);

    {
        TestTransaction t1(1);
        match = ip < 3;
        assert(match);

        TestTransaction t2(2);
        ip = 5;
        assert(t2.try_commit());

        t1.use();
        match = ip > 1;
        // XXX we are actually able to abort here, but don't, because there
        // *does* exist an eventual value that satisfies all constraints (2).
        // It would probably be better to abort.
        assert(match);

        TestTransaction t3(2);
        ip = 2;
        assert(t3.try_commit());
        assert(t1.try_commit());
    }

    ip.nontrans_write(4);

    try {
        TestTransaction t1(1);
        match = ip > 1;
        assert(match);

        TestTransaction t2(2);
        ip = 0;
        assert(t2.try_commit());

        t1.use();
        match = ip < 1;
        assert(false && "should not get here");
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testUpdateRead() {
    TCounter<int> ip;
    bool match;

    {
        TestTransaction t1(1);
        ++ip;
        match = ip > 0;
        assert(match);

        TestTransaction t2(2);
        ip = 1;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    assert(ip.nontrans_read() == 2);

    {
        TestTransaction t1(1);
        ++ip;
        match = ip > 0;
        assert(match);

        TestTransaction t2(2);
        ip = -1;
        assert(t2.try_commit());
        assert(!t1.try_commit());
    }

    assert(ip.nontrans_read() == -1);

    printf("PASS: %s\n", __FUNCTION__);
}

int main() {
    testTrivial();
    testConcurrentUpdate();
    testSimpleRangesOk();
    testSimpleRangesFail();
    testUpdateRead();
    return 0;
}
