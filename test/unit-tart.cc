#undef NDEBUG
#include <string>
#include <thread>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TART.hh"
#include "Transaction.hh"
#include <unistd.h>
#include "unit-tart-threads.hh"

#define GUARDED if (TransactionGuard tguard{})

void testSimple() {
    TART a;

    std::string key1 = "hello world";
    std::string key2 = "1234";
    {
        TransactionGuard t;
        a.insert(key1, 123);
        a.insert(key2, 321);
    }

    {
        TransactionGuard t;
        auto x = a.lookup(key1);
        auto y = a.lookup(key2);
        assert(x == 123);
        assert(y == 321);
    }

    {
        TransactionGuard t;
        a.erase(key1);
        auto x = a.lookup(key1);
        assert(x == 0);
    }

    {
        TransactionGuard t;
        auto x = a.lookup(key1);
        assert(x == 0);
        a.insert(key1, 567);
    }

    {
        TransactionGuard t;
        auto x = a.lookup(key1);
        assert(x == 567);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testErase() {
    TART a;

    std::string key1 = "hello world";

    // deleting non-existent node
    {
        TransactionGuard t;
        a.erase(key1);
        auto x = a.lookup(key1);
        assert(x == 0);
    }

    {
        TransactionGuard t;
        a.erase(key1);
        auto x = a.lookup(key1);
        assert(x == 0);
        a.insert(key1, 123);
        a.erase(key1);
        x = a.lookup(key1);
        assert(x == 0);    
    }

    printf("PASS: %s\n", __FUNCTION__);

}

void multiWrite() {
    {
        TransactionGuard t;
        aTART.insert(absentkey2, 456);
    }

    {
        TransactionGuard t;
        aTART.insert(absentkey2, 123);
    }
    {
        TransactionGuard t;
        auto x = aTART.lookup(absentkey2);
        assert(x == 123);    
    }
    printf("PASS: %s\n", __FUNCTION__);
}

void ThreadWrites1() {
    std::thread thr1 = std::thread(DelayWriteK2);
    std::thread thr2 = std::thread(WriteK2);
    printf("to join thr2\n");
    thr2.join();
    printf("to join thr1\n");
    thr1.join();
    printf("joined\n");


    {
        TransactionGuard t;
        printf("to lookup\n");
        auto x = aTART.lookup(absentkey2);
        printf("looked\n");
        assert(x == 123);
    }
    printf("PASS: %s\n", __FUNCTION__);

}


// optimization question: if a write re-writes the same value, should an install occur
void ABA() {
    printf("HI IM ABA\n");
    {
        TransactionGuard t;
        aTART.insert(absentkey2, 456);
    }

    std::thread thr1 = std::thread(ABA1);
    std::thread thr2 = std::thread(ABA2);
    std::thread thr3 = std::thread(ABA3);
    thr2.join();
    thr3.join();
    thr1.join();
    printf("joined\n");

    try {
        TransactionGuard t;
        auto x = aTART.lookup(absentkey2);
        auto y = aTART.lookup(absentkey1);
        assert(x == 456);
        assert(y == 0);
    } catch (Transaction::Abort e) {
        printf("main aba aborted\n");
    }

    printf("PASS: %s\n", __FUNCTION__);

    // ABA1 should fail due to key2 value changing
}

void Absent1() {
    // {
    //     TransactionGuard t;
    //     aTART.insert(absentkey2, 456)
    // }

    // std::thread thr1 = std::thread(ABA1);
    // std::thread thr3 = std::thread(ABA3);
    // thr2.join();
    // thr3.join();
    // thr1.join();


    // auto x = aTART.lookup(absentkey2);
    // auto y = aTART.lookup(absentkey1);
    // assert(x == 456);
    // assert(y == 0);
}

// will not work with a tree v number, will work with node vnumber
// absent read of k1 will check tree v number
void Absent2() {
    std::thread thr1 = std::thread(ReadK1WriteK2);
    usleep(10);
    std::thread thr2 = std::thread(WriteK2);
    {
        TransactionGuard t;
        auto x = aTART.lookup(absentkey2);
        assert(x == 123);
    }
}

void testReadRead() {
    TART art;
    {
        TransactionGuard t;
        art.insert("hello", 4);
    }

    TestTransaction t1(0);
    auto x = art.lookup("hello");

    TestTransaction t2(1);
    auto y = art.lookup("hello");
    assert(t2.try_commit());

    t1.use();
    assert(t1.try_commit());
    assert(x == y);
    assert(x == 4);
    printf("PASS: %s\n", __FUNCTION__);
}

void testReadWrite() {
    TART art;
    {
        TransactionGuard t;
        art.insert("hello", 4);
    }

    TestTransaction t1(0);
    auto x = art.lookup("hello");
    art.insert("world", 1);

    TestTransaction t2(1);
    art.insert("hello", 6);
    assert(t2.try_commit());
    assert(!t1.try_commit());

    printf("PASS: %s\n", __FUNCTION__);
}

void testPerNodeV() {
    TART art;
    {
        TransactionGuard t;
        art.insert("x", 1);
        art.insert("y", 2);
        art.insert("z", 3);
    }

    {
        TransactionGuard t;
        auto x = art.lookup("x");
        auto y = art.lookup("y");
        auto z = art.lookup("z");
        assert(x == 1);
        assert(y == 2);
        assert(z == 3);
    }

    TestTransaction t1(0);
    auto x = art.lookup("x");
    art.insert("z", 13);

    TestTransaction t2(1);
    art.insert("y", 12);
    assert(t2.try_commit());
    assert(t1.try_commit());

    {
        TransactionGuard t;
        auto x = art.lookup("x");
        auto y = art.lookup("y");
        auto z = art.lookup("z");
        assert(x == 1);
        assert(y == 12);
        assert(z == 13);
    }
}

int main() {
    aTART = TART();

    testReadWrite();
    testReadRead();

    testSimple();
    testErase();
    multiWrite();
    CleanATART();
    ThreadWrites1();
    CleanATART();
    ABA();
    CleanATART();
    testPerNodeV();

    printf("TART tests pass\n");

    return 0;
}
