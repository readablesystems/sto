#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "TIntPredicate.hh"

void testTrivial() {
	TIntPredicate<int> ip;

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

void testSimpleRangesOk() {
    TIntPredicate<int> ip;
    bool match;

    {
        TestTransaction t1(1);
        match = ip < 3;
        assert(match);

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

    ip.unsafe_write(1);

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
    TIntPredicate<int> ip;
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

    ip.unsafe_write(0);

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

    ip.unsafe_write(0);

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

    ip.unsafe_write(4);

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

int main() {
    assert((std::is_trivially_copyable<TIntPredicate<int>::pair_type>::value));
    testTrivial();
    testSimpleRangesOk();
    testSimpleRangesFail();
    return 0;
}
