#undef NDEBUG
#include <string>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "Commutators.hh"
#include "TMvBox.hh"
//XXX disabled string wrapper due to unknown compiler issue
//#include "StringWrapper.hh"

#define GUARDED if (TransactionGuard tguard{})

void testSimpleInt() {
    TMvBox<int> f;

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
    TMvBox<std::string> f;

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
    TMvBox<int> ib;
    TMvBox<int> box;
    bool match;

    {
        TestTransaction t1(1);
        match = ib < 3;
        assert(match);

        TestTransaction t2(2);
        Sto::mvcc_rw_upgrade();
        ib = 1;
        assert(t2.try_commit());
        assert(t1.try_commit());
    }

    

    {
        TestTransaction t1(1);
        Sto::mvcc_rw_upgrade();
        ib = 1;

        TestTransaction t2(2);
        Sto::mvcc_rw_upgrade();
        ib = 2;
        assert(t2.try_commit());
        assert(t1.try_commit());

        assert(ib.nontrans_read() == 1);
    }

    
    ib.nontrans_write(2);

    {
        TestTransaction t1(1);
        Sto::mvcc_rw_upgrade();
        match = ib == 2;
        assert(match);

        TestTransaction t2(2);
        Sto::mvcc_rw_upgrade();
        ib = 0;
        assert(t2.try_commit());

        

        TestTransaction t3(3);
        Sto::mvcc_rw_upgrade();
        ib = 2;
        assert(t3.try_commit());
        assert(t1.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testOpacity1() {
    TMvBox<int> f, g;
    TMvBox<int> box;
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
        assert(x == 0);
        assert(t1.try_commit());
    }

    

    {
        TransactionGuard t2;
        Sto::mvcc_rw_upgrade();
        int v = f;
        assert(v == 2);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testMvReads() {
    TMvBox<int> f, g;
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

    // Later reads also don't validate earlier writes
    {
        TestTransaction t1(1);
        Sto::mvcc_rw_upgrade();
        g = g + f;

        TestTransaction t2(2);
        Sto::mvcc_rw_upgrade();
        int x = f + g;
        assert(x == 1);

        t1.use();
        assert(t1.try_commit());

        t2.use();
        assert(t2.try_commit());
    }

    
    f.nontrans_write(1);
    g.nontrans_write(2);

    // Later reads of earlier transactions do not invalidate earlier writes of
    // later transactions
    {
        TestTransaction t1(1);
        Sto::mvcc_rw_upgrade();
        int x = f + g;

        TestTransaction t2(2);
        Sto::mvcc_rw_upgrade();
        f = f + 2 * g;

        t1.use();
        x = f + g;
        assert(x == 3);

        t2.use();
        assert(t2.try_commit());

        t1.use();
        assert(t1.try_commit());
    }

    
    f.nontrans_write(0);
    g.nontrans_write(1);

    // Read-my-writes support
    {
        TestTransaction t(1);
        Sto::mvcc_rw_upgrade();
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
    TMvBox<int> f, g;
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

    
    f.nontrans_write(1);
    g.nontrans_write(0);

    // Later reads in RW-transactions should invalidate concurrent writes
    {
        TestTransaction t1(1);
        Sto::mvcc_rw_upgrade();
        f = 0;

        TestTransaction t2(2);
        Sto::mvcc_rw_upgrade();
        g = f;

        t2.use();
        assert(t2.try_commit());

        t1.use();
        assert(t1.try_commit());
    }

    
    f.nontrans_write(1);
    g.nontrans_write(0);

    // Earlier reads in RW-transactions should not invalidate concurrent writes
    {
        TestTransaction t1(1);
        Sto::mvcc_rw_upgrade();
        f = g;

        TestTransaction t2(2);
        Sto::mvcc_rw_upgrade();
        g = 1;

        t1.use();
        assert(t1.try_commit());

        t2.use();
        assert(t2.try_commit());
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testMvCommute1() {
    TMvCommuteIntegerBox box;
    box.nontrans_write(0);

    {
        TestTransaction t1(1);
        Sto::mvcc_rw_upgrade();
        box.increment(1);

        TestTransaction t2(2);
        t2.use();
        Sto::mvcc_rw_upgrade();
        box.increment(2);
        assert(t2.try_commit());

        t1.use();
        assert(t1.try_commit());

        TestTransaction t3(3);
        t3.use();
        Sto::mvcc_rw_upgrade();
        auto v = static_cast<int>(box);
        assert(v == 3);
        assert(t3.try_commit());
    }
    printf("PASS: %s\n", __FUNCTION__);
}

void testMvCommute2() {
    TMvCommuteIntegerBox box;
    box.nontrans_write(0);

    {
        TestTransaction t1(1);
        Sto::mvcc_rw_upgrade();
        box.increment(1);
        assert(t1.try_commit());

        TestTransaction t2(2);
        Sto::mvcc_rw_upgrade();
        box.increment(2);

        TestTransaction t3(3);
        Sto::mvcc_rw_upgrade();
        auto v = static_cast<int>(box);
        assert(v == 1);
        assert(t3.try_commit());

        TestTransaction t4(4);
        box.increment(4);
        assert(t4.try_commit());

        t2.use();
        assert(t2.try_commit());
    }
    printf("PASS: %s\n", __FUNCTION__);
}

void testMvInline() {
    TMvBox<int> box;
    box.nontrans_write(0);

    const int *v0 = &box.nontrans_access();
    assert(*v0 == 0);

    {
        TestTransaction t1(1);
        box = 1;
        assert(t1.try_commit());
    }

    const int *v1 = &box.nontrans_access();
    assert(*v1 == 1);
    assert(v0 != v1);

    Transaction::epoch_advance_once();

    {
        TestTransaction t2(2);
        box = 2;
        assert(t2.try_commit());
    }

    const int *v2 = &box.nontrans_access();
    assert(*v2 == 2);
    assert(v1 != v2);
    assert(v0 == v2);

    printf("PASS: %s\n", __FUNCTION__);
}

void testCommuteGC() {
    TMvCommuteIntegerBox box;
    box.nontrans_write(0);

    {
        TestTransaction t1(1);
        box.increment(1);
        assert(t1.try_commit());

        TestTransaction t2(2);
        box.increment(2);
        assert(t2.try_commit());

        TestTransaction t3(3);
        box.increment(3);
        assert(t3.try_commit());

        TestTransaction t4(4);
        box.increment(4);
        assert(t4.try_commit());
    }

    Transaction::epoch_advance_once();

    MvHistory<int64_t> *h = nullptr;
    {
        TestTransaction t(5);
        Sto::mvcc_rw_upgrade();
        h = TMvBoxAccess::head(box);  // Needs to be called in a transaction
        assert(t.try_commit());
        assert(h);
        assert(h->status_is(COMMITTED_DELTA));
        auto hh = h;
        for (auto i = 0; i < 4; i++) {
            assert(hh);
            assert(hh->status_is(COMMITTED_DELTA));
            hh = hh->prev();
        }
        assert(hh);
        assert(hh->status() == COMMITTED);
        assert(!hh->prev());
    }

    {
        TestTransaction t(6);
        Sto::mvcc_rw_upgrade();
        auto hh = TMvBoxAccess::head(box);
        assert(t.try_commit());
        assert(h == hh);
        assert(h->status_is(COMMITTED_DELTA));
        assert(h->prev());
        assert(h->prev()->status_is(COMMITTED));
    }

    assert(10 == box.nontrans_read());

    printf("PASS: %s\n", __FUNCTION__);
}


int main() {
    testSimpleInt();
    testSimpleString();
    testConcurrentInt();
    testOpacity1();
    testMvReads();
    testMvWrites();
    testMvCommute1();
    testMvCommute2();
    testCommuteGC();
#if MVCC_INLINING
    testMvInline();
#endif

    std::thread advancer;  // empty thread because we have no advancer thread
    Transaction::rcu_release_all(advancer, 7);
    return 0;
}
