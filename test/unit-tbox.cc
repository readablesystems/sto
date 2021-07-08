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
    TBox<int> box;
    bool match;

    {
        TestTransaction t1(1);
        match = ib < 3;
        assert(match);
        box = 9; /* avoid read-only txn */

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
        box = 9; /* avoid read-only txn */

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
    TBox<int> box;
    f.nontrans_write(3);

    try {
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
        assert(false && "shouldn't get here");
        assert(x == 4);
        assert(!t1.try_commit());
    } catch (Transaction::Abort e) {
				TestTransaction::hard_reset();
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
    TBox<int, TNonopaqueWrapped<int> > box;
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

#if 0
void testStringWrapper() {
    TBox<std::string> f;

    GUARDED {
        f = "100";
    }

    assert(f.nontrans_read() == "100");

    GUARDED {
        std::string x("200");
        // normal assignment makes a copy
        f = x;
        assert(f.read() == "200");
        assert(f.nontrans_read() == "100");
        assert(x == "200");
    }

    assert(f.nontrans_read() == "200");

    GUARDED {
        std::string x("300");
        // move assignment does a move
        f = std::move(x);
        assert(f.read() == "300");
        assert(f.nontrans_read() == "200");
        if (x != "")
            std::cerr << "move assignment to string box did not appear to move\n"
                << "(although this might be compiler sensitive)\n";
        assert(x == "");
    }

    assert(f.nontrans_read() == "300");

    GUARDED {
        f = "400";
        assert(f.read().compare("400") == 0);
    }

    assert(f.nontrans_read() == "400");

    GUARDED {
        std::string x("500");
        f = x;
        assert(f.read().compare("500") == 0);
        assert(Sto::item(&f, 0).has_write());
        // stored write value has different address from assigned value
        assert(&Sto::item(&f, 0).write_value<std::string>() != &x);
        // in fact we can assign to x...
        x = "501";
        // and not change the write value
        assert(f.read().compare("500") == 0);
    }

    assert(f.nontrans_read() == "500");

    // NB: The usual `GUARDED` semantics are not good enough here.
    // We must commit `t` BEFORE `x` is destroyed, and in C++, destructors
    // are called in reverse declaration order.
    {
        TestTransaction t(1);
        std::string x("600");
        f = StringWrapper(x);
        assert(f.read().compare("600") == 0);
        assert(Sto::item(&f, 0).has_write());
        // thanks to StringWrapper, stored write value has same address
        // as local string
        assert(&Sto::item(&f, 0).write_value<std::string>() == &x);
        // in fact we can assign to x...
        x = "601";
        // and change the write value
        assert(f.read().compare("601") == 0);
        assert(t.try_commit());
        // in fact, thanks to std::move in TBox::install, successful commit
        // clears `x`
        if (x != "")
            std::cerr << "commit of StringWrapper did not appear to move\n"
                << "(although this might be compiler sensitive)\n";
        assert(x == "");
    }

    assert(f.nontrans_read() == "601");

    printf("PASS: %s\n", __FUNCTION__);
}
#endif

int main() {
    testSimpleInt();
    testSimpleString();
    testConcurrentInt();
    testOpacity1();
    testNoOpacity1();
    //testStringWrapper();

    std::thread advancer;  // empty thread because we have no advancer thread
    Transaction::rcu_release_all(advancer, 2);
    return 0;
}
