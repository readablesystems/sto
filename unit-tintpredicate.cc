#undef NDEBUG
#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "TIntPredicate.hh"
#include "StringWrapper.hh"
#include "TBox.hh"

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
    TBox<int> box;
    bool match;

    {
        TestTransaction t1(1);
        match = ip < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ip = 1;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(1);
        match = ip < 0;
        assert(!match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ip = 0;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(1);
        match = ip > -1;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ip = -1;
        assert(t2.try_commit());
        assert(!t1.try_commit());
    }

    {
        TestTransaction t1(1);
        match = ip > 0;
        assert(!match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ip = 0;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(1);
        match = ip >= 0;
        assert(match);
        match = ip < 4;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ip = 3;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    {
        TestTransaction t1(1);
        match = ip >= 0;
        assert(match);
        match = ip < 4;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ip = -1;
        assert(t2.try_commit());
        assert(!t1.try_commit());
    }

    ip.nontrans_write(3);

    {
        TestTransaction t1(1);
        match = ip >= 0;
        assert(match);
        match = ip < 4;
        assert(match);
        box = 9; /* avoid read-only txn */

        TestTransaction t2(2);
        ip = 4;
        assert(t2.try_commit());
        assert(!t1.try_commit());
    }

    {
        TestTransaction t1(1);
        match = ip > -4;
        assert(match);
        box = 9; /* avoid read-only txn */

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
        box = 9; /* avoid read-only txn */

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
    TBox<int> box;
    bool match;

    {
        TestTransaction t1(1);
        match = ip < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

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
        box = 9; /* avoid read-only txn */

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
        box = 9; /* avoid read-only txn */

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
        box = 9; /* avoid read-only txn */

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
    assert((mass::is_trivially_copyable<TIntPredicate<int>::pred_type>::value));
    assert((mass::is_trivially_copyable<TVersion>::value));
    assert((mass::is_trivially_copyable<TNonopaqueVersion>::value));

    // some assertions about TransactionBuffer
    {
        TransactionBuffer buf;
        void* v1 = Packer<uintptr_t>::pack(buf, 12938);
        void* v2 = Packer<uintptr_t>::pack_unique(buf, 12938);
        void* v3 = Packer<uintptr_t>::pack(buf, 12938);
        void* v4 = Packer<uintptr_t>::pack_unique(buf, 12938);
        assert(v1 == v2 && v2 == v3 && v3 == v4 && v4 == (void*) (uintptr_t) 12938);
        assert(Packer<uintptr_t>::unpack(v1) == 12938);
        assert(Packer<uintptr_t>::unpack(v2) == 12938);
        assert(Packer<uintptr_t>::unpack(v3) == 12938);
        assert(Packer<uintptr_t>::unpack(v4) == 12938);

        v1 = Packer<std::string>::pack(buf, "Hello");
        v2 = Packer<std::string>::pack_unique(buf, "Hello");
        v3 = Packer<std::string>::pack(buf, "Hello2");
        v4 = Packer<std::string>::pack_unique(buf, "Hello2");
        void* v5 = Packer<std::string>::pack(buf, "Hello");
        void* v6 = Packer<std::string>::pack_unique(buf, "Hello");
        assert(v1 != v2 && v1 != v5 && v2 != v5 && v1 != v6 && v5 != v6);
        assert(v2 == v6);
        assert(Packer<std::string>::unpack(v1) == "Hello");
        assert(Packer<std::string>::unpack(v2) == "Hello");
        assert(Packer<std::string>::unpack(v3) == "Hello2");
        assert(Packer<std::string>::unpack(v4) == "Hello2");
        assert(Packer<std::string>::unpack(v5) == "Hello");
        assert(Packer<std::string>::unpack(v6) == "Hello");

        std::string hello("Hello");
        void* v7 = Packer<std::string>::pack(buf, StringWrapper(hello));
        assert(v7 == &hello);
    }


    testTrivial();
    testSimpleRangesOk();
    testSimpleRangesFail();
    return 0;
}
