#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include "Transaction.hh"
#include "Vector.hh"

void testSimpleInt() {
    Vector<int> f;

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

    printf("PASS: testSimpleInt\n");
}

void testWriteNPushBack() {
    Vector<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    TestTransaction t2(1);
    f[0] = 4;

    TestTransaction t3(2);
    f.push_back(20); // This will resize the array
    assert(t3.try_commit());
    assert(t2.try_commit()); // This should not conflict with the write

    {
        TransactionGuard t4;
        assert(f[0] == 4); // Make sure that the value is actually updated
    }

    printf("PASS: testWriteNPushBack\n");
}

void testPushBack() {
    Vector<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    TestTransaction t2(1);
    f.push_back(4);
    
    TestTransaction t3(2);
    f.push_back(20);
    assert(t3.try_commit());
    assert(t2.try_commit()); // This should not conflict with the push_back

    {
        TransactionGuard t4;
        assert(f[0] == 10);
        assert(f[1] == 20);
        assert(f[2] == 4);
    }

    printf("PASS: testPushBack\n");
}


void testPushBackNRead() {
    Vector<int> f;

    {
        TransactionGuard t;
        f.push_back(10);
    }

    {
        TransactionGuard t2;
        f.push_back(4);
        assert(f[1] == 4);
    }

    printf("PASS: testPushBackNRead\n");
}

void testPushBackNRead1() {
    Vector<int> f;

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

    printf("PASS: testPushBackNRead1\n");
}

void testPushBackNRead2() {
    Vector<int> f;

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

    printf("PASS: testPushBackNRead2\n");
}


void testPushBackNWrite() {
    Vector<int> f;

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

    printf("PASS: testPushBackNRead\n");
}



void testSimpleString() {
    Vector<std::string> f;

    {
        TransactionGuard t;
        f.push_back("100");
    }

    {
	TransactionGuard t2;
	std::string f_read = f[0];
	assert(f_read.compare("100") == 0);
    }

    printf("PASS: testSimpleString\n");
}

void testIter() {
    std::vector<int> arr;
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        int x = rand();
        f.push_back(x);
        arr.push_back(x);
    }
    } RETRY(false);

    int max;
    TRANSACTION {
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY(false);
    
    assert(max == *(std::max_element(arr.begin(), arr.end())));
    printf("Max is %i\n", max);
    printf("PASS: vector max_element test\n");
    
}


void testConflictingIter() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false);

    TestTransaction t(1);
    std::max_element(f.begin(), f.end());
    
    TestTransaction t1(2);
    f[4] = 10;
    assert(t1.try_commit());
    assert(!t.try_commit());

    printf("PASS: conflicting vector max_element test\n");
    
}

void testModifyingIter() {
    Vector<int> f;
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

    printf("PASS: vector replace test\n");
}

void testConflictingModifyIter1() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false);
    
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

    printf("PASS: vector conflicting replace test1\n");
}

void testConflictingModifyIter2() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false);

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

    printf("PASS: vector conflicting replace test2\n");
}

void testConflictingModifyIter3() {
    Vector<int> f;
    TRANSACTION {
    for (int i = 0; i < 10; i++) {
        f.push_back(i);
    }
    } RETRY(false);
    
    TestTransaction t1(1);
    (int) f[4];
    
    TestTransaction t(2);
    std::replace(f.begin(), f.end(), 4, 6);
    assert(t.try_commit());
    assert(!t1.try_commit());

    {
        TransactionGuard t2;
        int v = f[4];
        assert(v == 6);
    }

    printf("PASS: vector conflicting replace test3\n");
}

void testIterNPushBack() {
    Vector<int> f;
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);
    
    int max;
    TRANSACTION {
        f.push_back(20);
        max = *(std::max_element(f.begin(), f.end()));
    } RETRY(false);

    assert(max == 20);
    printf("PASS: IterNPushBack\n");
}

void testIterNPushBack1() {
    Vector<int> f;
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
    
    printf("PASS: IterNPushBack1\n");    
}

void testIterNPushBack2() {
    Vector<int> f;

    {
        TransactionGuard tt;
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    }

    TestTransaction t1(1);
    std::max_element(f.begin(), f.end());
    
    TestTransaction t2(2);
    f.push_back(2);
    assert(t2.try_commit());
    assert(!t1.try_commit());
    
    printf("PASS: IterNPushBack2\n");
    
}


void testErase() {
    Vector<int> f;

    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);

    {
        TransactionGuard t1;
        f.erase(f.begin() + 5);
    }

    TRANSACTION {
        Vector<int>::iterator it = f.begin();
        Vector<int>::iterator eit = f.end();
        for (; it != eit; ++it) {
            printf("%i\n", (int)*it);
        }
    } RETRY(false);
    
    printf("PASS: testErase\n");
}

void testInsert() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);

    {
        TransactionGuard t1;
        f.insert(f.begin() + 5, 25);
    }

    TRANSACTION {
        Vector<int>::iterator it = f.begin();
        Vector<int>::iterator eit = f.end();
        for (; it != eit; ++it) {
            printf("%i\n", (int)*it);
        }
    } RETRY(false);
    
    printf("PASS: testInsert\n");
}


void testPushNPop() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);

    {
        TransactionGuard t;
        f.push_back(20);
        f.push_back(21);
        assert(f[0] == 0);
        assert(f.size() == 12);
        f.pop_back();
        f.pop_back();
    }
    
    TestTransaction t1(1);
    f.pop_back();
    f.push_back(15);
    
    TestTransaction t2(2);
    f.push_back(20);
    assert(t2.try_commit());
    assert(!t1.try_commit());
    
    TRANSACTION {
        assert(f.size() == 11);
        assert(f[10] == 20);
    } RETRY(false);
    
    
    printf("PASS: testPushNPop\n");
    
    TestTransaction t3(1);
    f.pop_back();
    f.push_back(15);
    
    TestTransaction t4(2);
    f.pop_back();
    assert(t4.try_commit());
    assert(!t3.try_commit());
    
    TRANSACTION {
        assert(f.size() == 10);
    } RETRY(false);

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
    
    assert(t5.try_commit());
    assert(!t7.try_commit());

    TRANSACTION {
        assert(f.size() == 9);
        assert(f[8] == 15);
    } RETRY(false);

    printf("PASS: testPushNPop2\n");
}

void testPopAndUdpate() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);
    
    TestTransaction t1(1);
    f[9] = 20;

    TestTransaction t2(2);
    f.pop_back();
    f.pop_back();
    assert(t2.try_commit());
    assert(!t1.try_commit());
}

void testMulPushPops() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);

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
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);

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
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);

    {
        TransactionGuard t1;
        f[9] = 20;
        f.pop_back();
    }
}

void testIteratorBetterSemantics() {
    Vector<int> f;
    
    TRANSACTION {
        for (int i = 0; i < 10; i++) {
            f.push_back(i);
        }
    } RETRY(false);

    TestTransaction t1(1);
    // XXX std::find uses a special case for random-access iterators
    // that calls size()
    Vector<int>::iterator it;
    for (it = f.begin(); it != f.end(); ++it)
        if (*it == 5)
            break;

    TestTransaction t2(2);
    f.push_back(12);
    assert(t2.try_commit());
    t1.use();
    int x = *it;
    assert(t1.try_commit());
    assert(x == 5);

    printf("PASS: IteratorBetterSemantics\n");

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
    return 0;
}
