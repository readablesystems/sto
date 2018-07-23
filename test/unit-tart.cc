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

struct Element {
    const char* key;
    int len;
    uintptr_t val;
    TVersion vers;
    bool poisoned;
};

#define GUARDED if (TransactionGuard tguard{})

// TODO these cause simple 2 to fail
// const char* absentkey1 = "he";
// const char* absentkey2 = "hello";

const char* absentkey1 = "hello";
const char* absentkey2 = "1234";
const char* absentkey2_1 = "1245";
const char* absentkey2_2 = "1256";
const char* absentkey2_3 = "1267";
const char* absentkey2_4 = "1278";
const char* absentkey2_5 = "1289";

const char* checkkey = "check1";
const char* checkkey2 = "check2";
const char* checkkey3 = "check3";

void process_mem_usage(double& vm_usage, double& resident_set) {
    vm_usage     = 0.0;
    resident_set = 0.0;

    // the two fields we want
    unsigned long vsize;
    long rss;
    {
        std::string ignore;
        std::ifstream ifs("/proc/self/stat", std::ios_base::in);
        ifs >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore
                >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore >> ignore
                >> ignore >> ignore >> vsize >> rss;
    }

    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
    vm_usage = vsize / 1024.0;
    resident_set = rss * page_size_kb;
}

void NoChecks() {
    TART aTART;
    {
        TransactionGuard t;
        aTART.transPut(absentkey1, 10);
    }
    {
        TransactionGuard t;
        aTART.transGet(absentkey1);
    }

    {
        TransactionGuard t;
        aTART.transRemove(absentkey1);
    }

    {
        TransactionGuard t;
        aTART.transPut(absentkey1, 10);
        aTART.transGet(absentkey1);
        aTART.transRemove(absentkey1);
    }
        // Insert check print statement, no check should occur
}

void Checks() {
    TART aTART;
    {
        TransactionGuard t;
        aTART.transPut(absentkey1, 10);
    }
    printf("1. ");
    {
        TransactionGuard t;
        auto x = aTART.transGet(absentkey1);
        aTART.transPut(checkkey, 100);
        if(x == 0) {
            printf("wtf\n");
        }
    }
    printf("\n2.");
    {
        TransactionGuard t;
        aTART.transGet(absentkey1);
        aTART.transPut(absentkey2, 12);
    }
    printf("\n3.");
    {
        TransactionGuard t;
        aTART.transGet(absentkey1);
        aTART.transRemove(absentkey2);
    }
    printf("\n4.");
    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        aTART.transPut(checkkey, 100);

        if (x == 0) {
            printf("wtf\n");
        }
    }
    printf("\n");
    printf("PASS: %s\n", __FUNCTION__);

}

void testSimple() {
    TART a;

    const char* key1 = "hello world";
    const char* key2 = "1234";
    {
        TransactionGuard t;
        a.transPut(key1, 123);
        a.transPut(key2, 321);
    }

    {
        TransactionGuard t;
        volatile auto x = a.transGet(key1);
        volatile auto y = a.transGet(key2);
        assert(x == 123);
        assert(y == 321);
    }

    {
        TransactionGuard t;
        a.transPut("foo", 1);
        a.transPut("foobar", 2);
    }

    {
        TransactionGuard t;
        assert(a.transGet("foobar") == 2);
    }


    printf("PASS: %s\n", __FUNCTION__);
}

void testSimple2() {
    TART aTART;

    {
        TransactionGuard t;
        aTART.transPut(absentkey1, 10);
        aTART.transPut(absentkey2, 10);
    }

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    aTART.transPut(absentkey2, 123);

    TestTransaction t2(0);
    aTART.transPut(absentkey2, 456);

    assert(t2.try_commit());
    assert(t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey2);
        assert(x == 123);
    }
    printf("PASS: %s\n", __FUNCTION__);
}

void testSimpleErase() {
    TART a;

    const char* key1 = "hello world";
    const char* key2 = "1234";
    {
        TransactionGuard t;
        a.transPut(key1, 123);
        a.transPut(key2, 321);
    }

    {
        TransactionGuard t;
        volatile auto x = a.transGet(key1);
        volatile auto y = a.transGet(key2);
        a.transPut(checkkey, 100);
        assert(x == 123);
        assert(y == 321);
    }

    {
        TransactionGuard t;
        a.transRemove(key1);
        volatile auto x = a.transGet(key1);
        a.transPut(checkkey, 100);
        assert(x == 0);
    }

    {
        TransactionGuard t;
        volatile auto x = a.transGet(key1);
        assert(x == 0);
        a.transPut(key1, 567);
    }

    {
        TransactionGuard t;
        volatile auto x = a.transGet(key1);
        a.transPut(checkkey, 100);
        assert(x == 567);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testEmptyErase() {
    TART a;

    const char* key1 = "hello world";

    // deleting non-existent node
    {
        TransactionGuard t;
        a.transRemove(key1);
        volatile auto x = a.transGet(key1);
        a.transPut(checkkey, 100);
        assert(x == 0);
    }

    {
        TransactionGuard t;
        a.transRemove(key1);
        volatile auto x = a.transGet(key1);
        assert(x == 0);
        a.transPut(key1, 123);
        a.transRemove(key1);
        x = a.transGet(key1);
        assert(x == 0);    
    }

    printf("PASS: %s\n", __FUNCTION__);

}

void testAbsentErase() {
    TART a;

    TestTransaction t1(0);
    a.transRemove("foo");
    a.transPut("bar", 1);

    TestTransaction t2(1);
    a.transPut("foo", 123);
    assert(t2.try_commit());

    t1.use();
    assert(!t1.try_commit());
    printf("PASS: %s\n", __FUNCTION__);
}

void multiWrite() {
    TART aTART;
    {
        TransactionGuard t;
        aTART.transPut(absentkey2, 456);
    }

    {
        TransactionGuard t;
        aTART.transPut(absentkey2, 123);
    }
    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey2);
        assert(x == 123);    
    }
    printf("PASS: %s\n", __FUNCTION__);
}

void multiThreadWrites() {
    TART aTART;
    {
        TransactionGuard t;
        aTART.transPut(absentkey2, 456);
    }

    TestTransaction t1(0);
    aTART.transPut(absentkey2, 123);

    TestTransaction t2(0);
    aTART.transPut(absentkey2, 456);

    assert(t1.try_commit());
    assert(t2.try_commit());

    {
        TransactionGuard t;
        // printf("to transGet\n");
        volatile auto x = aTART.transGet(absentkey2);
        // printf("looked\n");
        assert(x == 456);
    }
    printf("PASS: %s\n", __FUNCTION__);

}

void testReadDelete() {
    TART aTART;
    TestTransaction t0(0);
    aTART.transPut(absentkey1, 10);
    aTART.transPut(absentkey2, 10);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    aTART.transPut(absentkey2, 10);

    TestTransaction t2(0);
    aTART.transRemove(absentkey1);

    assert(t2.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        volatile auto y = aTART.transGet(absentkey2);
        assert(x == 0);
        assert(y == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testReadWriteDelete() {
    TART aTART;
    TestTransaction t0(0);
    aTART.transPut(absentkey1, 10);
    aTART.transPut(absentkey2, 10);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    aTART.transPut(absentkey2, 123);

    TestTransaction t2(0);
    aTART.transRemove(absentkey1);

    assert(t2.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        volatile auto y = aTART.transGet(absentkey2);
        assert(x == 0);
        assert(y == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testReadDeleteInsert() {
    TART aTART;
    TestTransaction t0(0);
    aTART.transPut(absentkey1, 10);
    aTART.transPut(absentkey2, 10);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    aTART.transPut(absentkey2, 123);

    TestTransaction t2(0);
    aTART.transRemove(absentkey1);
    assert(t2.try_commit());

    TestTransaction t3(0);
    aTART.transPut(absentkey1, 10);
    assert(t3.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        volatile auto y = aTART.transGet(absentkey2);
        assert(x == 10);
        assert(y == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}


void testInsertDelete() {
    TART aTART;
    TestTransaction t0(0);
    aTART.transPut(absentkey1, 10);
    aTART.transPut(absentkey2, 10);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.transPut(absentkey1, 123);
    aTART.transPut(absentkey2, 456);

    TestTransaction t2(0);
    aTART.transRemove(absentkey1);
    assert(t2.try_commit());

    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        volatile auto y = aTART.transGet(absentkey2);
        assert(x == 0);
        assert(y == 10);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

// test that reading poisoned val aborts
void testAbsent1_1() {
    TART aTART;

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    // a new transPut
    TestTransaction t2(0);
    aTART.transPut(absentkey1, 456);
    aTART.transPut(absentkey2, 123);

    t1.use();
    try {
        aTART.transGet(absentkey2);
    } catch (Transaction::Abort e) { 
        assert(t2.try_commit());
        {
            TransactionGuard t;
            volatile auto x = aTART.transGet(absentkey1);
            assert(x == 456);
        }
        printf("PASS: %s\n", __FUNCTION__);
        return;
    }
    assert(false);


}

// test you can write to a key after absent reading it
void testAbsent1_2() {
    TART aTART;

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    aTART.transPut(absentkey1, 123);

    // a new transPut
    TestTransaction t2(0);
    try {
        aTART.transPut(absentkey1, 456);
    } catch (Transaction::Abort e) {}

    assert(t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        assert(x == 123);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

// test that absent read detects changes made by other threads
void testAbsent1_3() {
    TART aTART;

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    aTART.transPut(absentkey2, 123);

    // a new transPut
    TestTransaction t2(0);
    aTART.transPut(absentkey1, 456);

    assert(t2.try_commit());
    
    TestTransaction t3(0);
    aTART.transRemove(absentkey1);

    assert(t3.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        assert(x == 0);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

// 
void testAbsent2_2() {
    TART aTART;

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    aTART.transPut(absentkey1, 123);
    aTART.transGet(absentkey2);
    aTART.transPut(absentkey2, 123);

    // a new transPut
    TestTransaction t2(0);
    try {
        aTART.transPut(absentkey1, 456);
    } catch (Transaction::Abort e) {}

    assert(t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        assert(x == 123);
    }

    printf("PASS: %s\n", __FUNCTION__);

}

void testAbsent3() {
    TART aTART;

    TestTransaction t0(0);
    aTART.transPut(absentkey2, 123);
    aTART.transPut(absentkey2_1, 456);

    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.transGet(absentkey1);
    aTART.transPut(absentkey2, 123);

    // an update
    TestTransaction t2(0);
    aTART.transPut(absentkey2_2, 456);

    assert(t2.try_commit());
    assert(t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        volatile auto y = aTART.transGet(absentkey2);
        assert(y == 123);
        assert(x == 0);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testAbsent3_2() {
    TART aTART;

    TestTransaction t0(0);
    aTART.transPut(absentkey2, 123);
    aTART.transPut(absentkey2_1, 123);
    aTART.transPut(absentkey2_2, 123);
    aTART.transPut(absentkey2_3, 123);
    assert(t0.try_commit());

    TestTransaction t1(0);
    aTART.transGet(absentkey2_4);
    aTART.transPut(absentkey2, 123);

    // an update
    TestTransaction t2(0);
    aTART.transPut(absentkey2_5, 456);

    assert(t2.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        volatile auto y = aTART.transGet(absentkey2);
        assert(y == 123);
        assert(x == 0);
    }

    printf("PASS: %s\n", __FUNCTION__);
}

void testABA1() {
    TART aTART;
    {
        TransactionGuard t;
        aTART.transPut(absentkey2, 456);
    }

    TestTransaction t1(0);
    aTART.transGet(absentkey2);
    aTART.transPut(absentkey1, 123);

    TestTransaction t2(0);
    aTART.transRemove(absentkey2);
    assert(t2.try_commit());

    TestTransaction t3(0);
    aTART.transPut(absentkey2, 456);

    assert(t3.try_commit());
    assert(!t1.try_commit());


    {
        TransactionGuard t;
        volatile auto x = aTART.transGet(absentkey1);
        volatile auto y = aTART.transGet(absentkey2);
        assert(y == 456);
        assert(x == 0);
    }

    printf("PASS: %s\n", __FUNCTION__);

    // ABA1 should fail due to key2 value changing
}

void testMultiRead() {
    TART art;
    {
        TransactionGuard t;
        art.transPut("hello", 4);
    }

    TestTransaction t1(0);
    volatile auto x = art.transGet("hello");

    TestTransaction t2(1);
    volatile auto y = art.transGet("hello");
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
        art.transPut("hello", 4);
    }

    TestTransaction t1(0);
    art.transGet("hello");
    art.transPut("world", 1);

    TestTransaction t2(1);
    art.transPut("hello", 6);
    assert(t2.try_commit());
    assert(!t1.try_commit());

    printf("PASS: %s\n", __FUNCTION__);
}

void testPerNodeV() {
    TART art;
    {
        TransactionGuard t;
        art.transPut("x", 1);
        art.transPut("y", 2);
        art.transPut("z", 3);
    }

    {
        TransactionGuard t;
        volatile auto x = art.transGet("x");
        volatile auto y = art.transGet("y");
        volatile auto z = art.transGet("z");
        assert(x == 1);
        assert(y == 2);
        assert(z == 3);
    }

    TestTransaction t1(0);
    art.transGet("x");
    art.transPut("z", 13);

    TestTransaction t2(1);
    art.transPut("y", 12);

    assert(t2.try_commit());
    assert(t1.try_commit());

    {
        TransactionGuard t;
        volatile auto x = art.transGet("x");
        volatile auto y = art.transGet("y");
        volatile auto z = art.transGet("z");
        assert(x == 1);
        assert(y == 12);
        assert(z == 13);
    }
    printf("PASS: %s\n", __FUNCTION__);
}

void testLookupRange() {
    TART art;
    
    TestTransaction t1(0);
    uintptr_t* result = new uintptr_t[10];
    size_t resultsFound;
    art.lookupRange({"a", 2}, {"z", 2}, {"", 0}, result, 10, resultsFound);
    art.transPut("foo", 1);

    TestTransaction t2(1);
    art.transPut("b", 1);
    assert(t2.try_commit());

    assert(!t1.try_commit());
    printf("PASS: %s\n", __FUNCTION__);
}

void testLookupRangeSplit() {
    TART art;

    {
        TransactionGuard t;
        art.transPut("rail", 1);
    }
    
    TestTransaction t1(0);
    uintptr_t* result = new uintptr_t[10];
    size_t resultsFound;
    art.lookupRange({"a", 2}, {"z", 2}, {"", 0}, result, 10, resultsFound);
    art.transPut("foo", 1);

    TestTransaction t2(1);
    art.transPut("rain", 1);
    assert(t2.try_commit());

    assert(!t1.try_commit());
    printf("PASS: %s\n", __FUNCTION__);
}

void testLookupRangeUpdate() {
    TART art;

    {
        TransactionGuard t;
        art.transPut("rail", 1);
    }
    
    TestTransaction t1(0);
    uintptr_t* result = new uintptr_t[10];
    size_t resultsFound;
    art.lookupRange({"a", 2}, {"z", 2}, {"", 0}, result, 10, resultsFound);
    art.transPut("foo", 1);

    TestTransaction t2(1);
    art.transPut("rail", 2);
    assert(t2.try_commit());

    assert(!t1.try_commit());
    printf("PASS: %s\n", __FUNCTION__);
}

void testRCU(TART* a) {
    double vm_usage; double resident_set;
    process_mem_usage(vm_usage, resident_set);
    printf("Before transPut\n");
    printf("RSS: %f, VM: %f\n", resident_set, vm_usage);

    for (int i = 0; i < 1000000; i++) {
        TRANSACTION_E {
            a->transPut(i, i);
        } RETRY_E(true);
    }

    process_mem_usage(vm_usage, resident_set);
    printf("After transPut\n");
    printf("RSS: %f, VM: %f\n", resident_set, vm_usage);

    for (int i = 0; i < 1000000; i++) {
        TRANSACTION_E {
            a->transRemove(i);
        } RETRY_E(true);
    }

    process_mem_usage(vm_usage, resident_set);
    printf("After transRemove\n");
    printf("RSS: %f, VM: %f\n", resident_set, vm_usage);

    for (int i = 0; i < 1000000; i++) {
        TRANSACTION_E {
            a->transPut(i, i);
        } RETRY_E(true);
    }

    process_mem_usage(vm_usage, resident_set);
    printf("After re-transPut\n");
    printf("RSS: %f, VM: %f\n", resident_set, vm_usage);
}

int main() {
    pthread_t advancer;
    pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
    pthread_detach(advancer); 

    TART* a = new TART();

    uintptr_t* result = new uintptr_t[10];
    size_t resultsFound;
    {
        TransactionGuard t;
        a->transPut("romane", 1);
        a->transPut("romanus", 2);
        a->transPut("romulus", 3);
        a->transPut("rubens", 4);
        a->transPut("ruber", 5);
        a->transPut("rubicon", 6);
        a->transPut("rubicundus", 7);

        a->transRemove("romanus");

        bool success = a->lookupRange({"romane", 7}, {"ruber", 6}, {"", 0}, result, 10, resultsFound);
        printf("success: %d\n", success);
        for (int i = 0; i < resultsFound; i++) {
            printf("%d: %d\n", resultsFound, result[i]);
        }
    }

    // a->print();

    testSimple();
    testSimple2();
    testSimpleErase();
    testEmptyErase();
    testAbsentErase();
    multiWrite();
    multiThreadWrites();
    testReadDelete(); // problem w/ lacking implementation of transRemove
    testReadWriteDelete();
    testReadDeleteInsert();
    testAbsent1_1();
    testInsertDelete();
    testAbsent1_2();
    testAbsent1_3(); // ABA read transPut delete detection no longer exists
    testAbsent2_2();
    testAbsent3();
    testAbsent3_2();
    testABA1(); // ABA doesn't work
    testMultiRead();
    testReadWrite();
    testPerNodeV();
    testReadWrite();
    testLookupRange();
    testLookupRangeSplit();
    testLookupRangeUpdate();

    printf("TART tests pass\n");

    return 0;
}
